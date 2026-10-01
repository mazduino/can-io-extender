#include "ConfigLink.h"
#include "Calibration.h"
#include "Inputs.h"
#include "Outputs.h"
#include "CanLink.h"

namespace {
#ifndef CONFIG_LINK_BAUD
#define CONFIG_LINK_BAUD 9600
#endif

char gLine[96];
uint8_t gLen;

const char* findValue(const char* json, const char* key) {
  char pat[16];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char* p = strstr(json, pat);
  if (!p) return nullptr;
  p += strlen(pat);
  while (*p == ' ') p++;
  if (*p != ':') return nullptr;
  p++;
  while (*p == ' ') p++;
  return p;
}

bool intField(const char* json, const char* key, long *out) {
  const char* p = findValue(json, key);
  if (!p) return false;
  char* end = nullptr;
  const long v = strtol(p, &end, 10);
  if (end == p) return false;
  *out = v;
  return true;
}

bool strFieldEquals(const char* json, const char* key, const char* expect) {
  const char* p = findValue(json, key);
  if (!p || *p != '"') return false;
  p++;
  const size_t n = strlen(expect);
  return strncmp(p, expect, n) == 0 && p[n] == '"';
}

void reply(const char* body) {
  Serial2.print('{');
  Serial2.print(body);
  Serial2.print("}\n");
}

void replyStatus(const char* cmd, bool ok) {
  char buf[64];
  snprintf(buf, sizeof(buf), "\"r\":\"%s\",\"ok\":%s", cmd, ok ? "true" : "false");
  reply(buf);
}

void sendInfo() {
  char buf[128];
  snprintf(buf, sizeof(buf),
           "\"r\":\"info\",\"dev\":\"mazduino-iox\",\"fw\":%d,\"node\":%d,"
           "\"base\":%u,\"ch\":%d,\"slots\":%d,\"bps\":%lu",
           FW_VERSION, NODE_ID, (unsigned)CAN_BASE_ID, ANALOG_CHANNEL_COUNT,
           CAL_SLOT_COUNT, (unsigned long)CanLink::lockedBitrate());
  reply(buf);
}

void sendLive() {
  Serial2.print("{\"r\":\"live\",\"mv\":[");
  for (uint8_t i = 0; i < ANALOG_CHANNEL_COUNT; i++) {
    if (i) Serial2.print(',');
    Serial2.print(Inputs::analogMv(i));
  }
  Serial2.print("],\"vb\":");
  Serial2.print(Inputs::batteryMv());
  Serial2.print(",\"in\":");
  Serial2.print(Inputs::digitalBits());
  Serial2.print(",\"cal\":[");
  for (uint8_t s = 0; s < CAL_SLOT_COUNT; s++) {
    if (s) Serial2.print(',');
    Serial2.print(calibrationValue(s));
  }
  Serial2.print("]}\n");
}

void sendSlot(uint8_t slot) {
  const CalSlot* s = calibrationSlot(slot);
  if (!s) { replyStatus("cal_get", false); return; }

  Serial2.print("{\"r\":\"cal\",\"s\":");
  Serial2.print(slot);
  Serial2.print(",\"src\":");
  Serial2.print(s->source);
  Serial2.print(",\"pts\":[");
  for (uint8_t i = 0; i < s->pointCount; i++) {
    if (i) Serial2.print(',');
    Serial2.print('[');
    Serial2.print(s->points[i].mv);
    Serial2.print(',');
    Serial2.print(s->points[i].value);
    Serial2.print(']');
  }
  Serial2.print("]}\n");
}

void handle(const char* json) {
  long slot = 0, v = 0;

  if (strFieldEquals(json, "cmd", "info"))  { sendInfo(); return; }
  if (strFieldEquals(json, "cmd", "live"))  { sendLive(); return; }

  if (strFieldEquals(json, "cmd", "cal_get")) {
    intField(json, "s", &slot);
    sendSlot((uint8_t)slot);
    return;
  }
  if (strFieldEquals(json, "cmd", "cal_src")) {
    long ch = CAL_SOURCE_NONE;
    intField(json, "s", &slot);
    intField(json, "ch", &ch);
    replyStatus("cal_src", calibrationSetSource((uint8_t)slot, (uint8_t)ch));
    return;
  }
  if (strFieldEquals(json, "cmd", "cal_pt")) {
    long mv = 0;
    intField(json, "s", &slot);
    if (!intField(json, "mv", &mv) || !intField(json, "v", &v)) {
      replyStatus("cal_pt", false);
      return;
    }
    replyStatus("cal_pt",
                calibrationSetPoint((uint8_t)slot, (uint16_t)mv, (int16_t)v));
    return;
  }
  if (strFieldEquals(json, "cmd", "cal_del")) {
    intField(json, "s", &slot);
    replyStatus("cal_del", calibrationClearPoints((uint8_t)slot));
    return;
  }
  if (strFieldEquals(json, "cmd", "cal_save")) {
    calibrationSave();
    replyStatus("cal_save", true);
    return;
  }
  if (strFieldEquals(json, "cmd", "cal_reset")) {
    calibrationReset();
    calibrationSave();
    replyStatus("cal_reset", true);
    return;
  }
  replyStatus("?", false);
}
}

namespace ConfigLink {
void begin() {
  Serial2.begin(CONFIG_LINK_BAUD);
  gLen = 0;
}

void update() {
  while (Serial2.available()) {
    const char c = (char)Serial2.read();
    if (c == '\n' || c == '\r') {
      if (gLen) {
        gLine[gLen] = '\0';
        handle(gLine);
        gLen = 0;
      }
      continue;
    }
    if (gLen < sizeof(gLine) - 1) {
      gLine[gLen++] = c;
    } else {
      gLen = 0;
    }
  }
}
}
