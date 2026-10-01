#include "CanLink.h"
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
  gMcp.setBitrate(kBitrates[gProbeIndex].speed, MCP_CRYSTAL);
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
  gLocked = false;
  gLockedBps = 0;
  enterProbe(0);
  return true;
}

uint32_t lockedBitrate() { return gLockedBps; }
bool isLocked()          { return gLocked; }

void update() {
  const uint32_t now = millis();

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

  if (now - gLastRxMs >= kBusLostMs) {
    gDropouts++;
    gLocked = false;
    gLockedBps = 0;
    enterProbe(0);
  }
}

bool controllerPresent() { return gPresent; }
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
  return gMcp.sendMessage(&f) == MCP2515::ERROR_OK;
}

bool receive(struct can_frame* frame) {
  if (!gLocked) return false;
  if (gMcp.readMessage(frame) != MCP2515::ERROR_OK) return false;
  gLastRxMs = millis();
  gRxFrames++;
  return true;
}
}
