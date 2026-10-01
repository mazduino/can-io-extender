#ifndef CAN_LINK_H
#define CAN_LINK_H

#include <Arduino.h>
#include <mcp2515.h>
#include "Config.h"

namespace CanLink {
bool begin();

uint32_t lockedBitrate();
bool isLocked();

void update();

bool send(uint16_t id, const uint8_t* data, uint8_t len);
bool receive(struct can_frame* frame);

bool controllerPresent();

uint32_t probingBitrate();

uint8_t sweepRxErrors();

bool sweepComplete();

uint16_t dropouts();

uint32_t rxFrames();
uint32_t lockedForMs();
}

#endif
