#ifndef INPUTS_H
#define INPUTS_H

#include <Arduino.h>
#include "Config.h"

#define SIM_PAGE_SIZE 32
#define SIM_ON   0x01
#define SIM_AUTO 0x02

namespace Inputs {
void begin();
void update();

uint16_t analogMv(uint8_t channel);

uint16_t batteryMv();

uint8_t digitalBits();

uint8_t diagBits();

uint16_t hallDeciHz(uint8_t channel);
uint16_t hallValue(uint8_t channel);

bool simActive();
void simStop();
uint8_t simPageRead(uint16_t offset);
void simPageWrite(uint16_t offset, uint8_t value);
}

#endif
