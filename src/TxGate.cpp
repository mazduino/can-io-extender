#include "TxGate.h"

namespace {
bool decide(TxGate &g, bool changed, const uint8_t *data,
            uint32_t now, uint16_t fastMs, uint16_t idleMs) {
  if (!g.primed) {
    g.primed = true;
    memcpy(g.last, data, 8);
    g.lastTxMs = now;
    return true;
  }

  const uint32_t since = now - g.lastTxMs;
  const bool due = changed ? (since >= fastMs) : (since >= idleMs);
  if (!due) return false;

  memcpy(g.last, data, 8);
  g.lastTxMs = now;
  return true;
}
}

void txGateInit(TxGate &g) {
  memset(g.last, 0, sizeof(g.last));
  g.lastTxMs = 0;
  g.primed = false;
}

bool txGateExact(TxGate &g, const uint8_t *data, uint8_t cmpLen,
                 uint32_t now, uint16_t fastMs, uint16_t idleMs) {
  if (cmpLen > 8) cmpLen = 8;
  const bool changed = memcmp(g.last, data, cmpLen) != 0;
  return decide(g, changed, data, now, fastMs, idleMs);
}

bool txGateU16(TxGate &g, const uint8_t *data, uint8_t pairs, uint16_t threshold,
               uint32_t now, uint16_t fastMs, uint16_t idleMs) {
  if (pairs > 4) pairs = 4;
  bool changed = false;
  for (uint8_t i = 0; i < pairs && !changed; i++) {
    const uint16_t a = (uint16_t)g.last[i * 2] | ((uint16_t)g.last[i * 2 + 1] << 8);
    const uint16_t b = (uint16_t)data[i * 2]   | ((uint16_t)data[i * 2 + 1] << 8);
    const uint16_t d = (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
    if (d >= threshold) changed = true;
  }
  return decide(g, changed, data, now, fastMs, idleMs);
}
