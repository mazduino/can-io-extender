#include "CanMonitor.h"

namespace {
const uint8_t kSlots = 16;
const uint8_t kNoticeBytes = 24;

struct Entry {
  uint32_t id;
  uint16_t count;
  uint16_t rate;
  uint8_t  dlc;
  uint8_t  data[8];
  bool     used;
};

Entry gEntries[kSlots];
uint8_t gOrder[kSlots];
uint32_t gLastUpdate;

bool gConsole;
uint32_t gFilter = CANMON_ALL;
uint16_t gDropped;

uint32_t frameId(const struct can_frame &f) {
  return (f.can_id & CAN_EFF_FLAG) ? (f.can_id & CAN_EFF_MASK) | 0x80000000UL
                                   : (f.can_id & CAN_SFF_MASK);
}

Entry *slotFor(uint32_t id) {
  Entry *freeSlot = nullptr;
  Entry *quietest = &gEntries[0];
  for (uint8_t i = 0; i < kSlots; i++) {
    Entry &e = gEntries[i];
    if (e.used && e.id == id) return &e;
    if (!e.used && !freeSlot) freeSlot = &e;
    if (e.rate + e.count < quietest->rate + quietest->count) quietest = &e;
  }
  Entry *e = freeSlot ? freeSlot : quietest;
  memset(e, 0, sizeof(*e));
  e->id = id;
  e->used = true;
  return e;
}

void printHex(uint32_t v, uint8_t digits) {
  for (int8_t s = (digits - 1) * 4; s >= 0; s -= 4) {
    Serial.print((char)"0123456789ABCDEF"[(v >> s) & 0x0F]);
  }
}

void printFrame(uint32_t id, const struct can_frame &f) {
  if (gDropped && Serial.availableForWrite() >= kNoticeBytes) {
    Serial.print(F("-- dropped "));
    Serial.println(gDropped);
    gDropped = 0;
  }
  const uint8_t dlc = f.can_dlc > 8 ? 8 : f.can_dlc;
  const int need = 20 + ((id & 0x80000000UL) ? 8 : 3) + dlc * 3;
  if (Serial.availableForWrite() < need) {
    if (gDropped < 0xFFFF) gDropped++;
    return;
  }
  const uint32_t ms = millis();
  Serial.print(ms / 1000);
  Serial.print('.');
  const uint16_t frac = ms % 1000;
  if (frac < 100) Serial.print('0');
  if (frac < 10) Serial.print('0');
  Serial.print(frac);
  Serial.print(F("  "));
  if (id & 0x80000000UL) printHex(id & 0x1FFFFFFFUL, 8);
  else printHex(id, 3);
  Serial.print(F("  ["));
  Serial.print(dlc);
  Serial.print(F("] "));
  for (uint8_t i = 0; i < dlc; i++) {
    Serial.print(' ');
    printHex(f.data[i], 2);
  }
  Serial.println();
}
}

namespace CanMonitor {
void record(const struct can_frame &f) {
  const uint32_t id = frameId(f);
  Entry *e = slotFor(id);
  if (e->count < 0xFFFF) e->count++;
  e->dlc = f.can_dlc > 8 ? 8 : f.can_dlc;
  memcpy(e->data, f.data, e->dlc);

  if (gConsole && (gFilter == CANMON_ALL || gFilter == id)) printFrame(id, f);
}

void update(uint32_t nowMs) {
  const uint32_t span = nowMs - gLastUpdate;
  if (span < 1000) return;
  gLastUpdate = nowMs;
  for (uint8_t i = 0; i < kSlots; i++) {
    Entry &e = gEntries[i];
    if (!e.used) continue;
    e.rate = (uint16_t)((uint32_t)e.count * 1000UL / span);
    if (e.rate == 0 && e.count == 0) e.used = false;
    e.count = 0;
  }
  for (uint8_t i = 0; i < kSlots; i++) gOrder[i] = i;
  for (uint8_t i = 1; i < kSlots; i++) {
    const uint8_t k = gOrder[i];
    int8_t j = i - 1;
    const uint16_t rk = gEntries[k].used ? gEntries[k].rate + 1 : 0;
    while (j >= 0) {
      const Entry &o = gEntries[gOrder[j]];
      const uint16_t ro = o.used ? o.rate + 1 : 0;
      if (ro >= rk) break;
      gOrder[j + 1] = gOrder[j];
      j--;
    }
    gOrder[j + 1] = k;
  }
}

void top(uint8_t rank, uint32_t &id, uint16_t &rate) {
  id = 0;
  rate = 0;
  if (rank >= kSlots) return;
  const Entry &e = gEntries[gOrder[rank]];
  if (!e.used) return;
  id = e.id;
  rate = e.rate;
}

void watched(uint16_t id, uint16_t &rate, uint8_t &dlc, uint8_t *data) {
  rate = 0;
  dlc = 0;
  memset(data, 0, 8);
  for (uint8_t i = 0; i < kSlots; i++) {
    const Entry &e = gEntries[i];
    if (!e.used || e.id != id) continue;
    rate = e.rate;
    dlc = e.dlc;
    memcpy(data, e.data, 8);
    return;
  }
}

void consoleToggle(uint32_t filter) {
  if (gConsole && filter == gFilter) {
    consoleStop();
    return;
  }
  gConsole = true;
  gFilter = filter;
  gDropped = 0;
  Serial.println();
  Serial.print(F("== CAN console, "));
  if (filter == CANMON_ALL) Serial.print(F("all IDs"));
  else { Serial.print(F("ID 0x")); printHex(filter, 3); }
  Serial.println(F(". Send ! to stop. =="));
}

void consoleStop() {
  if (!gConsole) return;
  gConsole = false;
  Serial.println(F("== CAN console stopped =="));
}

bool consoleOn() { return gConsole; }
}
