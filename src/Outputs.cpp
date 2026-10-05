#include "Outputs.h"
#include "OutputRules.h"
#include "Settings.h"

namespace {
const uint8_t kLcPins[5] = {PIN_LC1, PIN_LC2, PIN_LC3, PIN_LC4, PIN_LC5};
const uint8_t kLsPortBit[2] = {_BV(2), _BV(1)};
const uint32_t kLsTicksPerSec = 250000;
const uint16_t kLsMinPulse = 25;

uint8_t  gBits0, gBits1;
uint8_t  gCanBits0, gCanBits1;
uint8_t  gCanHs1Duty, gCanHs2Duty;
uint8_t  gCanLsDuty[2];
uint32_t gLastCommandMs;
bool     gFailsafe = true;

uint16_t gHsHz;
uint16_t gHsTop;

struct SoftPwm {
  volatile uint16_t on;
  volatile uint16_t off;
  volatile bool high;
  bool running;
};
SoftPwm gLs[2];

void hsTimerConfig(uint16_t hz) {
  if (hz == gHsHz) return;
  gHsHz = hz;
  static const uint16_t kDiv[5] = {1, 8, 64, 256, 1024};
  uint8_t cs = 1;
  uint32_t top = F_CPU / hz - 1;
  while (top > 0xFFFF && cs < 5) {
    top = F_CPU / ((uint32_t)kDiv[cs] * hz) - 1;
    cs++;
  }
  gHsTop = (uint16_t)min(top, 0xFFFFUL);
  const uint8_t com = TCCR5A & (_BV(COM5A1) | _BV(COM5B1));
  TCCR5B = 0;
  TCCR5A = com | _BV(WGM51);
  ICR5 = gHsTop;
  TCNT5 = 0;
  TCCR5B = _BV(WGM53) | _BV(WGM52) | cs;
}

void hsWrite(uint8_t pin, volatile uint16_t &ocr, uint8_t comBit, bool on, uint8_t duty) {
  if (!on || duty == 0 || duty >= 255) {
    TCCR5A &= ~comBit;
    digitalWrite(pin, on ? HIGH : LOW);
    return;
  }
  ocr = (uint16_t)(((uint32_t)gHsTop + 1) * duty / 255);
  TCCR5A |= comBit;
}

void driveHighSide(uint8_t bits1, uint8_t hs1Duty, uint8_t hs2Duty) {
  hsTimerConfig(settingsHsPwmHz());
  hsWrite(PIN_HS1, OCR5B, _BV(COM5B1), bits1 & OUT_BIT_HS1, hs1Duty);
  hsWrite(PIN_HS2, OCR5A, _BV(COM5A1), bits1 & OUT_BIT_HS2, hs2Duty);
}

void lsStop(uint8_t ch, bool level) {
  if (gLs[ch].running) {
    if (ch == 0) TIMSK4 &= ~_BV(OCIE4A);
    else         TIMSK4 &= ~_BV(OCIE4B);
    gLs[ch].running = false;
  }
  digitalWrite(ch == 0 ? PIN_LS1 : PIN_LS2, level ? HIGH : LOW);
}

void driveLowSide(uint8_t ch, bool on, uint8_t duty) {
  if (!on || duty == 0 || duty >= 255) {
    lsStop(ch, on);
    return;
  }
  const uint16_t period = kLsTicksPerSec / settingsLsPwmHz(ch);
  const uint16_t high = (uint16_t)((uint32_t)period * duty / 255);
  if (high < kLsMinPulse) { lsStop(ch, false); return; }
  if (period - high < kLsMinPulse) { lsStop(ch, true); return; }
  uint8_t sreg = SREG;
  cli();
  gLs[ch].on = high;
  gLs[ch].off = period - high;
  if (!gLs[ch].running) {
    gLs[ch].running = true;
    gLs[ch].high = true;
    PORTG |= kLsPortBit[ch];
    if (ch == 0) { OCR4A = TCNT4 + high; TIFR4 = _BV(OCF4A); TIMSK4 |= _BV(OCIE4A); }
    else         { OCR4B = TCNT4 + high; TIFR4 = _BV(OCF4B); TIMSK4 |= _BV(OCIE4B); }
  }
  SREG = sreg;
}

inline uint16_t lsNext(uint16_t ocr, uint16_t interval) {
  uint16_t next = ocr + interval;
  if ((int16_t)(next - TCNT4) < 8) next = TCNT4 + 8;
  return next;
}

void driveSimple(uint8_t bits0) {
  for (uint8_t i = 0; i < 5; i++) {
    digitalWrite(kLcPins[i], (bits0 & (1 << i)) ? HIGH : LOW);
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
  uint8_t dl[2] = {0, 0};
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
    if (i == 5 || i == 6) {
      dl[i - 5] = test ? testDuty(i - 3)
                : canOn ? gCanLsDuty[i - 5]
                : rulesDuty(i);
    }
  }
  gBits0 = b0;
  gBits1 = b1;
  driveSimple(b0);
  driveLowSide(0, b0 & OUT_BIT_LS1, dl[0]);
  driveLowSide(1, b0 & OUT_BIT_LS2, dl[1]);
  driveHighSide(b1, d1, d2);
  driveLogic(b1);
}
}

