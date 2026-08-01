#pragma once
#include <cstdint>
#include <string>

namespace spine {

class IDispenser {
 public:
  virtual ~IDispenser() = default;
  virtual void rotateTo(int hopper) = 0;
  virtual void gatePulse(int ms) = 0;
};

enum class ScaleMode { Settle, Dispense };

class IScale {
 public:
  virtual ~IScale() = default;
  virtual float grams() = 0;
  virtual void tare() = 0;
  virtual void setMode(ScaleMode) {}
};

class ICloudOps {
 public:
  virtual ~ICloudOps() = default;
  virtual std::string appendDispense(const std::string& pet, float dispensed,
                                     const std::string& hopper, int64_t tsMs) = 0;
  virtual bool recordEaten(const std::string& pet, const std::string& rowKey, float eaten) = 0;
  virtual bool markOutcome(const std::string& cmdKey, const std::string& status,
                           const std::string& reason, float grams,
                           const std::string& blockingPet = "") = 0;
  virtual double dispensedToday(const std::string& pet, int64_t localMidnightMs) = 0;
};

class IClock {
 public:
  virtual ~IClock() = default;
  virtual int64_t epochMs() = 0;
  virtual void sleepMs(uint32_t ms) = 0;
};

}
