#include "Inputs.h"
#include "Settings.h"


namespace {
const uint8_t kAnalogPins[ANALOG_CHANNEL_COUNT] = {
    A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10};

const uint8_t kSwitchPins[4] = {PIN_SW1, PIN_SW2, PIN_SW3, PIN_SW4};
const uint8_t kHallPins[4]   = {PIN_HALL1, PIN_HALL2, PIN_HALL3, PIN_HALL4};

uint16_t gAnalogMv[ANALOG_CHANNEL_COUNT];
uint8_t  gDigitalBits;
uint8_t  gDiagBits;
uint16_t gHallDeciHz[4];
uint16_t gHallValue[4];

const uint32_t kHallTimeoutUs = 1000000UL;
const uint8_t kHallSamples = 4;

volatile uint32_t gHallLastEdge[4];
volatile uint32_t gHallLastGap[4];
volatile uint32_t gHallGaps[4][kHallSamples];
volatile uint8_t  gHallGapIdx[4];
volatile uint8_t  gHallGapCount[4];
volatile uint8_t  gHallFilterPct[4];

uint8_t  gSim[SIM_PAGE_SIZE];

uint16_t simU16(uint8_t off) { return (uint16_t)gSim[off] | ((uint16_t)gSim[off + 1] << 8); }

void simDefaults() {
  memset(gSim, 0, sizeof(gSim));
  gSim[2] = 13800 & 0xFF; gSim[3] = 13800 >> 8;
  for (uint8_t i = 0; i < 10; i++) { gSim[4 + i * 2] = 2500 & 0xFF; gSim[5 + i * 2] = 2500 >> 8; }
  gSim[24] = 900 & 0xFF; gSim[25] = 900 >> 8;
}

uint16_t triangle(uint32_t t, uint32_t period, uint16_t lo, uint16_t hi) {
  const uint32_t ph = t % period;
  const uint32_t half = period / 2;
  const uint32_t x = ph < half ? ph : period - ph;
  return lo + (uint16_t)((uint32_t)(hi - lo) * x / half);
}

uint16_t hallDeciHzFor(uint8_t i, uint16_t value) {
  const uint8_t fn = settingsHallFunction(i);
  if (fn == HALL_FN_RPM) return (uint16_t)((uint32_t)value * settingsHallPpr10(i) / 60UL);
  if (fn == HALL_FN_SPEED) return (uint16_t)((uint32_t)value * settingsHallPulsesPerKm(i) / 3600UL);
  return value;
}

void simulate(uint32_t t) {
  uint16_t hall[4];
  if (gSim[0] & SIM_AUTO) {
    gAnalogMv[0] = (uint16_t)(triangle(t, 30000, 12600, 14400) / VBATT_DIVIDER_RATIO);
    for (uint8_t i = 1; i < ANALOG_CHANNEL_COUNT; i++) {
      gAnalogMv[i] = triangle(t + i * 1500UL, 12000UL + i * 1000UL, 500, 4500);
    }
    hall[0] = triangle(t, 16000, 850, 7200);
    hall[1] = triangle(t, 40000, 0, 1800);
    hall[2] = triangle(t, 20000, 0, 1000);
    hall[3] = triangle(t, 24000, 0, 1000);
    const uint32_t phase = t % 20000;
    const bool blink = (t / 333) & 1;
    uint8_t bits = 0x20;
    if (phase < 5000 && blink) bits |= 0x01;
    if (phase >= 5000 && phase < 10000 && blink) bits |= 0x02;
    if (phase >= 10000 && phase < 15000 && blink) bits |= 0x03;
    if ((t / 4000) & 1) bits |= 0x04;
    if (hall[0] < 1500) bits |= 0x08;
    if (phase >= 8000) bits |= 0x10;
    gDigitalBits = bits;
  } else {
    gAnalogMv[0] = (uint16_t)(simU16(2) / VBATT_DIVIDER_RATIO);
    for (uint8_t i = 1; i < ANALOG_CHANNEL_COUNT; i++) gAnalogMv[i] = simU16(2 + i * 2);
    for (uint8_t i = 0; i < 4; i++) hall[i] = simU16(24 + i * 2);
    gDigitalBits = gSim[1];
  }
  for (uint8_t i = 0; i < 4; i++) {
    gHallValue[i] = hall[i];
    gHallDeciHz[i] = hallDeciHzFor(i, hall[i]);
  }
  gDiagBits = 0;
}

uint8_t  gAnalogCursor;
uint32_t gLastAnalogMs;
uint32_t gLastDigitalMs;
uint32_t gLastFreqMs;

inline void hallEdge(uint8_t i) {
  const uint32_t t = micros();
  const uint32_t gap = t - gHallLastEdge[i];
  if (gHallGapCount[i] == 0 && gHallLastEdge[i] == 0) {
    gHallLastEdge[i] = t;
    return;
  }
  if (gap > kHallTimeoutUs) {
    gHallLastEdge[i] = t;
    gHallLastGap[i] = 0;
    gHallGapCount[i] = 0;
    return;
  }
  const uint8_t pct = gHallFilterPct[i];
  if (pct && gHallLastGap[i] && gap < (gHallLastGap[i] / 100UL) * pct) return;
  gHallLastEdge[i] = t;
  gHallLastGap[i] = gap;
  gHallGaps[i][gHallGapIdx[i]] = gap;
  gHallGapIdx[i] = (gHallGapIdx[i] + 1) % kHallSamples;
  if (gHallGapCount[i] < kHallSamples) gHallGapCount[i]++;
}

void hall1Isr() { hallEdge(0); }
void hall2Isr() { hallEdge(1); }
void hall3Isr() { hallEdge(2); }
void hall4Isr() { hallEdge(3); }

uint16_t smooth(uint32_t value, uint16_t prior, uint8_t alpha) {
  const uint32_t v = (value * (256UL - alpha) + (uint32_t)prior * alpha) >> 8;
  return v > 0xFFFF ? 0xFFFF : (uint16_t)v;
}

void updateHall(uint8_t i) {
  gHallFilterPct[i] = settingsHallFilterPct(i);

  noInterrupts();
  const uint8_t count = gHallGapCount[i];
  const uint32_t last = gHallLastEdge[i];
  uint32_t sum = 0;
  for (uint8_t k = 0; k < count; k++) sum += gHallGaps[i][k];
  interrupts();

  if (count == 0 || sum == 0 || micros() - last > kHallTimeoutUs) {
    gHallDeciHz[i] = 0;
    gHallValue[i] = 0;
    return;
  }

  const uint32_t deciHz = (10000000UL * count) / sum;
  uint32_t value = deciHz;
  switch (settingsHallFunction(i)) {
    case HALL_FN_RPM: {
      const uint16_t ppr10 = settingsHallPpr10(i);
      value = ppr10 ? (600000000UL / ppr10) * count / sum : 0;
      break;
    }
    case HALL_FN_SPEED: {
      const uint16_t ppkm = settingsHallPulsesPerKm(i);
      value = ppkm ? (uint32_t)((36000000000ULL * count) / ((uint64_t)sum * ppkm)) : 0;
      break;
    }
    default:
      break;
  }
  const uint8_t alpha = settingsHallSmoothing(i);
  gHallDeciHz[i] = gHallDeciHz[i] ? smooth(deciHz, gHallDeciHz[i], alpha)
                                  : (uint16_t)min(deciHz, 0xFFFFUL);
  gHallValue[i] = gHallValue[i] ? smooth(value, gHallValue[i], alpha)
                                : (uint16_t)min(value, 0xFFFFUL);
}

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
  simDefaults();
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
  memset(gHallValue, 0, sizeof(gHallValue));
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

  if (now - gLastFreqMs >= FREQ_WINDOW_MS) {
    gLastFreqMs = now;
    for (uint8_t i = 0; i < 4; i++) updateHall(i);
  }
  if (gSim[0] & SIM_ON) simulate(now);
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

uint16_t hallValue(uint8_t channel) {
  return (channel < 4) ? gHallValue[channel] : 0;
}

bool simActive() { return gSim[0] & SIM_ON; }

void simStop() { gSim[0] = 0; }

uint8_t simPageRead(uint16_t offset) {
  return offset < SIM_PAGE_SIZE ? gSim[offset] : 0;
}

void simPageWrite(uint16_t offset, uint8_t value) {
  if (offset < SIM_PAGE_SIZE) gSim[offset] = value;
}
}
