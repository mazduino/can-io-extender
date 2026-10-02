#include "CanLink.h"
#include "Settings.h"
#include <SPI.h>

namespace {
MCP2515 gMcp(PIN_CAN_CS);

struct BitrateOption {
  CAN_SPEED speed;
  uint32_t  bps;
};

const BitrateOption kBitrates[] = {
  {CAN_500KBPS, 500000},
  {CAN_250KBPS, 250000},
  {CAN_125KBPS, 125000},
#if MCP_SUPPORTS_1MBPS
  {CAN_1000KBPS, 1000000},
#endif
};
const uint8_t kBitrateCount = sizeof(kBitrates) / sizeof(kBitrates[0]);

const uint16_t kProbeMs = 300;

const uint16_t kBusLostMs = 3000;

const uint8_t kFramesToLock = 2;

bool     gLocked;
uint32_t gLockedBps;
uint8_t  gProbeIndex;
uint32_t gProbeStartedMs;
uint32_t gLastRxMs;
bool     gPresent;
uint8_t  gSweepRxErr;
uint8_t  gProbesDone;
uint8_t  gGoodFrames;
uint16_t gDropouts;
uint32_t gLockedAtMs;
uint32_t gRxFrames;
bool     gFixed;
uint32_t gTxFrames;
uint16_t gTxFails;
uint32_t gStatMs;
uint32_t gStatRx;
uint32_t gStatTx;
uint16_t gRxRate;
uint16_t gTxRate;
uint8_t  gRec;
uint8_t  gTec;

int8_t bitrateIndex(uint32_t bps) {
  for (uint8_t i = 0; i < kBitrateCount; i++) {
    if (kBitrates[i].bps == bps) return (int8_t)i;
  }
  return -1;
}

int8_t fixedIndex() {
  switch (settingsBitrate()) {
    case BITRATE_125K: return bitrateIndex(125000);
    case BITRATE_250K: return bitrateIndex(250000);
    case BITRATE_500K: return bitrateIndex(500000);
    case BITRATE_1M:   return bitrateIndex(1000000);
    default:           return -1;
  }
}

void updateStats(uint32_t now) {
  if (now - gStatMs < 1000) return;
  const uint32_t span = now - gStatMs;
  gRxRate = (uint16_t)((gRxFrames - gStatRx) * 1000UL / span);
  gTxRate = (uint16_t)((gTxFrames - gStatTx) * 1000UL / span);
  gStatRx = gRxFrames;
  gStatTx = gTxFrames;
  gStatMs = now;
  gRec = gPresent ? gMcp.errorCountRX() : 0;
  gTec = gPresent ? gMcp.errorCountTX() : 0;
}

bool frameIsPlausible(const struct can_frame &f) {
  if (f.can_dlc > 8) return false;
  if (f.can_id & CAN_EFF_FLAG) return true;
  return (f.can_id & 0x7FF) == f.can_id;
}

bool controllerStillThere() {
  return gMcp.setConfigMode() == MCP2515::ERROR_OK;
}

void enterProbe(uint8_t index) {
  gProbeIndex = index % kBitrateCount;
  if (gProbeIndex == 0) {
    gSweepRxErr = 0;
    gProbesDone = 0;
  }

  gMcp.reset();

  gPresent = (gMcp.setBitrate(kBitrates[gProbeIndex].speed, MCP_CRYSTAL)
              == MCP2515::ERROR_OK);

  gMcp.setListenOnlyMode();
  gProbeStartedMs = millis();
  gGoodFrames = 0;
  gProbesDone++;
}

void lockCurrent() {
  gMcp.reset();
  gPresent = (gMcp.setBitrate(kBitrates[gProbeIndex].speed, MCP_CRYSTAL)
              == MCP2515::ERROR_OK);
  gMcp.setNormalMode();
  gLocked = true;
  gLockedBps = kBitrates[gProbeIndex].bps;
  gLastRxMs = millis();
  gLockedAtMs = gLastRxMs;
  gRxFrames = 0;
}
}

namespace CanLink {
bool begin() {
  SPI.begin();
  restart();
  return true;
}

void restart() {
  gLocked = false;
  gLockedBps = 0;
  const int8_t fixed = fixedIndex();
  gFixed = fixed >= 0;
  if (gFixed) {
    gProbeIndex = (uint8_t)fixed;
    lockCurrent();
  } else {
    enterProbe(0);
  }
}

uint32_t lockedBitrate() { return gLockedBps; }
bool isLocked()          { return gLocked; }

void update() {
  const uint32_t now = millis();
  updateStats(now);

  if (!gLocked) {
    struct can_frame f;
    if (gMcp.readMessage(&f) == MCP2515::ERROR_OK && frameIsPlausible(f)) {
      if (++gGoodFrames >= kFramesToLock && controllerStillThere()) {
        lockCurrent();
      }
      return;
    }

    const uint8_t rxErr = gMcp.errorCountRX();
    if (rxErr > gSweepRxErr) gSweepRxErr = rxErr;

    if (now - gProbeStartedMs >= kProbeMs) {
      enterProbe(gProbeIndex + 1);
    }
    return;
  }

  if (!gFixed && now - gLastRxMs >= kBusLostMs) {
    gDropouts++;
    gLocked = false;
    gLockedBps = 0;
    enterProbe(0);
  }
}

bool controllerPresent() { return gPresent; }
uint16_t rxPerSecond()   { return gRxRate; }
uint16_t txPerSecond()   { return gTxRate; }
uint16_t txFailures()    { return gTxFails; }
uint8_t  rxErrorCount()  { return gRec; }
uint8_t  txErrorCount()  { return gTec; }
bool     fixedBitrate()  { return gFixed; }
uint16_t dropouts()      { return gDropouts; }
uint32_t rxFrames()      { return gRxFrames; }
uint32_t lockedForMs()   { return gLocked ? (millis() - gLockedAtMs) : 0; }
uint32_t probingBitrate() { return gLocked ? 0 : kBitrates[gProbeIndex].bps; }
uint8_t  sweepRxErrors()  { return gSweepRxErr; }

bool     sweepComplete()  { return gProbesDone >= kBitrateCount; }

bool send(uint16_t id, const uint8_t* data, uint8_t len) {
  if (!gLocked) return false;

  struct can_frame f;
  f.can_id = id;
  f.can_dlc = (len > 8) ? 8 : len;
  memset(f.data, 0, sizeof(f.data));
  memcpy(f.data, data, f.can_dlc);
  if (gMcp.sendMessage(&f) != MCP2515::ERROR_OK) {
    if (gTxFails < 0xFFFF) gTxFails++;
    return false;
  }
  gTxFrames++;
  return true;
}

bool receive(struct can_frame* frame) {
  if (!gLocked) return false;
  if (gMcp.readMessage(frame) != MCP2515::ERROR_OK) return false;
  gLastRxMs = millis();
  gRxFrames++;
  return true;
}
}
