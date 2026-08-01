#pragma once
#include <algorithm>
#include <string>

#include "spine_config.h"

namespace spine {

enum class Hold {
  None,
  BowlNotEmpty,
  PortionStillInBowl,
  AllotmentReached,
  OutsideSchedule,
  Offline,
};

inline const char* holdReason(Hold h) {
  switch (h) {
    case Hold::BowlNotEmpty: return "bowl_not_empty";
    case Hold::PortionStillInBowl: return "portion_in_bowl";
    case Hold::AllotmentReached: return "allotment_reached";
    case Hold::OutsideSchedule: return "outside_schedule";
    case Hold::Offline: return "offline";
    default: return "";
  }
}

struct BowlOwner {
  std::string pet;
  std::string hopper;
  float grams = 0.0f;

  bool known() const { return !pet.empty() && !hopper.empty(); }
  void clear() {
    pet.clear();
    hopper.clear();
    grams = 0.0f;
  }
};

struct PortionDecision {
  Hold hold = Hold::None;
  float grams = 0.0f;
};

inline PortionDecision decidePortion(const SpineConfig& cfg, float bowlGrams,
                                     const BowlOwner& owner, const std::string& candidatePet,
                                     const std::string& candidateHopper, float targetGrams,
                                     float dailyAllotment, double alreadyDispensedToday) {
  const bool residual = bowlGrams >= cfg.emptyFloorG;
  float portionRemaining = targetGrams;
  if (residual) {
    if (!owner.known() || owner.pet != candidatePet || owner.hopper != candidateHopper)
      return {Hold::BowlNotEmpty, 0.0f};
    portionRemaining = targetGrams - bowlGrams;
    if (portionRemaining <= cfg.dispenseCutoffG)
      return {Hold::PortionStillInBowl, 0.0f};
  }

  if (alreadyDispensedToday < 0) return {Hold::Offline, 0.0f};
  const float remaining = dailyAllotment - static_cast<float>(alreadyDispensedToday);
  if (remaining < cfg.allotFloorG) return {Hold::AllotmentReached, 0.0f};

  const float grams = std::min(portionRemaining, remaining);
  if (grams <= 0.0f) return {Hold::PortionStillInBowl, 0.0f};
  return {Hold::None, grams};
}

}
