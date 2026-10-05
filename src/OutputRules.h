#ifndef OUTPUT_RULES_H
#define OUTPUT_RULES_H

#include <Arduino.h>

#define OUT_COUNT 11

#define OUT_MODE_CAN      0
#define OUT_MODE_RULE     1
#define OUT_MODE_CAN_RULE 2
#define OUT_MODE_CAN_AND  3

struct OutRule {
  uint8_t mode;
  uint8_t src1;
  uint8_t ops;
  uint8_t src2;
  int16_t target1;
  int16_t hyst1;
  int16_t target2;
  int16_t hyst2;
  uint8_t onDelay;
  uint8_t duty;
  uint8_t flags;
  uint8_t limit;
};

#define RULES_PAGE_SIZE (OUT_COUNT * sizeof(OutRule))
#define TEST_PAGE_SIZE  8

void rulesLoad();
void rulesSave();
void rulesUpdate(uint32_t nowMs);

uint8_t rulesMode(uint8_t out);
bool rulesActive(uint8_t out);
uint8_t rulesDuty(uint8_t out);

uint8_t rulesPageRead(uint16_t offset);
void rulesPageWrite(uint16_t offset, uint8_t value);

bool testActive();
bool testOutput(uint8_t out);
uint8_t testDuty(uint8_t out);
void testStop();
uint8_t testPageRead(uint16_t offset);
void testPageWrite(uint16_t offset, uint8_t value);

#endif
