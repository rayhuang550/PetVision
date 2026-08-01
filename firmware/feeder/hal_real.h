#pragma once

#include <Adafruit_NAU7802.h>
#include <Arducam_Mega.h>
#include <Arduino.h>

#include <mutex>
#include <vector>

#include "hardware_config.h"
#include "ports.h"

class RealScale : public spine::IScale {
 public:
  explicit RealScale(HardwareConfig* cfg) : cfg_(cfg) {}

  bool begin();
  float grams() override;
  void tare() override;
  void setMode(spine::ScaleMode mode) override;
  bool calibrate(float knownGrams);
  bool stability(float* peakToPeakGrams);
  bool ready() const { return ready_; }
  bool tared() const { return tared_; }
  int32_t raw() const { return lastRaw_; }

 private:
  int32_t readMedian(int samples);

  HardwareConfig* cfg_;
  Adafruit_NAU7802 nau_;
  mutable std::mutex mu_;
  bool ready_ = false;
  bool tared_ = false;
  int32_t tareRaw_ = 0;
  int32_t lastRaw_ = 0;
  spine::ScaleMode mode_ = spine::ScaleMode::Settle;
};

class RealDispenser : public spine::IDispenser {
 public:
  explicit RealDispenser(HardwareConfig* cfg) : cfg_(cfg) {}

  void begin();
  void rotateTo(int hopper) override;
  void gatePulse(int ms) override;

  bool jog(int32_t steps);
  void release();
  void setHome();
  void clearHome();
  bool saveHopper(int hopper, int32_t closedOffset, int32_t openOffset);
  bool setStepInterval(uint32_t intervalUs);
  bool homed() const { return homed_; }
  bool configured() const { return cfg_->dispenserConfigured(); }
  int32_t position() const { return position_; }

 private:
  void moveTo(int32_t target);
  void stepBy(int32_t steps);
  void coilsWrite(uint8_t mask);

  HardwareConfig* cfg_;
  std::mutex mu_;
  int phase_ = 0;
  int32_t position_ = 0;
  int selectedHopper_ = 0;
  bool homed_ = false;
};

class RealCamera {
 public:
  RealCamera() : camera_(10) {}
  bool begin();
  bool capture(std::vector<uint8_t>* jpeg);
  bool ready() const { return ready_; }

 private:
  Arducam_Mega camera_;
  std::mutex mu_;
  bool ready_ = false;
};
