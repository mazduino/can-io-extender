#include "Outputs.h"
#include "OutputRules.h"

namespace {
const uint8_t kSimplePins[7] = {PIN_LC1, PIN_LC2, PIN_LC3, PIN_LC4,
                                PIN_LC5, PIN_LS1, PIN_LS2};

uint8_t  gBits0, gBits1;
uint8_t  gCanBits0, gCanBits1;
uint8_t  gCanHs1Duty, gCanHs2Duty;
uint32_t gLastCommandMs;
bool     gFailsafe = true;

void driveSimple(uint8_t bits0) {
  for (uint8_t i = 0; i < 7; i++) {
    digitalWrite(kSimplePins[i], (bits0 & (1 << i)) ? HIGH : LOW);
  }
}

void driveHighSide(uint8_t bits1, uint8_t hs1Duty, uint8_t hs2Duty) {
  if (bits1 & OUT_BIT_HS1) {
    if (hs1Duty == 0 || hs1Duty >= 255) digitalWrite(PIN_HS1, HIGH);
    else                                analogWrite(PIN_HS1, hs1Duty);
  } else {
    digitalWrite(PIN_HS1, LOW);
  }

  if (bits1 & OUT_BIT_HS2) {
    if (hs2Duty == 0 || hs2Duty >= 255) digitalWrite(PIN_HS2, HIGH);
    else                                analogWrite(PIN_HS2, hs2Duty);
  } else {
    digitalWrite(PIN_HS2, LOW);
  }
}

void driveLogic(uint8_t bits1) {
  digitalWrite(PIN_IGN1, (bits1 & OUT_BIT_IGN1) ? HIGH : LOW);
  digitalWrite(PIN_IGN2, (bits1 & OUT_BIT_IGN2) ? HIGH : LOW);
}
bool outBit(uint8_t b0, uint8_t b1, uint8_t out) {
  return out < 7 ? (b0 >> out) & 1 : (b1 >> (out - 7)) & 1;
}

void compose() {
  uint8_t b0 = 0, b1 = 0;
  uint8_t d1 = 0, d2 = 0;
  const bool test = testActive();
  for (uint8_t i = 0; i < OUT_COUNT; i++) {
    bool on;
    bool canOn = false;
    if (test) {
      on = testOutput(i);
    } else {
      canOn = !gFailsafe && outBit(gCanBits0, gCanBits1, i);
      const uint8_t mode = rulesMode(i);
      const bool ruleOn = rulesActive(i);
      on = mode == OUT_MODE_CAN  ? canOn
         : mode == OUT_MODE_RULE ? ruleOn
         : mode == OUT_MODE_CAN_AND ? (canOn && ruleOn)
         : (canOn || ruleOn);
    }
    if (!on) continue;
    if (i < 7) b0 |= (1 << i);
    else b1 |= (1 << (i - 7));
    if (i == 7 || i == 8) {
      const uint8_t duty = test ? testDuty(i - 7)
                         : canOn ? (i == 7 ? gCanHs1Duty : gCanHs2Duty)
                         : rulesDuty(i);
      if (i == 7) d1 = duty; else d2 = duty;
    }
  }
  gBits0 = b0;
  gBits1 = b1;
  driveSimple(b0);
  driveHighSide(b1, d1, d2);
  driveLogic(b1);
}
}

namespace Outputs {
void begin() {
  for (uint8_t i = 0; i < 7; i++) {
    pinMode(kSimplePins[i], OUTPUT);
    digitalWrite(kSimplePins[i], LOW);
  }
  pinMode(PIN_HS1, OUTPUT);  digitalWrite(PIN_HS1, LOW);
  pinMode(PIN_HS2, OUTPUT);  digitalWrite(PIN_HS2, LOW);
  pinMode(PIN_IGN1, OUTPUT); digitalWrite(PIN_IGN1, LOW);
  pinMode(PIN_IGN2, OUTPUT); digitalWrite(PIN_IGN2, LOW);

  gBits0 = gBits1 = 0;
  gCanBits0 = gCanBits1 = 0;
  gCanHs1Duty = gCanHs2Duty = 0;
  gFailsafe = true;
}

void applyCommand(uint8_t bits0, uint8_t bits1, uint8_t hs1Duty, uint8_t hs2Duty) {
  gLastCommandMs = millis();
  gFailsafe = false;
  gCanBits0 = bits0 & 0x7F;
  gCanBits1 = bits1 & 0x0F;
  gCanHs1Duty = hs1Duty;
  gCanHs2Duty = hs2Duty;
  compose();
}

void update() {
  const uint32_t now = millis();
  if (!gFailsafe && (now - gLastCommandMs >= OUTPUT_TIMEOUT_MS)) {
    gFailsafe = true;
    gCanBits0 = gCanBits1 = 0;
  }
  rulesUpdate(now);
  compose();
}

void allOff() {
  gCanBits0 = gCanBits1 = 0;
  gCanHs1Duty = gCanHs2Duty = 0;
  gFailsafe = true;
  testStop();
  compose();
}

uint8_t stateBits0() { return gBits0; }

uint8_t stateBits1() { return gBits1; }

bool inFailsafe()    { return gFailsafe; }
}
