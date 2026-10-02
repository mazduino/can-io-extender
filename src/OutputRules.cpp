#include "OutputRules.h"
#include "Inputs.h"
#include "Calibration.h"
#include <EEPROM.h>

namespace {
const int kEepromAddr = 1536;
const uint8_t kMagic = 0x0A;
const uint8_t kVersion = 1;

const uint8_t SRC_NONE = 0;
const uint8_t SRC_AV_FIRST = 1;
const uint8_t SRC_RAW_FIRST = 11;
const uint8_t SRC_HALL_FIRST = 21;
const uint8_t SRC_SW_FIRST = 25;
const uint8_t SRC_BATTERY = 29;

const uint8_t CMP_GT = 0, CMP_GE = 1, CMP_LT = 2, CMP_LE = 3, CMP_EQ = 4, CMP_NE = 5;
const uint8_t LOGIC_NONE = 0, LOGIC_AND = 1, LOGIC_OR = 2;

struct RulesHeader {
  uint8_t magic;
  uint8_t version;
};

OutRule gRules[OUT_COUNT];
bool gCond1[OUT_COUNT];
bool gCond2[OUT_COUNT];
bool gActive[OUT_COUNT];
uint32_t gTrueSince[OUT_COUNT];

uint8_t gTest[TEST_PAGE_SIZE];

bool sourceValue(uint8_t src, int32_t &v) {
  if (src >= SRC_AV_FIRST && src < SRC_AV_FIRST + 10) {
    v = calibrationValue(src - SRC_AV_FIRST);
  } else if (src >= SRC_RAW_FIRST && src < SRC_RAW_FIRST + 10) {
    v = Inputs::analogMv(src - SRC_RAW_FIRST + 1);
  } else if (src >= SRC_HALL_FIRST && src < SRC_HALL_FIRST + 4) {
    v = Inputs::hallValue(src - SRC_HALL_FIRST);
  } else if (src >= SRC_SW_FIRST && src < SRC_SW_FIRST + 4) {
    v = (Inputs::digitalBits() >> (src - SRC_SW_FIRST)) & 1;
  } else if (src == SRC_BATTERY) {
    v = Inputs::batteryMv();
  } else {
    return false;
  }
  return true;
}

bool evalCond(uint8_t src, uint8_t cmp, int32_t target, int32_t hyst, bool prev) {
  int32_t v;
  if (!sourceValue(src, v)) return false;
  if (hyst < 0) hyst = 0;
  switch (cmp) {
    case CMP_GT: return prev ? v >= target - hyst : v > target;
    case CMP_GE: return prev ? v >= target - hyst : v >= target;
    case CMP_LT: return prev ? v <= target + hyst : v < target;
    case CMP_LE: return prev ? v <= target + hyst : v <= target;
    case CMP_EQ: return v == target;
    case CMP_NE: return v != target;
    default:     return false;
  }
}
}

void rulesLoad() {
  RulesHeader h;
  EEPROM.get(kEepromAddr, h);
  if (h.magic != kMagic || h.version != kVersion) {
    memset(gRules, 0, sizeof(gRules));
  } else {
    EEPROM.get(kEepromAddr + (int)sizeof(h), gRules);
  }
  memset(gCond1, 0, sizeof(gCond1));
  memset(gCond2, 0, sizeof(gCond2));
  memset(gActive, 0, sizeof(gActive));
  memset(gTrueSince, 0, sizeof(gTrueSince));
  memset(gTest, 0, sizeof(gTest));
}

void rulesSave() {
  RulesHeader h = {kMagic, kVersion};
  EEPROM.put(kEepromAddr, h);
  EEPROM.put(kEepromAddr + (int)sizeof(h), gRules);
}

void rulesUpdate(uint32_t nowMs) {
  for (uint8_t i = 0; i < OUT_COUNT; i++) {
    const OutRule &r = gRules[i];
    if ((r.mode & 0x03) == OUT_MODE_CAN || r.src1 == SRC_NONE) {
      gActive[i] = false;
      gTrueSince[i] = 0;
      continue;
    }
    gCond1[i] = evalCond(r.src1, r.ops & 0x07, r.target1, r.hyst1, gCond1[i]);
    const uint8_t logic = (r.ops >> 3) & 0x03;
    bool on = gCond1[i];
    if (logic != LOGIC_NONE && r.src2 != SRC_NONE) {
      gCond2[i] = evalCond(r.src2, (r.ops >> 5) & 0x07, r.target2, r.hyst2, gCond2[i]);
      if (logic == LOGIC_AND) on = on && gCond2[i];
      else if (logic == LOGIC_OR) on = on || gCond2[i];
    }
    if (!on) {
      gActive[i] = false;
      gTrueSince[i] = 0;
      continue;
    }
    if (gTrueSince[i] == 0) gTrueSince[i] = nowMs ? nowMs : 1;
    gActive[i] = (nowMs - gTrueSince[i]) >= (uint32_t)r.onDelay * 100UL;
  }
}

uint8_t rulesMode(uint8_t out) {
  if (out >= OUT_COUNT) return OUT_MODE_CAN;
  const uint8_t m = gRules[out].mode & 0x03;
  return m > OUT_MODE_CAN_RULE ? OUT_MODE_CAN : m;
}

bool rulesActive(uint8_t out) {
  return out < OUT_COUNT && gActive[out];
}

uint8_t rulesDuty(uint8_t out) {
  return out < OUT_COUNT ? gRules[out].duty : 0;
}

uint8_t rulesPageRead(uint16_t offset) {
  if (offset >= RULES_PAGE_SIZE) return 0;
  return ((const uint8_t*)gRules)[offset];
}

void rulesPageWrite(uint16_t offset, uint8_t value) {
  if (offset >= RULES_PAGE_SIZE) return;
  ((uint8_t*)gRules)[offset] = value;
}

bool testActive() { return gTest[0] & 1; }

bool testOutput(uint8_t out) {
  if (out < 7) return (gTest[1] >> out) & 1;
  if (out < OUT_COUNT) return (gTest[2] >> (out - 7)) & 1;
  return false;
}

uint8_t testDuty(uint8_t hs) { return hs < 2 ? gTest[3 + hs] : 0; }

void testStop() { memset(gTest, 0, sizeof(gTest)); }

uint8_t testPageRead(uint16_t offset) {
  return offset < TEST_PAGE_SIZE ? gTest[offset] : 0;
}

void testPageWrite(uint16_t offset, uint8_t value) {
  if (offset < TEST_PAGE_SIZE) gTest[offset] = value;
}
