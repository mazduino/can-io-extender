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
  uint16_t mv[CAL_POINT_MAX];
  int16_t  value[CAL_POINT_MAX];
};

#define CAL_SOURCE_NONE 0x0F
#define CAL_PAGE_SIZE   (CAL_SLOT_COUNT * sizeof(CalSlot))

void calibrationLoad();
void calibrationSave();
void calibrationReset();

const CalSlot* calibrationSlot(uint8_t slot);

bool calibrationSetSource(uint8_t slot, uint8_t channel);

bool calibrationSetPoint(uint8_t slot, uint16_t mv, int16_t value);

bool calibrationGetPoint(uint8_t slot, uint8_t index, CalPoint *out);
bool calibrationClearPoints(uint8_t slot);

int16_t calibrationValue(uint8_t slot);
uint16_t calibrationSourceMv(uint8_t slot);

uint8_t calibrationPageRead(uint16_t offset);
void calibrationPageWrite(uint16_t offset, uint8_t value);

#endif
