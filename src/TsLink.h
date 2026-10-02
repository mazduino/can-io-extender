#ifndef TSLINK_H
#define TSLINK_H

#include <Arduino.h>

#define TS_SIGNATURE   "mazduino-iox 1"
#define TS_PAGE_ID     1
#define TS_OCH_SIZE    56

namespace TsLink {
void begin();
void update();
uint32_t lastActivityMs();
}

#endif
