#pragma once

#include <Preferences.h>

#include <cstdint>

struct HardwareConfig {
  static constexpr uint32_t kSchemaVersion = 1;

  float countsPerGram = 0.0f;
  uint32_t stepIntervalUs = 2000;
  int32_t hopperClosed[2] = {0, 2048};
  int32_t hopperOpen[2] = {0, 2048};
  bool hopperConfigured[2] = {false, false};

  bool load();
  bool saveScale(float value);
  bool saveStepInterval(uint32_t value);
  bool saveHopper(int hopper, int32_t closedOffset, int32_t openOffset);
  bool scaleCalibrated() const { return countsPerGram != 0.0f; }
  bool dispenserConfigured() const {
    return hopperConfigured[0] && hopperConfigured[1];
  }

 private:
  bool open(Preferences* prefs, bool readOnly);
};
