#include "feed_flow.h"

#include <algorithm>
#include <cstdlib>

namespace spine {

const char* stateName(State s) {
  switch (s) {
    case State::Idle: return "idle";
    case State::Identify: return "identify";
    case State::Lookup: return "lookup";
    case State::CheckBowlEmpty: return "check_bowl_empty";
    case State::Rotate: return "rotate";
    case State::Dispense: return "dispense";
    case State::Monitor: return "monitor";
    case State::Settle: return "settle";
  }
  return "?";
}

void FeedFlow::setState(State s) {
  if (onState) onState(s);
}

FlowResult FeedFlow::runManualFeed(const std::string& cmdKey, const PetCfg& pet,
                                   const std::string& ianaTz) {
  return run(cmdKey, pet, ianaTz, false);
}

FlowResult FeedFlow::runIdentifiedFeed(const PetCfg& pet, const std::string& ianaTz) {
  return run("", pet, ianaTz, true);
}

FlowResult FeedFlow::held(const std::string& cmdKey, const PetCfg& pet, Hold hold,
                          float bowlGrams) {
  const std::string reason = holdReason(hold);
  const bool bowlHold = hold == Hold::BowlNotEmpty || hold == Hold::PortionStillInBowl;
  const float reportedGrams = bowlHold ? std::max(0.0f, bowlGrams) : -1.0f;
  const std::string blockingPet = bowlHold && bowlOwner_.known() ? bowlOwner_.pet : "";
  if (!cmdKey.empty())
    cloud_->markOutcome(cmdKey, "held", reason, reportedGrams, blockingPet);
  setState(State::Idle);
  return {"held", reason, reportedGrams, blockingPet};
}

FlowResult FeedFlow::run(const std::string& cmdKey, const PetCfg& pet, const std::string& ianaTz,
                         bool gateOnSchedule) {
  const char* tz = posixTz(ianaTz);

  setState(State::Lookup);
  if (gateOnSchedule) {
    if (!eligibility(pet.schedule, tz, clock_->epochMs()))
      return held(cmdKey, pet, Hold::OutsideSchedule);
  }
  const double already = cloud_->dispensedToday(pet.id, localMidnightMs(tz, clock_->epochMs()));

  setState(State::CheckBowlEmpty);
  float bowlBefore = scale_->grams();
  if (bowlBefore < cfg_.emptyFloorG) {
    bowlOwner_.clear();
    scale_->tare();
    bowlBefore = scale_->grams();
  }
  const PortionDecision d = decidePortion(cfg_, bowlBefore, bowlOwner_, pet.id, pet.hopper,
                                          pet.targetGrams, pet.dailyAllotment, already);
  if (d.hold != Hold::None) return held(cmdKey, pet, d.hold, bowlBefore);

  setState(State::Rotate);
  disp_->rotateTo(atoi(pet.hopper.c_str()));

  setState(State::Dispense);
  scale_->setMode(ScaleMode::Dispense);
  int pulses = 0;
  while (scale_->grams() - bowlBefore < d.grams - cfg_.dispenseCutoffG &&
         pulses < cfg_.maxGatePulses) {
    disp_->gatePulse(cfg_.gatePulseMs);
    clock_->sleepMs(cfg_.pulseSettleMs);
    pulses++;
  }
  const float bowlAfterDispense = scale_->grams();
  const float dispensed = std::max(0.0f, bowlAfterDispense - bowlBefore);
  bowlOwner_ = {pet.id, pet.hopper, bowlAfterDispense};

  const std::string rowKey = cloud_->appendDispense(pet.id, dispensed, pet.hopper, clock_->epochMs());
  if (!cmdKey.empty()) cloud_->markOutcome(cmdKey, "done", "", dispensed, "");

  setState(State::Monitor);
  clock_->sleepMs(cfg_.monitorMs);

  setState(State::Settle);
  scale_->setMode(ScaleMode::Settle);
  const float remainder = scale_->grams();
  const float removedFromBowl = std::max(0.0f, bowlAfterDispense - remainder);
  const float eaten = std::min(dispensed, removedFromBowl);
  if (!rowKey.empty()) cloud_->recordEaten(pet.id, rowKey, eaten);

  if (remainder < cfg_.emptyFloorG)
    bowlOwner_.clear();
  else
    bowlOwner_ = {pet.id, pet.hopper, remainder};

  setState(State::Idle);
  return {"done", "", dispensed, ""};
}

}
