#pragma once
#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spine {
struct EmbedResult {
  bool ok = false;
  int petCount = 0;
  std::vector<float> embedding;
  float boxHeightFrac = 0.0f;
  std::string requestId;
};
}

struct EnrollEmbed {
  bool ok = false;
  int petCount = 0;
  std::vector<float> embedding;
  std::vector<uint8_t> cropJpeg;
};

class VisionClient {
 public:
  spine::EmbedResult embed(const uint8_t* jpeg, size_t len);

  EnrollEmbed embedForEnroll(const uint8_t* jpeg, size_t len);
};
