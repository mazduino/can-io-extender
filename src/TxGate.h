#ifndef TX_GATE_H
#define TX_GATE_H

#include <Arduino.h>

struct TxGate {
  uint8_t  last[8];
  uint32_t lastTxMs;
  bool     primed;
};

void txGateInit(TxGate &g);

bool txGateExact(TxGate &g, const uint8_t *data, uint8_t cmpLen,
                 uint32_t now, uint16_t fastMs, uint16_t idleMs);

bool txGateU16(TxGate &g, const uint8_t *data, uint8_t pairs, uint16_t threshold,
               uint32_t now, uint16_t fastMs, uint16_t idleMs);

#endif
