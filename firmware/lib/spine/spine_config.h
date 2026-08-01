#pragma once

namespace spine {

struct SpineConfig {
  int mConfirm = 2;
  int nWindow = 3;
  float minBoxFrac = 0.25f;
  float matchThreshold = 0.20f;
  int maxIdentifyFrames = 12;

  float emptyFloorG = 5.0f;
  float allotFloorG = 5.0f;

  float dispenseCutoffG = 1.0f;
  int maxGatePulses = 30;
  int gatePulseMs = 150;
  int pulseSettleMs = 300;

  int monitorMs = 6000;

  int heartbeatMs = 15000;
};

}
