#include <Arduino.h>
#include "Config.h"
#include "CanLink.h"
#include "Inputs.h"
#include "Outputs.h"
#include "Calibration.h"
#include "ConfigLink.h"
#include "TxGate.h"
#include "TsLink.h"
#include "Settings.h"

namespace {
uint32_t gLastFreqTx;
TxGate gGateA, gGateB, gGateC, gGateDigital, gGateCal, gGateCal2, gGateCal3;
uint32_t gBootMs;
uint32_t gLastStatusPrint;
bool gBannerPrinted;

inline void putU16(uint8_t* b, uint16_t v) {
  b[0] = (uint8_t)(v & 0xFF);
  b[1] = (uint8_t)(v >> 8);
}

void sendAnalogFrames(uint32_t now) {
  uint8_t d[8];

  putU16(&d[0], Inputs::batteryMv());
  putU16(&d[2], Inputs::analogMv(1));
  putU16(&d[4], Inputs::analogMv(2));
  putU16(&d[6], Inputs::analogMv(3));
  if (txGateU16(gGateA, d, 4, TX_ANALOG_THRESHOLD_MV, now,
                TX_FAST_MS, TX_ANALOG_IDLE_MS)) {
    CanLink::send(CAN_ID_ANALOG_A, d, 8);
  }

  putU16(&d[0], Inputs::analogMv(4));
  putU16(&d[2], Inputs::analogMv(5));
  putU16(&d[4], Inputs::analogMv(6));
  putU16(&d[6], Inputs::analogMv(7));
  if (txGateU16(gGateB, d, 4, TX_ANALOG_THRESHOLD_MV, now,
                TX_FAST_MS, TX_ANALOG_IDLE_MS)) {
    CanLink::send(CAN_ID_ANALOG_B, d, 8);
  }

  putU16(&d[0], Inputs::analogMv(8));
  putU16(&d[2], Inputs::analogMv(9));
  putU16(&d[4], Inputs::analogMv(10));
  putU16(&d[6], 0);
  if (txGateU16(gGateC, d, 3, TX_ANALOG_THRESHOLD_MV, now,
                TX_FAST_MS, TX_ANALOG_IDLE_MS)) {
    CanLink::send(CAN_ID_ANALOG_C, d, 8);
  }
}

void sendDigitalFrame(uint32_t now) {
  uint8_t d[8];
  d[0] = Inputs::digitalBits();
  d[1] = Inputs::diagBits();
  d[2] = Outputs::stateBits0();
  d[3] = Outputs::stateBits1();

  uint8_t flags = 0;
  if (Outputs::inFailsafe()) flags |= 0x01;
  if (CanLink::isLocked())   flags |= 0x02;
  d[4] = flags;

  const uint32_t up = (millis() - gBootMs) / 1000UL;
  putU16(&d[5], (up > 0xFFFF) ? 0xFFFF : (uint16_t)up);

  d[7] = (uint8_t)((FW_VERSION << 4) | settingsNode());

  if (txGateExact(gGateDigital, d, 5, now, TX_FAST_MS, TX_DIGITAL_IDLE_MS)) {
    CanLink::send(CAN_ID_DIGITAL, d, 8);
  }
}

void sendFreqFrame() {
  uint8_t d[8];
  for (uint8_t i = 0; i < 4; i++) {
    putU16(&d[i * 2], Inputs::hallDeciHz(i));
  }
  CanLink::send(CAN_ID_FREQ, d, 8);
}

void sendCalGroup(TxGate &gate, uint16_t id, uint8_t first, uint8_t count,
                  uint32_t now) {
  uint8_t d[8];
  memset(d, 0, sizeof(d));
  for (uint8_t i = 0; i < count; i++) putU16(&d[i * 2], (uint16_t)calibrationValue(first + i));
  if (txGateU16(gate, d, count, TX_CAL_THRESHOLD, now, TX_FAST_MS, TX_ANALOG_IDLE_MS)) {
    CanLink::send(id, d, 8);
  }
}

void sendCalFrames(uint32_t now) {
  sendCalGroup(gGateCal, CAN_ID_CAL, 0, 4, now);
  sendCalGroup(gGateCal2, CAN_ID_CAL2, 4, 4, now);
  sendCalGroup(gGateCal3, CAN_ID_CAL3, 8, 2, now);
}

void printBanner() {
  Serial.println(F("=== Mazduino CAN IO Extender ==="));
  Serial.print(F("Node "));
  Serial.print(settingsNode());
  Serial.print(F("  CAN ID 0x"));
  Serial.print(CAN_BASE_ID, HEX);
  Serial.print(F("-0x"));
  Serial.println(CAN_BASE_ID + 0x0F, HEX);
  Serial.print(F("MCP2515 crystal: "));
  Serial.println(MCP_SUPPORTS_1MBPS ? F("16 MHz (1 Mbps available)")
                                    : F("8 MHz (500 kbps max — Haltech bus not supported)"));
  Serial.print(F("MCP2515 over SPI: "));
  Serial.println(CanLink::controllerPresent() ? F("responding") : F("NO RESPONSE"));
}

void handleRx() {
  struct can_frame f;
  while (CanLink::receive(&f)) {
    if (f.can_id & CAN_EFF_FLAG) continue;

    if ((f.can_id & 0x7FF) == CAN_ID_OUTPUT_CMD && f.can_dlc >= 1) {
      uint8_t d[4] = {0, 0, 0, 0};
      memcpy(d, f.data, f.can_dlc < 4 ? f.can_dlc : 4);
      Outputs::applyCommand(d[0], d[1], d[2], d[3]);
    }
  }
}
}

