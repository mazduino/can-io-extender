#ifndef TSLINK_H
#define TSLINK_H

#include <Arduino.h>

#define TS_SIGNATURE   "mazduino-iox 1"
#define TS_PAGE_CAL      1
#define TS_PAGE_SETTINGS 2
#define TS_PAGE_RULES    3
#define TS_PAGE_TEST     4
#define TS_PAGE_SIM      5
#define TS_OCH_SIZE      124

namespace TsLink {
void begin();
void update();
uint32_t lastActivityMs();
bool seen();
}

#endif
