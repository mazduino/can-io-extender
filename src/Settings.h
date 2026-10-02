#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

#define BITRATE_AUTO  0
#define BITRATE_125K  1
#define BITRATE_250K  2
#define BITRATE_500K  3
#define BITRATE_1M    4

#define SETTINGS_PAGE_SIZE 8

void settingsLoad();
void settingsSave();

uint8_t settingsNode();
uint8_t settingsBitrate();

uint8_t settingsPageRead(uint16_t offset);
void settingsPageWrite(uint16_t offset, uint8_t value);

#endif
