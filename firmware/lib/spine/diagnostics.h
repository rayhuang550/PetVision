#pragma once

#include <cstdint>
#include <cstdlib>

namespace spine {

inline bool validDiagnosticJog(int32_t steps) {
  const int32_t magnitude = std::abs(steps);
  return magnitude == 8 || magnitude == 64 || magnitude == 256;
}

inline bool validStepInterval(uint32_t microseconds) {
  return microseconds == 1200 || microseconds == 2000 || microseconds == 3000;
}

inline bool validHopperOffsets(int hopper, int32_t closedOffset, int32_t openOffset) {
  return hopper >= 1 && hopper <= 2 && closedOffset != openOffset &&
         std::abs(openOffset - closedOffset) <= 512;
}

constexpr float kScaleStabilityLimitG = 1.0f;

inline bool scaleReadingStable(float peakToPeakGrams) {
  return peakToPeakGrams <= kScaleStabilityLimitG;
}

struct HardwareReadiness {
  bool scaleAvailable = false;
  bool scaleCalibrated = false;
  bool scaleTared = false;
  bool scaleStable = false;
  bool cameraAvailable = false;
  bool cameraChecked = false;
  bool visionChecked = false;
  bool hopperOffsets = false;
  bool homed = false;
  bool stepperConfirmed = false;
};

inline const char* hardwareReadinessReason(const HardwareReadiness& r) {
  if (!r.scaleAvailable) return "scale_unavailable";
  if (!r.scaleCalibrated) return "scale_uncalibrated";
  if (!r.scaleTared) return "scale_not_tared";
  if (!r.scaleStable) return "scale_unstable";
  if (!r.cameraAvailable) return "camera_unavailable";
  if (!r.cameraChecked) return "camera_not_checked";
  if (!r.visionChecked) return "vision_not_checked";
  if (!r.hopperOffsets) return "hopper_offsets_missing";
  if (!r.homed) return "home_required";
  if (!r.stepperConfirmed) return "stepper_not_confirmed";
  return "";
}

}
