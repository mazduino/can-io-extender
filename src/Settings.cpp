#include "Settings.h"
#include "Config.h"
#include <EEPROM.h>

namespace {
const int kEepromAddr = 1024;
const uint8_t kMagic = 0x5E;
const uint8_t kVersion = 1;

struct SettingsHeader {
  uint8_t magic;
  uint8_t version;
  uint8_t buildNode;
};

struct SettingsPage {
  uint8_t node;
  uint8_t bitrate;
  uint8_t reserved[SETTINGS_PAGE_SIZE - 2];
};

SettingsPage gPage;

void sanitize() {
  gPage.node &= 0x03;
  if (gPage.bitrate > BITRATE_1M) gPage.bitrate = BITRATE_AUTO;
}
}

void settingsLoad() {
  SettingsHeader h;
  EEPROM.get(kEepromAddr, h);
  if (h.magic != kMagic || h.version != kVersion) {
    memset(&gPage, 0, sizeof(gPage));
    gPage.node = NODE_ID;
    gPage.bitrate = BITRATE_AUTO;
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

uint8_t settingsPageRead(uint16_t offset) {
  if (offset >= SETTINGS_PAGE_SIZE) return 0;
  return ((const uint8_t*)&gPage)[offset];
}

void settingsPageWrite(uint16_t offset, uint8_t value) {
  if (offset >= SETTINGS_PAGE_SIZE) return;
  ((uint8_t*)&gPage)[offset] = value;
}
