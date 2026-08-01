#include "hal_real.h"

#include <SPI.h>
#include <Wire.h>

#include <algorithm>
#include <cmath>

#include "diagnostics.h"

namespace {
constexpr int kSda = 8;
constexpr int kScl = 9;
constexpr int kStepperPins[4] = {39, 40, 41, 42};
constexpr uint8_t kHalfStep[8] = {0b1000, 0b1100, 0b0100, 0b0110,
                                  0b0010, 0b0011, 0b0001, 0b1001};
constexpr int kCameraSck = 12;
constexpr int kCameraMiso = 13;
constexpr int kCameraMosi = 11;
constexpr int kCameraCs = 10;
}

bool RealScale::begin() {
  Wire.begin(kSda, kScl);
  std::lock_guard<std::mutex> lock(mu_);
  if (!nau_.begin(&Wire)) return false;
  nau_.setLDO(NAU7802_3V0);
  nau_.setGain(NAU7802_GAIN_128);
  nau_.setRate(NAU7802_RATE_10SPS);
  for (int i = 0; i < 10; i++) {
    uint32_t started = millis();
    while (!nau_.available() && millis() - started < 250) delay(1);
    if (nau_.available()) nau_.read();
  }
  bool calibrated = false;
  for (int i = 0; i < 5 && !calibrated; i++) {
    calibrated = nau_.calibrate(NAU7802_CALMOD_INTERNAL);
    if (!calibrated) delay(100);
  }
  ready_ = calibrated;
  return ready_;
}

int32_t RealScale::readMedian(int samples) {
  if (!ready_) return lastRaw_;
  samples = std::max(1, std::min(samples, 31));
  int32_t values[31];
  for (int i = 0; i < samples; i++) {
    const uint32_t started = millis();
    while (!nau_.available() && millis() - started < 250) delay(1);
    values[i] = nau_.available() ? nau_.read() : lastRaw_;
  }
  std::sort(values, values + samples);
  lastRaw_ = values[samples / 2];
  return lastRaw_;
}

float RealScale::grams() {
  std::lock_guard<std::mutex> lock(mu_);
  const int samples = mode_ == spine::ScaleMode::Dispense ? 3 : 5;
  const int32_t value = readMedian(samples);
  if (!tared_ || cfg_->countsPerGram == 0.0f) return 0.0f;
  return static_cast<float>(value - tareRaw_) / cfg_->countsPerGram;
}

void RealScale::tare() {
  std::lock_guard<std::mutex> lock(mu_);
  tareRaw_ = readMedian(31);
  tared_ = true;
  Serial.printf("[hal:scale] tare raw=%ld\n", static_cast<long>(tareRaw_));
}

void RealScale::setMode(spine::ScaleMode mode) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!ready_ || mode == mode_) return;
  nau_.setRate(mode == spine::ScaleMode::Dispense ? NAU7802_RATE_80SPS : NAU7802_RATE_10SPS);
  mode_ = mode;
}

bool RealScale::calibrate(float knownGrams) {
  if (!ready_ || !tared_ || !std::isfinite(knownGrams) || knownGrams <= 0.0f) return false;
  std::lock_guard<std::mutex> lock(mu_);
  const int32_t value = readMedian(31);
  const float cpg = static_cast<float>(value - tareRaw_) / knownGrams;
  return cfg_->saveScale(cpg);
}

bool RealScale::stability(float* peakToPeakGrams) {
  if (!ready_ || !tared_ || cfg_->countsPerGram == 0.0f) return false;
  std::lock_guard<std::mutex> lock(mu_);
  int32_t lo = INT32_MAX;
  int32_t hi = INT32_MIN;
  for (int i = 0; i < 50; i++) {
    const int32_t value = readMedian(1);
    lo = std::min(lo, value);
    hi = std::max(hi, value);
  }
  *peakToPeakGrams = std::abs(static_cast<float>(hi - lo) / cfg_->countsPerGram);
  return spine::scaleReadingStable(*peakToPeakGrams);
}

