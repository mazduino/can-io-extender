#ifndef INPUTS_H
#define INPUTS_H

#include <Arduino.h>
#include "Config.h"

namespace Inputs {
void begin();
void update();

uint16_t analogMv(uint8_t channel);

uint16_t batteryMv();

uint8_t digitalBits();

uint8_t diagBits();

uint16_t hallDeciHz(uint8_t channel);
uint16_t hallValue(uint8_t channel);
}

#endif
