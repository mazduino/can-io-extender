#include "Calibration.h"
#include "Inputs.h"
#include <EEPROM.h>

namespace {
const int kEepromAddr = 0;
const uint8_t kMagic = 0xCA;
const uint8_t kVersion = 3;
const uint8_t kOldSlotCount = 4;
const uint8_t kOldSourceNone = 0x0F;

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
  memset(&s, 0, sizeof(s));
}

void sanitize(CalSlot &s) {
  s.filter &= 0x03;
  if (s.pointCount > CAL_POINT_MAX) s.pointCount = CAL_POINT_MAX;
}

void adoptOld(uint8_t source, const CalSlot &old) {
  if (source < 1 || source > CAL_SLOT_COUNT || old.pointCount == 0) return;
  CalSlot &dst = gSlots[source - 1];
  if (dst.pointCount != 0) return;
  dst = old;
  dst.filter = 0;
  sanitize(dst);
  for (uint8_t p = dst.pointCount; p < CAL_POINT_MAX; p++) {
    dst.mv[p] = 0;
    dst.value[p] = 0;
  }
}

void migrate(uint8_t version, int addr) {
  calibrationReset();
  for (uint8_t i = 0; i < kOldSlotCount; i++) {
    CalSlot old;
    if (version == 1) {
      CalSlotV1 v1;
      EEPROM.get(addr + i * (int)sizeof(CalSlotV1), v1);
      old.filter = 0;
      old.pointCount = v1.pointCount;
      for (uint8_t p = 0; p < CAL_POINT_MAX; p++) {
        old.mv[p] = v1.points[p].mv;
        old.value[p] = v1.points[p].value;
      }
      adoptOld(v1.source, old);
    } else {
      EEPROM.get(addr + i * (int)sizeof(CalSlot), old);
      adoptOld(old.filter == kOldSourceNone ? 0 : old.filter, old);
    }
  }
  calibrationSave();
}
}

void calibrationReset() {
  for (uint8_t i = 0; i < CAL_SLOT_COUNT; i++) clearSlot(gSlots[i]);
}

void calibrationLoad() {
  CalHeader h;
  EEPROM.get(kEepromAddr, h);
  const int addr = kEepromAddr + (int)sizeof(CalHeader);
  if (h.magic == kMagic && (h.version == 1 || h.version == 2)) {
    migrate(h.version, addr);
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

uint8_t calibrationInput(uint8_t slot) {
  return slot + 1;
}

bool calibrationSetPoint(uint8_t slot, uint16_t mv, int16_t value) {
  if (slot >= CAL_SLOT_COUNT) return false;
  CalSlot &s = gSlots[slot];
  const uint8_t used = calibrationUsedPoints(slot);
  s.pointCount = (used == 0 && s.mv[0] == 0 && s.value[0] == 0) ? 0 : (used ? used : 1);

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
  if (index >= calibrationUsedPoints(slot)) return false;
  out->mv = s.mv[index];
  out->value = s.value[index];
  return true;
}

bool calibrationClearPoints(uint8_t slot) {
  if (slot >= CAL_SLOT_COUNT) return false;
  clearSlot(gSlots[slot]);
  return true;
}

uint8_t calibrationUsedPoints(uint8_t slot) {
  if (slot >= CAL_SLOT_COUNT) return 0;
  const CalSlot &s = gSlots[slot];
  uint8_t n = 1;
  while (n < CAL_POINT_MAX && s.mv[n] > s.mv[n - 1]) n++;
  return n < 2 ? 0 : n;
}

int16_t calibrationValue(uint8_t slot) {
  if (slot >= CAL_SLOT_COUNT) return 0;
  const CalSlot &s = gSlots[slot];
  const uint8_t n = calibrationUsedPoints(slot);
  if (n < 2) return 0;

  const uint16_t mv = Inputs::analogMv(calibrationInput(slot));

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

uint8_t calibrationFilter(uint8_t slot) {
  return slot < CAL_SLOT_COUNT ? (gSlots[slot].filter & 0x03) : 0;
}