void setup() {
  Serial.begin(115200);

  Outputs::begin();
  Inputs::begin();
  txGateInit(gGateA); txGateInit(gGateB); txGateInit(gGateC);
  txGateInit(gGateDigital); txGateInit(gGateCal);
  txGateInit(gGateCal2); txGateInit(gGateCal3);
  calibrationLoad();
  settingsLoad();
  ConfigLink::begin();
  TsLink::begin();
  CanLink::begin();

  gBootMs = millis();
}

void loop() {
  CanLink::update();
  Inputs::update();
  Outputs::update();
  ConfigLink::update();
  TsLink::update();
  handleRx();

  const uint32_t now = millis();

  if (CanLink::isLocked()) {
    sendAnalogFrames(now);
    sendDigitalFrame(now);
    sendCalFrames(now);

    if (now - gLastFreqTx >= TX_FREQ_INTERVAL_MS) {
      gLastFreqTx = now;
      sendFreqFrame();
    }
  }

  if (!TsLink::seen() && now - gLastStatusPrint >= 2000 &&
      now - TsLink::lastActivityMs() >= 3000) {
    gLastStatusPrint = now;
    if (!gBannerPrinted) {
      gBannerPrinted = true;
      printBanner();
    }
    if (CanLink::isLocked()) {
      Serial.print(F("Bus locked "));
      Serial.print(CanLink::lockedBitrate());
      Serial.print(F(" bps | Vbatt "));
      Serial.print(Inputs::batteryMv() / 1000.0f, 1);
      Serial.print(F(" V | IN 0b"));
      Serial.print(Inputs::digitalBits(), BIN);
      Serial.print(F(" | OUT 0x"));
      Serial.print(Outputs::stateBits0(), HEX);
      Serial.print(Outputs::inFailsafe() ? F(" (FAILSAFE)") : F(""));

      Serial.print(F(" | rx="));
      Serial.print(CanLink::rxFrames());
      Serial.print(F(" held="));
      Serial.print(CanLink::lockedForMs() / 1000);
      Serial.print(F("s drops="));
      Serial.println(CanLink::dropouts());
    } else if (!CanLink::controllerPresent()) {
      Serial.print(F("MCP2515 not responding over SPI. drops="));
      Serial.println(CanLink::dropouts());
      Serial.println(F("  Check CS on D9, the ICSP header (SCK/MOSI/MISO), and 5V/GND."));
    } else if (!CanLink::sweepComplete()) {
      Serial.print(F("Probing "));
      Serial.print(CanLink::probingBitrate());
      Serial.println(F(" bps..."));
    } else if (CanLink::sweepRxErrors() == 0) {
      Serial.println(F("MCP2515 OK, but no CAN activity at any bitrate."));
      Serial.println(F("  Check CANH/CANL are not swapped, 120R at both bus ends,"));
      Serial.println(F("  common ground, and that the other device is transmitting."));
    } else {
      Serial.print(F("CAN activity seen but no valid frame (rxErr="));
      Serial.print(CanLink::sweepRxErrors());
      Serial.println(F(")."));
      Serial.println(F("  Bitrate is outside the list, or Y1 is not 8 MHz."));
    }
  }
}
