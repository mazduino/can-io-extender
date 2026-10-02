#include "TsLink.h"
#include "Config.h"
#include "Calibration.h"
#include "Inputs.h"
#include "Outputs.h"
#include "CanLink.h"

namespace {
const uint16_t kCommandTimeoutMs = 500;

char gCmd;
uint8_t gHeader[6];
uint8_t gHeaderLen;
uint8_t gHeaderNeed;
uint16_t gOffset;
uint16_t gRemaining;
uint32_t gLastRxMs;
uint32_t gBootMs;

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
  d[34] = (Outputs::inFailsafe() ? 0x01 : 0) | (CanLink::isLocked() ? 0x02 : 0);
  d[35] = (uint8_t)((FW_VERSION << 4) | (NODE_ID & 0x0F));
  for (uint8_t s = 0; s < CAL_SLOT_COUNT; s++) {
    putU16(&d[36 + s * 2], (uint16_t)calibrationValue(s));
    putU16(&d[44 + s * 2], calibrationSourceMv(s));
  }
  const uint32_t up = (millis() - gBootMs) / 1000UL;
  putU16(&d[52], up > 0xFFFF ? 0xFFFF : (uint16_t)up);
  putU16(&d[54], (uint16_t)(CanLink::lockedBitrate() / 1000UL));

  Serial.write(d, sizeof(d));
}

void sendVersion() {
  Serial.print(F("Mazduino CAN IO Extender fw"));
  Serial.print(FW_VERSION);
  Serial.print(F(" node"));
  Serial.print(NODE_ID);
}

void idle() {
  gCmd = 0;
  gHeaderLen = 0;
  gHeaderNeed = 0;
  gRemaining = 0;
}

void startCommand(char c) {
  switch (c) {
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

void headerComplete() {
  const bool ourPage = gHeader[1] == TS_PAGE_ID;

  if (gCmd == 'b') {
    if (ourPage) calibrationSave();
    idle();
    return;
  }

  gOffset = u16At(&gHeader[2]);
  const uint16_t count = u16At(&gHeader[4]);

  if (gCmd == 'p') {
    for (uint16_t i = 0; i < count; i++) {
      Serial.write(ourPage ? calibrationPageRead(gOffset + i) : (uint8_t)0);
    }
    idle();
    return;
  }

  gRemaining = count;
  if (!ourPage) gOffset = 0xFFFF;
  if (gRemaining == 0) idle();
}

void consume(uint8_t b) {
  if (gHeaderLen < gHeaderNeed) {
    gHeader[gHeaderLen++] = b;
    if (gHeaderLen == gHeaderNeed) headerComplete();
    return;
  }
  if (gOffset != 0xFFFF) calibrationPageWrite(gOffset++, b);
  if (--gRemaining == 0) idle();
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

  while (Serial.available() > 0) {
    const uint8_t b = (uint8_t)Serial.read();
    gLastRxMs = millis();
    if (gCmd) consume(b);
    else startCommand((char)b);
  }
}

uint32_t lastActivityMs() {
  return gLastRxMs;
}
}