ISR(TIMER4_COMPA_vect) {
  SoftPwm &p = gLs[0];
  if (p.high) { PORTG &= ~kLsPortBit[0]; p.high = false; OCR4A = lsNext(OCR4A, p.off); }
  else        { PORTG |= kLsPortBit[0];  p.high = true;  OCR4A = lsNext(OCR4A, p.on); }
}

ISR(TIMER4_COMPB_vect) {
  SoftPwm &p = gLs[1];
  if (p.high) { PORTG &= ~kLsPortBit[1]; p.high = false; OCR4B = lsNext(OCR4B, p.off); }
  else        { PORTG |= kLsPortBit[1];  p.high = true;  OCR4B = lsNext(OCR4B, p.on); }
}

namespace Outputs {
void begin() {
  for (uint8_t i = 0; i < 5; i++) {
    pinMode(kLcPins[i], OUTPUT);
    digitalWrite(kLcPins[i], LOW);
  }
  pinMode(PIN_LS1, OUTPUT); digitalWrite(PIN_LS1, LOW);
  pinMode(PIN_LS2, OUTPUT); digitalWrite(PIN_LS2, LOW);
  TCCR4A = 0;
  TCCR4B = _BV(CS41) | _BV(CS40);
  TIMSK4 = 0;
  pinMode(PIN_HS1, OUTPUT);  digitalWrite(PIN_HS1, LOW);
  pinMode(PIN_HS2, OUTPUT);  digitalWrite(PIN_HS2, LOW);
  pinMode(PIN_IGN1, OUTPUT); digitalWrite(PIN_IGN1, LOW);
  pinMode(PIN_IGN2, OUTPUT); digitalWrite(PIN_IGN2, LOW);

  gBits0 = gBits1 = 0;
  gCanBits0 = gCanBits1 = 0;
  gCanHs1Duty = gCanHs2Duty = 0;
  gCanLsDuty[0] = gCanLsDuty[1] = 0;
  gHsHz = 0;
  gFailsafe = true;
}

void applyCommand(uint8_t bits0, uint8_t bits1, uint8_t hs1Duty, uint8_t hs2Duty,
                  uint8_t ls1Duty, uint8_t ls2Duty) {
  gLastCommandMs = millis();
  gFailsafe = false;
  gCanBits0 = bits0 & 0x7F;
  gCanBits1 = bits1 & 0x0F;
  gCanHs1Duty = hs1Duty;
  gCanHs2Duty = hs2Duty;
  gCanLsDuty[0] = ls1Duty;
  gCanLsDuty[1] = ls2Duty;
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
  gCanLsDuty[0] = gCanLsDuty[1] = 0;
  gFailsafe = true;
  testStop();
  compose();
}

uint8_t stateBits0() { return gBits0; }

uint8_t stateBits1() { return gBits1; }

bool inFailsafe()    { return gFailsafe; }
}
