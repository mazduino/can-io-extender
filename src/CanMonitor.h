#ifndef CAN_MONITOR_H
#define CAN_MONITOR_H

#include <Arduino.h>
#include <mcp2515.h>

#define CANMON_TOP 6
#define CANMON_ALL 0xFFFFFFFFUL

namespace CanMonitor {
void record(const struct can_frame &f);
void update(uint32_t nowMs);

void top(uint8_t rank, uint32_t &id, uint16_t &rate);
void watched(uint16_t id, uint16_t &rate, uint8_t &dlc, uint8_t *data);

void consoleToggle(uint32_t filter);
void consoleStop();
bool consoleOn();
}

#endif
