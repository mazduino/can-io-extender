#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "Config.h"

#define CAL_SLOT_COUNT 4
#define CAL_POINT_MAX  8

struct CalPoint {
  uint16_t mv;
  int16_t  value;
};

struct CalSlot {
  uint8_t  source;
  uint8_t  pointCount;
  CalPoint points[CAL_POINT_MAX];
};

#define CAL_SOURCE_NONE 0xFF

void calibrationLoad();
void calibrationSave();
void calibrationReset();

const CalSlot* calibrationSlot(uint8_t slot);

bool calibrationSetSource(uint8_t slot, uint8_t channel);

bool calibrationSetPoint(uint8_t slot, uint16_t mv, int16_t value);

bool calibrationGetPoint(uint8_t slot, uint8_t index, CalPoint *out);
bool calibrationClearPoints(uint8_t slot);

int16_t calibrationValue(uint8_t slot);

#endif
