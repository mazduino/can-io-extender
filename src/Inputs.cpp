#include "Inputs.h"

namespace {
const uint8_t kAnalogPins[ANALOG_CHANNEL_COUNT] = {
    A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10};

const uint8_t kSwitchPins[4] = {PIN_SW1, PIN_SW2, PIN_SW3, PIN_SW4};
const uint8_t kHallPins[4]   = {PIN_HALL1, PIN_HALL2, PIN_HALL3, PIN_HALL4};

uint16_t gAnalogMv[ANALOG_CHANNEL_COUNT];
uint8_t  gDigitalBits;
uint8_t  gDiagBits;
uint16_t gHallDeciHz[4];

volatile uint16_t gHallPulses[4];

uint8_t  gAnalogCursor;
uint32_t gLastAnalogMs;
uint32_t gLastDigitalMs;
uint32_t gLastFreqMs;

void hall1Isr() { gHallPulses[0]++; }
void hall2Isr() { gHallPulses[1]++; }
void hall3Isr() { gHallPulses[2]++; }
void hall4Isr() { gHallPulses[3]++; }

uint16_t readAnalogMv(uint8_t pin) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 4; i++) {
    sum += analogRead(pin);
  }
  const uint16_t counts = sum / 4;
  return (uint16_t)(((uint32_t)counts * ADC_VREF_MV) / 1023UL);
}
}

namespace Inputs {
void begin() {
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(kSwitchPins[i], INPUT);
    pinMode(kHallPins[i], INPUT);
  }
  pinMode(PIN_HS1_DIAG, INPUT);
  pinMode(PIN_HS2_DIAG, INPUT);

  attachInterrupt(digitalPinToInterrupt(PIN_HALL1), hall1Isr, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_HALL2), hall2Isr, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_HALL3), hall3Isr, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_HALL4), hall4Isr, FALLING);

  gAnalogCursor = 0;
  memset(gAnalogMv, 0, sizeof(gAnalogMv));
  memset(gHallDeciHz, 0, sizeof(gHallDeciHz));
  gDigitalBits = 0;
  gDiagBits = 0;
}

void update() {
  const uint32_t now = millis();

  if (now - gLastAnalogMs >= ANALOG_SAMPLE_INTERVAL_MS) {
    gLastAnalogMs = now;

    gAnalogMv[gAnalogCursor] = readAnalogMv(kAnalogPins[gAnalogCursor]);
    if (++gAnalogCursor >= ANALOG_CHANNEL_COUNT) gAnalogCursor = 0;
  }

  if (now - gLastDigitalMs >= DIGITAL_SAMPLE_INTERVAL_MS) {
    gLastDigitalMs = now;

    uint8_t bits = 0;
    for (uint8_t i = 0; i < 4; i++) {
      if (digitalRead(kSwitchPins[i]) == LOW) bits |= (1 << i);
      if (digitalRead(kHallPins[i]) == HIGH)  bits |= (1 << (4 + i));
    }
    gDigitalBits = bits;

    uint8_t diag = 0;
    if (batteryMv() >= VBATT_PRESENT_MV) {
      if (digitalRead(PIN_HS1_DIAG) == LOW) diag |= 0x01;
      if (digitalRead(PIN_HS2_DIAG) == LOW) diag |= 0x02;
    }
    gDiagBits = diag;
  }

  const uint32_t freqElapsed = now - gLastFreqMs;
  if (freqElapsed >= FREQ_WINDOW_MS) {
    gLastFreqMs = now;
    for (uint8_t i = 0; i < 4; i++) {
      uint16_t pulses;
      noInterrupts();
      pulses = gHallPulses[i];
      gHallPulses[i] = 0;
      interrupts();

      const uint32_t deciHz = ((uint32_t)pulses * 10000UL) / freqElapsed;
      gHallDeciHz[i] = (deciHz > 0xFFFF) ? 0xFFFF : (uint16_t)deciHz;
    }
  }
}

uint16_t analogMv(uint8_t channel) {
  return (channel < ANALOG_CHANNEL_COUNT) ? gAnalogMv[channel] : 0;
}

uint16_t batteryMv() {
  return (uint16_t)min((uint32_t)(gAnalogMv[0] * VBATT_DIVIDER_RATIO), 65535UL);
}

uint8_t digitalBits() { return gDigitalBits; }
uint8_t diagBits()    { return gDiagBits; }

uint16_t hallDeciHz(uint8_t channel) {
  return (channel < 4) ? gHallDeciHz[channel] : 0;
}
}
