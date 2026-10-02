#include "Settings.h"
#include "Config.h"
#include <EEPROM.h>

namespace {
const int kEepromAddr = 1024;
const uint8_t kMagic = 0x5E;
const uint8_t kVersion = 2;

struct SettingsHeader {
  uint8_t magic;
  uint8_t version;
  uint8_t buildNode;
};

struct SettingsPage {
  uint8_t  node;
  uint8_t  bitrate;
  uint16_t hallPpr10[HALL_COUNT];
  uint8_t  hallFilter[HALL_COUNT];
  uint8_t  hallSmoothing[HALL_COUNT];
  uint8_t  hallSignal[HALL_COUNT];
  uint8_t  reserved[SETTINGS_PAGE_SIZE - 22];
};

const uint8_t kSignalCylMask   = 0x0F;
const uint8_t kSignalTwoStroke = 0x10;
const uint8_t kSignalCustomPpr = 0x20;
const uint8_t kSignalFnShift   = 6;

const uint8_t kFilterPct[4] = {0, 25, 50, 75};

SettingsPage gPage;

void sanitize() {
  gPage.node &= 0x03;
  if (gPage.bitrate > BITRATE_1M) gPage.bitrate = BITRATE_AUTO;
  for (uint8_t i = 0; i < HALL_COUNT; i++) gPage.hallFilter[i] &= 0x03;
}

void defaults() {
  memset(&gPage, 0, sizeof(gPage));
  gPage.node = NODE_ID;
  gPage.bitrate = BITRATE_AUTO;
  for (uint8_t i = 0; i < HALL_COUNT; i++) {
    gPage.hallPpr10[i] = 20;
    gPage.hallFilter[i] = 1;
    gPage.hallSmoothing[i] = 128;
    gPage.hallSignal[i] = 4;
  }
}
}

void settingsLoad() {
  SettingsHeader h;
  EEPROM.get(kEepromAddr, h);
  if (h.magic == kMagic && h.version == 1) {
    uint8_t old[2];
    EEPROM.get(kEepromAddr + (int)sizeof(h), old);
    defaults();
    gPage.node = old[0];
    gPage.bitrate = old[1];
    if (h.buildNode != NODE_ID) gPage.node = NODE_ID;
    sanitize();
    settingsSave();
    return;
  }
  if (h.magic != kMagic || h.version != kVersion) {
    defaults();
    settingsSave();
    return;
  }
  EEPROM.get(kEepromAddr + (int)sizeof(h), gPage);
  if (h.buildNode != NODE_ID) {
    gPage.node = NODE_ID;
    sanitize();
    settingsSave();
    return;
  }
  sanitize();
}

void settingsSave() {
  SettingsHeader h = {kMagic, kVersion, NODE_ID};
  EEPROM.put(kEepromAddr, h);
  EEPROM.put(kEepromAddr + (int)sizeof(h), gPage);
}

uint8_t settingsNode() { return gPage.node & 0x03; }

uint8_t settingsBitrate() {
  return gPage.bitrate > BITRATE_1M ? BITRATE_AUTO : gPage.bitrate;
}

uint8_t settingsHallFunction(uint8_t hall) {
  if (hall >= HALL_COUNT) return HALL_FN_FREQ;
  const uint8_t fn = gPage.hallSignal[hall] >> kSignalFnShift;
  return fn > HALL_FN_FREQ ? HALL_FN_FREQ : fn;
}

uint16_t settingsHallPulsesPerKm(uint8_t hall) {
  return hall < HALL_COUNT ? gPage.hallPpr10[hall] : 0;
}

uint16_t settingsHallPpr10(uint8_t hall) {
  if (hall >= HALL_COUNT) return 0;
  const uint8_t sig = gPage.hallSignal[hall];
  if (sig & kSignalCustomPpr) return gPage.hallPpr10[hall];
  const uint8_t cyl = sig & kSignalCylMask;
  if (cyl == 0 || cyl > 12) return 0;
  return (sig & kSignalTwoStroke) ? cyl * 10 : cyl * 5;
}

uint8_t settingsHallFilterPct(uint8_t hall) {
  return hall < HALL_COUNT ? kFilterPct[gPage.hallFilter[hall] & 0x03] : 0;
}

uint8_t settingsHallSmoothing(uint8_t hall) {
  return hall < HALL_COUNT ? gPage.hallSmoothing[hall] : 0;
}

uint8_t settingsPageRead(uint16_t offset) {
  if (offset >= SETTINGS_PAGE_SIZE) return 0;
  return ((const uint8_t*)&gPage)[offset];
}

void settingsPageWrite(uint16_t offset, uint8_t value) {
  if (offset >= SETTINGS_PAGE_SIZE) return;
  ((uint8_t*)&gPage)[offset] = value;
}
