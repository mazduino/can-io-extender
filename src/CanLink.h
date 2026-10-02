#ifndef CAN_LINK_H
#define CAN_LINK_H

#include <Arduino.h>
#include <mcp2515.h>
#include "Config.h"

namespace CanLink {
bool begin();
void restart();

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
uint16_t rxPerSecond();
uint16_t txPerSecond();
uint16_t txFailures();
uint8_t  rxErrorCount();
uint8_t  txErrorCount();
bool     fixedBitrate();
}

#endif
