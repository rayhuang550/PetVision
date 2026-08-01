#pragma once
#include <deque>
#include <string>
#include <vector>

#include "spine_config.h"

namespace spine {

struct FrameObs {
  int petCount = 0;
  float boxHeightFrac = 0.0f;
  std::vector<float> embedding;
  std::string topPet;
  float topSim = 0.0f;
};

class IdentityWindow {
 public:
  explicit IdentityWindow(const SpineConfig& cfg) : cfg_(cfg) {}

  void append(const FrameObs& f);
  void reset() { win_.clear(); }

  bool candidate(std::string* pet, std::vector<float>* avgEmbedding) const;

  size_t size() const { return win_.size(); }

 private:
  bool confirms(const FrameObs& f) const;

  SpineConfig cfg_;
  std::deque<FrameObs> win_;
};

}
