#include "TsLink.h"
#include "Config.h"
#include "Calibration.h"
#include "Inputs.h"
#include "Outputs.h"
#include "CanLink.h"
#include "Settings.h"
#include "OutputRules.h"
#include "CanMonitor.h"

namespace {
const uint16_t kCommandTimeoutMs = 500;
const uint16_t kSniffArgTimeoutMs = 300;

char gCmd;
uint8_t gHeader[6];
uint8_t gHeaderLen;
uint8_t gHeaderNeed;
uint16_t gOffset;
uint16_t gRemaining;
uint32_t gLastRxMs;
uint32_t gBootMs;
bool gSeen;
uint8_t gPage;
uint8_t gBitrateBefore;
bool gSniffArg;
uint32_t gSniffId;
uint8_t gSniffDigits;

uint8_t pageRead(uint8_t page, uint16_t offset) {
  if (page == TS_PAGE_CAL) return calibrationPageRead(offset);
  if (page == TS_PAGE_SETTINGS) return settingsPageRead(offset);
  if (page == TS_PAGE_RULES) return rulesPageRead(offset);
  if (page == TS_PAGE_TEST) return testPageRead(offset);
  return 0;
}

void pageWrite(uint8_t page, uint16_t offset, uint8_t value) {
  if (page == TS_PAGE_CAL) calibrationPageWrite(offset, value);
  else if (page == TS_PAGE_SETTINGS) settingsPageWrite(offset, value);
  else if (page == TS_PAGE_RULES) rulesPageWrite(offset, value);
  else if (page == TS_PAGE_TEST) testPageWrite(offset, value);
}

inline void putU16(uint8_t* b, uint16_t v) {
  b[0] = (uint8_t)(v & 0xFF);
  b[1] = (uint8_t)(v >> 8);
}

inline uint16_t u16At(const uint8_t* b) {
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

void sendOutputChannels() {
  uint8_t d[TS_OCH_SIZE];
  memset(d, 0, sizeof(d));

  putU16(&d[0], Inputs::batteryMv());
  for (uint8_t i = 1; i <= 10; i++) putU16(&d[i * 2], Inputs::analogMv(i));
  for (uint8_t i = 0; i < 4; i++) putU16(&d[22 + i * 2], Inputs::hallDeciHz(i));
  d[30] = Inputs::digitalBits();
  d[31] = Inputs::diagBits();
  d[32] = Outputs::stateBits0();
  d[33] = Outputs::stateBits1();
  d[34] = (Outputs::inFailsafe() ? 0x01 : 0) | (CanLink::isLocked() ? 0x02 : 0) |
          (MCP_SUPPORTS_1MBPS ? 0x04 : 0) | (CanLink::controllerPresent() ? 0x08 : 0) |
          (CanLink::fixedBitrate() ? 0x10 : 0);
  d[35] = (uint8_t)((FW_VERSION << 4) | settingsNode());
  for (uint8_t s = 0; s < CAL_SLOT_COUNT; s++) putU16(&d[36 + s * 2], (uint16_t)calibrationValue(s));
  const uint32_t up = (millis() - gBootMs) / 1000UL;
  putU16(&d[56], up > 0xFFFF ? 0xFFFF : (uint16_t)up);
  putU16(&d[58], (uint16_t)(CanLink::lockedBitrate() / 1000UL));
  putU16(&d[60], CanLink::rxPerSecond());
  putU16(&d[62], CanLink::txPerSecond());
  d[64] = CanLink::rxErrorCount();
  d[65] = CanLink::txErrorCount();
  putU16(&d[66], CanLink::txFailures());
  for (uint8_t i = 0; i < 4; i++) putU16(&d[68 + i * 2], Inputs::hallValue(i));
  for (uint8_t r = 0; r < CANMON_TOP; r++) {
    uint32_t id;
    uint16_t rate;
    CanMonitor::top(r, id, rate);
    id &= 0x1FFFFFFFUL;
    putU16(&d[76 + r * 6], (uint16_t)(id & 0xFFFF));
    putU16(&d[78 + r * 6], (uint16_t)(id >> 16));
    putU16(&d[80 + r * 6], rate);
  }
  uint16_t wRate;
  uint8_t wDlc;
  CanMonitor::watched(settingsMonitorId(), wRate, wDlc, &d[115]);
  putU16(&d[112], wRate);
  d[114] = wDlc;

  Serial.write(d, sizeof(d));
}

void sendVersion() {
  Serial.print(F("Mazduino CAN IO Extender fw"));
  Serial.print(FW_VERSION);
  Serial.print(F(" node"));
  Serial.print(settingsNode());
}

void idle() {
  gCmd = 0;
  gHeaderLen = 0;
  gHeaderNeed = 0;
  gRemaining = 0;
}

int8_t hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

void sniffFinish() {
  gSniffArg = false;
  CanMonitor::consoleToggle(gSniffDigits ? gSniffId : CANMON_ALL);
}

bool sniffArg(char c) {
  const int8_t h = hexDigit(c);
  if (h >= 0 && gSniffDigits < 8) {
    gSniffId = (gSniffId << 4) | (uint8_t)h;
    gSniffDigits++;
    return true;
  }
  if (c == 'x' || c == 'X') return true;
  sniffFinish();
  return c == '\r' || c == '\n' || c == ' ';
}

void startCommand(char c) {
  if (c && strchr("QSCApMb", c)) {
    gSeen = true;
    CanMonitor::consoleStop();
  }
  switch (c) {
    case '!': gSniffArg = true; gSniffId = 0; gSniffDigits = 0; break;
    case 'Q': Serial.print(F(TS_SIGNATURE)); break;
    case 'S': sendVersion(); break;
    case 'C': Serial.write((uint8_t)1); break;
    case 'A': sendOutputChannels(); break;
    case 'p':
    case 'M': gCmd = c; gHeaderNeed = 6; break;
    case 'b': gCmd = c; gHeaderNeed = 2; break;
    default: break;
  }
}

void writeDone() {
  if (gPage == TS_PAGE_SETTINGS && settingsBitrate() != gBitrateBefore) {
    CanLink::restart();
  }
  idle();
}

void headerComplete() {
  gPage = gHeader[1];

  if (gCmd == 'b') {
    if (gPage == TS_PAGE_CAL) calibrationSave();
    else if (gPage == TS_PAGE_SETTINGS) settingsSave();
    else if (gPage == TS_PAGE_RULES) rulesSave();
    idle();
    return;
  }

  gOffset = u16At(&gHeader[2]);
  const uint16_t count = u16At(&gHeader[4]);

  if (gCmd == 'p') {
    for (uint16_t i = 0; i < count; i++) Serial.write(pageRead(gPage, gOffset + i));
    idle();
    return;
  }

  gBitrateBefore = settingsBitrate();
  gRemaining = count;
  if (gRemaining == 0) idle();
}

void consume(uint8_t b) {
  if (gHeaderLen < gHeaderNeed) {
    gHeader[gHeaderLen++] = b;
    if (gHeaderLen == gHeaderNeed) headerComplete();
    return;
  }
  pageWrite(gPage, gOffset++, b);
  if (--gRemaining == 0) writeDone();
}
}

namespace TsLink {
void begin() {
  idle();
  gBootMs = millis();
  gLastRxMs = 0;
}

void update() {
  if (gCmd && millis() - gLastRxMs > kCommandTimeoutMs) idle();
  if (gSniffArg && millis() - gLastRxMs > kSniffArgTimeoutMs) sniffFinish();

  while (Serial.available() > 0) {
    const uint8_t b = (uint8_t)Serial.read();
    gLastRxMs = millis();
    if (gCmd) consume(b);
    else if (gSniffArg && sniffArg((char)b)) continue;
    else startCommand((char)b);
  }
}

uint32_t lastActivityMs() {
  return gLastRxMs;
}

bool seen() {
  return gSeen;
}
}
