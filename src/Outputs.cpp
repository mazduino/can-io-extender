#include "Outputs.h"

namespace {
const uint8_t kSimplePins[7] = {PIN_LC1, PIN_LC2, PIN_LC3, PIN_LC4,
                                PIN_LC5, PIN_LS1, PIN_LS2};

uint8_t  gBits0, gBits1;
uint8_t  gHs1Duty, gHs2Duty;
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
  gHs1Duty = gHs2Duty = 0;
  gFailsafe = true;
}

void applyCommand(uint8_t bits0, uint8_t bits1, uint8_t hs1Duty, uint8_t hs2Duty) {
  gLastCommandMs = millis();
  gFailsafe = false;

  gBits0 = bits0 & 0x7F;
  gBits1 = bits1 & 0x0F;
  gHs1Duty = hs1Duty;
  gHs2Duty = hs2Duty;

  driveSimple(gBits0);
  driveHighSide(gBits1, gHs1Duty, gHs2Duty);
  driveLogic(gBits1);
}

void update() {
  if (!gFailsafe && (millis() - gLastCommandMs >= OUTPUT_TIMEOUT_MS)) {
    allOff();
    gFailsafe = true;
    return;
  }
}

void allOff() {
  gBits0 = 0;
  gBits1 = 0;
  gHs1Duty = gHs2Duty = 0;
  driveSimple(0);
  driveHighSide(0, 0, 0);
  digitalWrite(PIN_IGN1, LOW);
  digitalWrite(PIN_IGN2, LOW);
}

uint8_t stateBits0() { return gBits0; }

uint8_t stateBits1() { return gBits1; }

bool inFailsafe()    { return gFailsafe; }
}
