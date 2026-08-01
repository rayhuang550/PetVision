#include "hardware_config.h"

#include <cmath>

namespace {
constexpr const char* kNamespace = "petvision_hw";
}

bool HardwareConfig::open(Preferences* prefs, bool readOnly) {
  return prefs->begin(kNamespace, readOnly);
}

bool HardwareConfig::load() {
  Preferences prefs;
  if (!open(&prefs, true)) return false;
  const uint32_t schema = prefs.getUInt("schema", 0);
  if (schema == kSchemaVersion) {
    countsPerGram = prefs.getFloat("scale_cpg", 0.0f);
    stepIntervalUs = prefs.getUInt("step_us", 2000);
    hopperConfigured[0] = prefs.getBool("h1_ok", false);
    hopperConfigured[1] = prefs.getBool("h2_ok", false);
    hopperClosed[0] = prefs.getInt("h1_closed", 0);
    hopperOpen[0] = prefs.getInt("h1_open", 0);
    hopperClosed[1] = prefs.getInt("h2_closed", 2048);
    hopperOpen[1] = prefs.getInt("h2_open", 2048);
  }
  prefs.end();
  return schema == kSchemaVersion;
}

bool HardwareConfig::saveScale(float value) {
  if (!std::isfinite(value) || value == 0.0f) return false;
  Preferences prefs;
  if (!open(&prefs, false)) return false;
  prefs.putUInt("schema", kSchemaVersion);
  const bool ok = prefs.putFloat("scale_cpg", value) == sizeof(float);
  prefs.end();
  if (ok) countsPerGram = value;
  return ok;
}

bool HardwareConfig::saveStepInterval(uint32_t value) {
  if (value != 1200 && value != 2000 && value != 3000) return false;
  Preferences prefs;
  if (!open(&prefs, false)) return false;
  prefs.putUInt("schema", kSchemaVersion);
  const bool ok = prefs.putUInt("step_us", value) == sizeof(uint32_t);
  prefs.end();
  if (ok) stepIntervalUs = value;
  return ok;
}

bool HardwareConfig::saveHopper(int hopper, int32_t closedOffset, int32_t openOffset) {
  if (hopper < 1 || hopper > 2 || closedOffset == openOffset) return false;
  Preferences prefs;
  if (!open(&prefs, false)) return false;
  prefs.putUInt("schema", kSchemaVersion);
  const char* okKey = hopper == 1 ? "h1_ok" : "h2_ok";
  const char* closedKey = hopper == 1 ? "h1_closed" : "h2_closed";
  const char* openKey = hopper == 1 ? "h1_open" : "h2_open";
  const bool ok = prefs.putInt(closedKey, closedOffset) == sizeof(int32_t) &&
                  prefs.putInt(openKey, openOffset) == sizeof(int32_t) &&
                  prefs.putBool(okKey, true) == 1;
  prefs.end();
  if (ok) {
    hopperConfigured[hopper - 1] = true;
    hopperClosed[hopper - 1] = closedOffset;
    hopperOpen[hopper - 1] = openOffset;
  }
  return ok;
}
