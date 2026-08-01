#include "identity_window.h"

#include <cmath>

namespace spine {

bool IdentityWindow::confirms(const FrameObs& f) const {
  return f.petCount == 1 && !f.topPet.empty() && f.topSim >= cfg_.matchThreshold &&
         f.boxHeightFrac >= cfg_.minBoxFrac;
}

void IdentityWindow::append(const FrameObs& f) {
  if (f.petCount >= 2) {
    win_.clear();
    return;
  }
  win_.push_back(f);
  while (win_.size() > static_cast<size_t>(cfg_.nWindow)) win_.pop_front();
}

bool IdentityWindow::candidate(std::string* pet, std::vector<float>* avgEmbedding) const {
  for (const auto& f : win_) {
    if (!confirms(f)) continue;
    int count = 0;
    for (const auto& g : win_)
      if (confirms(g) && g.topPet == f.topPet) count++;
    if (count < cfg_.mConfirm) continue;

    std::vector<float> avg(f.embedding.size(), 0.0f);
    int used = 0;
    for (const auto& g : win_) {
      if (!(confirms(g) && g.topPet == f.topPet) || g.embedding.size() != avg.size()) continue;
      for (size_t i = 0; i < avg.size(); i++) avg[i] += g.embedding[i];
      used++;
    }
    if (used == 0 || avg.empty()) return false;
    float norm = 0.0f;
    for (float v : avg) norm += v * v;
    norm = std::sqrt(norm);
    if (norm <= 0.0f) return false;
    for (float& v : avg) v /= norm;

    *pet = f.topPet;
    *avgEmbedding = std::move(avg);
    return true;
  }
  return false;
}

}
