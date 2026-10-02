#include "Calibration.h"
#include "Inputs.h"
#include <EEPROM.h>

namespace {
const int kEepromAddr = 0;
const uint8_t kMagic = 0xCA;
const uint8_t kVersion = 2;

struct CalHeader {
  uint8_t magic;
  uint8_t version;
};

struct CalSlotV1 {
  uint8_t  source;
  uint8_t  pointCount;
  CalPoint points[CAL_POINT_MAX];
};

CalSlot gSlots[CAL_SLOT_COUNT];

void clearSlot(CalSlot &s) {
  s.source = CAL_SOURCE_NONE;
  s.pointCount = 0;
  memset(s.mv, 0, sizeof(s.mv));
  memset(s.value, 0, sizeof(s.value));
}

bool sourceValid(uint8_t source) {
  return source < ANALOG_CHANNEL_COUNT;
}

void sanitize(CalSlot &s) {
  if (s.pointCount > CAL_POINT_MAX) s.pointCount = CAL_POINT_MAX;
  if (!sourceValid(s.source)) s.source = CAL_SOURCE_NONE;
}

void loadV1(int addr) {
  for (uint8_t i = 0; i < CAL_SLOT_COUNT; i++) {
    CalSlotV1 old;
    EEPROM.get(addr, old);
    addr += (int)sizeof(CalSlotV1);
    CalSlot &s = gSlots[i];
    s.source = old.source;
    s.pointCount = old.pointCount;
    for (uint8_t p = 0; p < CAL_POINT_MAX; p++) {
      s.mv[p] = old.points[p].mv;
      s.value[p] = old.points[p].value;
    }
    sanitize(s);
  }
}
}

void calibrationReset() {
  for (uint8_t i = 0; i < CAL_SLOT_COUNT; i++) clearSlot(gSlots[i]);
}

void calibrationLoad() {
  CalHeader h;
  EEPROM.get(kEepromAddr, h);
  const int addr = kEepromAddr + (int)sizeof(CalHeader);
  if (h.magic == kMagic && h.version == 1) {
    loadV1(addr);
    calibrationSave();
    return;
  }
  if (h.magic != kMagic || h.version != kVersion) {
    calibrationReset();
    return;
  }
  EEPROM.get(addr, gSlots);
  for (uint8_t i = 0; i < CAL_SLOT_COUNT; i++) sanitize(gSlots[i]);
}

void calibrationSave() {
  CalHeader h = {kMagic, kVersion};
  EEPROM.put(kEepromAddr, h);
  EEPROM.put(kEepromAddr + (int)sizeof(CalHeader), gSlots);
}

const CalSlot* calibrationSlot(uint8_t slot) {
  return (slot < CAL_SLOT_COUNT) ? &gSlots[slot] : nullptr;
}

bool calibrationSetSource(uint8_t slot, uint8_t channel) {
  if (slot >= CAL_SLOT_COUNT) return false;
  if (channel == 0xFF) channel = CAL_SOURCE_NONE;
  if (channel != CAL_SOURCE_NONE && !sourceValid(channel)) return false;
  gSlots[slot].source = channel;
  return true;
}

bool calibrationSetPoint(uint8_t slot, uint16_t mv, int16_t value) {
  if (slot >= CAL_SLOT_COUNT) return false;
  CalSlot &s = gSlots[slot];

  for (uint8_t i = 0; i < s.pointCount; i++) {
    if (s.mv[i] == mv) {
      s.value[i] = value;
      return true;
    }
  }
  if (s.pointCount >= CAL_POINT_MAX) return false;

  uint8_t pos = s.pointCount;
  while (pos > 0 && s.mv[pos - 1] > mv) {
    s.mv[pos] = s.mv[pos - 1];
    s.value[pos] = s.value[pos - 1];
    pos--;
  }
  s.mv[pos] = mv;
  s.value[pos] = value;
  s.pointCount++;
  return true;
}

bool calibrationGetPoint(uint8_t slot, uint8_t index, CalPoint *out) {
  if (slot >= CAL_SLOT_COUNT || !out) return false;
  const CalSlot &s = gSlots[slot];
  if (index >= s.pointCount) return false;
  out->mv = s.mv[index];
  out->value = s.value[index];
  return true;
}

bool calibrationClearPoints(uint8_t slot) {
  if (slot >= CAL_SLOT_COUNT) return false;
  gSlots[slot].pointCount = 0;
  return true;
}

uint16_t calibrationSourceMv(uint8_t slot) {
  if (slot >= CAL_SLOT_COUNT || !sourceValid(gSlots[slot].source)) return 0;
  return Inputs::analogMv(gSlots[slot].source);
}

int16_t calibrationValue(uint8_t slot) {
  if (slot >= CAL_SLOT_COUNT) return 0;
  const CalSlot &s = gSlots[slot];
  const uint8_t n = s.pointCount > CAL_POINT_MAX ? CAL_POINT_MAX : s.pointCount;
  if (!sourceValid(s.source) || n < 2) return 0;

  const uint16_t mv = Inputs::analogMv(s.source);

  if (mv <= s.mv[0]) return s.value[0];
  if (mv >= s.mv[n - 1]) return s.value[n - 1];

  for (uint8_t i = 1; i < n; i++) {
    if (mv > s.mv[i]) continue;
    const int32_t dx = (int32_t)s.mv[i] - s.mv[i - 1];
    if (dx <= 0) return s.value[i];
    const int32_t dy = (int32_t)s.value[i] - s.value[i - 1];
    return (int16_t)(s.value[i - 1] + (dy * ((int32_t)mv - s.mv[i - 1])) / dx);
  }
  return s.value[n - 1];
}

uint8_t calibrationPageRead(uint16_t offset) {
  if (offset >= CAL_PAGE_SIZE) return 0;
  return ((const uint8_t*)gSlots)[offset];
}

void calibrationPageWrite(uint16_t offset, uint8_t value) {
  if (offset >= CAL_PAGE_SIZE) return;
  ((uint8_t*)gSlots)[offset] = value;
}
