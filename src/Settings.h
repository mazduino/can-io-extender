#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

#define BITRATE_AUTO  0
#define BITRATE_125K  1
#define BITRATE_250K  2
#define BITRATE_500K  3
#define BITRATE_1M    4

#define SETTINGS_PAGE_SIZE 24
#define HALL_COUNT 4

void settingsLoad();
void settingsSave();

uint8_t settingsNode();
uint8_t settingsBitrate();

uint16_t settingsHallPpr10(uint8_t hall);
uint8_t settingsHallFilterPct(uint8_t hall);
uint8_t settingsHallSmoothing(uint8_t hall);

uint8_t settingsPageRead(uint16_t offset);
void settingsPageWrite(uint16_t offset, uint8_t value);

#endif
