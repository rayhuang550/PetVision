#pragma once
#include <functional>
#include <string>
#include <vector>

#include "gates.h"
#include "local_time.h"
#include "ports.h"
#include "spine_config.h"

namespace spine {

enum class State { Idle, Identify, Lookup, CheckBowlEmpty, Rotate, Dispense, Monitor, Settle };
const char* stateName(State s);

struct PetCfg {
  std::string id;
  std::string name;
  std::string hopper;
  float targetGrams = 0.0f;
  float dailyAllotment = 0.0f;
  std::vector<Window> schedule;
};

struct FlowResult {
  std::string status;
  std::string reason;
  float grams = 0.0f;
  std::string blockingPet;
};

class FeedFlow {
 public:
  FeedFlow(IDispenser* disp, IScale* scale, ICloudOps* cloud, IClock* clock, const SpineConfig& cfg)
      : disp_(disp), scale_(scale), cloud_(cloud), clock_(clock), cfg_(cfg) {}

  FlowResult runManualFeed(const std::string& cmdKey, const PetCfg& pet, const std::string& ianaTz);

  FlowResult runIdentifiedFeed(const PetCfg& pet, const std::string& ianaTz);

  const BowlOwner& bowlOwner() const { return bowlOwner_; }

  std::function<void(State)> onState;

 private:
  FlowResult run(const std::string& cmdKey, const PetCfg& pet, const std::string& ianaTz,
                 bool gateOnSchedule);
  FlowResult held(const std::string& cmdKey, const PetCfg& pet, Hold hold, float bowlGrams = -1.0f);
  void setState(State s);

  IDispenser* disp_;
  IScale* scale_;
  ICloudOps* cloud_;
  IClock* clock_;
  SpineConfig cfg_;
  BowlOwner bowlOwner_;
};

}
