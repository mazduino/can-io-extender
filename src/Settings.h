#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

#define BITRATE_AUTO  0
#define BITRATE_125K  1
#define BITRATE_250K  2
#define BITRATE_500K  3
#define BITRATE_1M    4

#define SETTINGS_PAGE_SIZE 32
#define HALL_COUNT 4

void settingsLoad();
void settingsSave();

uint8_t settingsNode();
uint8_t settingsBitrate();
uint16_t settingsMonitorId();

#define PWM_HZ_MIN 4
#define PWM_HZ_MAX 500

uint16_t settingsHsPwmHz();
uint16_t settingsLsPwmHz(uint8_t ls);

#define HALL_FN_RPM   0
#define HALL_FN_SPEED 1
#define HALL_FN_FREQ  2
#define HALL_FN_SWITCH 3

uint8_t settingsHallFunction(uint8_t hall);
bool settingsHallInverted(uint8_t hall);
uint16_t settingsHallPpr10(uint8_t hall);
uint16_t settingsHallPulsesPerKm(uint8_t hall);
uint8_t settingsHallFilterPct(uint8_t hall);
uint8_t settingsHallSmoothing(uint8_t hall);

uint8_t settingsPageRead(uint16_t offset);
void settingsPageWrite(uint16_t offset, uint8_t value);

#endif