void RealDispenser::begin() {
  for (int pin : kStepperPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

void RealDispenser::coilsWrite(uint8_t mask) {
  for (int i = 0; i < 4; i++) digitalWrite(kStepperPins[i], (mask >> (3 - i)) & 1);
}

void RealDispenser::release() {
  std::lock_guard<std::mutex> lock(mu_);
  coilsWrite(0);
}

void RealDispenser::stepBy(int32_t steps) {
  const int dir = steps >= 0 ? 1 : -1;
  const uint32_t count = static_cast<uint32_t>(std::abs(steps));
  for (uint32_t i = 0; i < count; i++) {
    phase_ = (phase_ + dir + 8) % 8;
    coilsWrite(kHalfStep[phase_]);
    delayMicroseconds(cfg_->stepIntervalUs);
    position_ += dir;
  }
  coilsWrite(0);
}

bool RealDispenser::jog(int32_t steps) {
  if (!spine::validDiagnosticJog(steps)) return false;
  std::lock_guard<std::mutex> lock(mu_);
  stepBy(steps);
  return true;
}

void RealDispenser::moveTo(int32_t target) { stepBy(target - position_); }

void RealDispenser::setHome() {
  std::lock_guard<std::mutex> lock(mu_);
  position_ = 0;
  selectedHopper_ = 0;
  homed_ = true;
}

void RealDispenser::clearHome() {
  std::lock_guard<std::mutex> lock(mu_);
  homed_ = false;
  selectedHopper_ = 0;
}

bool RealDispenser::saveHopper(int hopper, int32_t closedOffset, int32_t openOffset) {
  if (!spine::validHopperOffsets(hopper, closedOffset, openOffset)) return false;
  return cfg_->saveHopper(hopper, closedOffset, openOffset);
}

bool RealDispenser::setStepInterval(uint32_t intervalUs) {
  if (!spine::validStepInterval(intervalUs)) return false;
  return cfg_->saveStepInterval(intervalUs);
}

void RealDispenser::rotateTo(int hopper) {
  if (!homed_ || !configured() || hopper < 1 || hopper > 2) return;
  std::lock_guard<std::mutex> lock(mu_);
  moveTo(cfg_->hopperClosed[hopper - 1]);
  selectedHopper_ = hopper;
}

void RealDispenser::gatePulse(int ms) {
  if (!homed_ || !configured() || selectedHopper_ < 1 || selectedHopper_ > 2) return;
  std::lock_guard<std::mutex> lock(mu_);
  const int index = selectedHopper_ - 1;
  const uint32_t started = millis();
  do {
    moveTo(cfg_->hopperOpen[index]);
    moveTo(cfg_->hopperClosed[index]);
  } while (millis() - started < static_cast<uint32_t>(std::max(ms, 1)));
}

bool RealCamera::begin() {
  SPI.begin(kCameraSck, kCameraMiso, kCameraMosi, kCameraCs);
  std::lock_guard<std::mutex> lock(mu_);
  ready_ = camera_.begin() == CAM_ERR_SUCCESS;
  if (ready_) camera_.setAutoFocus(0x01);
  return ready_;
}

bool RealCamera::capture(std::vector<uint8_t>* jpeg) {
  if (!ready_) return false;
  std::lock_guard<std::mutex> lock(mu_);
  if (camera_.takePicture(CAM_IMAGE_MODE_QVGA, CAM_IMAGE_PIX_FMT_JPG) != CAM_ERR_SUCCESS)
    return false;
  const size_t length = camera_.getReceivedLength();
  if (length < 4 || length > 512 * 1024) return false;
  jpeg->clear();
  jpeg->reserve(length);
  while (camera_.getReceivedLength()) jpeg->push_back(camera_.readByte());
  return jpeg->size() >= 4 && (*jpeg)[0] == 0xff && (*jpeg)[1] == 0xd8;
}
