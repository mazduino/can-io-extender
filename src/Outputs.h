#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <Arduino.h>
#include "Config.h"

namespace Outputs {
#define OUT_BIT_LC1 0x01
#define OUT_BIT_LC2 0x02
#define OUT_BIT_LC3 0x04
#define OUT_BIT_LC4 0x08
#define OUT_BIT_LC5 0x10
#define OUT_BIT_LS1 0x20
#define OUT_BIT_LS2 0x40

#define OUT_BIT_HS1  0x01
#define OUT_BIT_HS2  0x02
#define OUT_BIT_IGN1 0x04
#define OUT_BIT_IGN2 0x08

void begin();

void applyCommand(uint8_t bits0, uint8_t bits1, uint8_t hs1Duty, uint8_t hs2Duty);

void update();

void allOff();

uint8_t stateBits0();
uint8_t stateBits1();

bool inFailsafe();
}

#endif
