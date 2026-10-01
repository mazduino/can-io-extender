#include "Calibration.h"
#include "Inputs.h"
#include <EEPROM.h>

namespace {
const int kEepromAddr = 0;
const uint8_t kMagic = 0xCA;
const uint8_t kVersion = 1;

struct CalHeader {
  uint8_t magic;
  uint8_t version;
};

CalSlot gSlots[CAL_SLOT_COUNT];

void clearSlot(CalSlot &s) {
  s.source = CAL_SOURCE_NONE;
  s.pointCount = 0;
  memset(s.points, 0, sizeof(s.points));
}
}

void calibrationReset() {
  for (uint8_t i = 0; i < CAL_SLOT_COUNT; i++) clearSlot(gSlots[i]);
}

void calibrationLoad() {
  CalHeader h;
  EEPROM.get(kEepromAddr, h);
  if (h.magic != kMagic || h.version != kVersion) {
    calibrationReset();
    return;
  }
  int addr = kEepromAddr + (int)sizeof(CalHeader);
  for (uint8_t i = 0; i < CAL_SLOT_COUNT; i++) {
    EEPROM.get(addr, gSlots[i]);
    addr += (int)sizeof(CalSlot);

    if (gSlots[i].pointCount > CAL_POINT_MAX) clearSlot(gSlots[i]);
    if (gSlots[i].source != CAL_SOURCE_NONE &&
        gSlots[i].source >= ANALOG_CHANNEL_COUNT) {
      clearSlot(gSlots[i]);
    }
  }
}

void calibrationSave() {
  CalHeader h = {kMagic, kVersion};
  EEPROM.put(kEepromAddr, h);
  int addr = kEepromAddr + (int)sizeof(CalHeader);
  for (uint8_t i = 0; i < CAL_SLOT_COUNT; i++) {
    EEPROM.put(addr, gSlots[i]);
    addr += (int)sizeof(CalSlot);
  }
}

const CalSlot* calibrationSlot(uint8_t slot) {
  return (slot < CAL_SLOT_COUNT) ? &gSlots[slot] : nullptr;
}

bool calibrationSetSource(uint8_t slot, uint8_t channel) {
  if (slot >= CAL_SLOT_COUNT) return false;
  if (channel != CAL_SOURCE_NONE && channel >= ANALOG_CHANNEL_COUNT) return false;
  gSlots[slot].source = channel;
  return true;
}

bool calibrationSetPoint(uint8_t slot, uint16_t mv, int16_t value) {
  if (slot >= CAL_SLOT_COUNT) return false;
  CalSlot &s = gSlots[slot];

  for (uint8_t i = 0; i < s.pointCount; i++) {
    if (s.points[i].mv == mv) {
      s.points[i].value = value;
      return true;
    }
  }
  if (s.pointCount >= CAL_POINT_MAX) return false;

  uint8_t pos = s.pointCount;
  while (pos > 0 && s.points[pos - 1].mv > mv) {
    s.points[pos] = s.points[pos - 1];
    pos--;
  }
  s.points[pos].mv = mv;
  s.points[pos].value = value;
  s.pointCount++;
  return true;
}

bool calibrationGetPoint(uint8_t slot, uint8_t index, CalPoint *out) {
  if (slot >= CAL_SLOT_COUNT || !out) return false;
  const CalSlot &s = gSlots[slot];
  if (index >= s.pointCount) return false;
  *out = s.points[index];
  return true;
}

bool calibrationClearPoints(uint8_t slot) {
  if (slot >= CAL_SLOT_COUNT) return false;
  gSlots[slot].pointCount = 0;
  return true;
}

int16_t calibrationValue(uint8_t slot) {
  if (slot >= CAL_SLOT_COUNT) return 0;
  const CalSlot &s = gSlots[slot];
  if (s.source == CAL_SOURCE_NONE || s.pointCount < 2) return 0;

  const uint16_t mv = Inputs::analogMv(s.source);

  if (mv <= s.points[0].mv) return s.points[0].value;
  if (mv >= s.points[s.pointCount - 1].mv) return s.points[s.pointCount - 1].value;

  for (uint8_t i = 1; i < s.pointCount; i++) {
    const CalPoint &a = s.points[i - 1];
    const CalPoint &b = s.points[i];
    if (mv > b.mv) continue;

    const int32_t dx = (int32_t)b.mv - a.mv;
    if (dx == 0) return b.value;
    const int32_t dy = (int32_t)b.value - a.value;

    return (int16_t)(a.value + (dy * ((int32_t)mv - a.mv)) / dx);
  }
  return s.points[s.pointCount - 1].value;
}
