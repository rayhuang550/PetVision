#include "vision_client.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <mbedtls/base64.h>

#include "psram_json.h"
#include "secrets.h"

namespace {
int postJpeg(const char* pathQuery, const uint8_t* jpeg, size_t len, String* resp) {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(20000);
  http.begin(client, String("http://") + VISION_HOST + ":" + VISION_PORT + pathQuery);
  http.addHeader("Content-Type", "image/jpeg");
  const int code = http.POST(const_cast<uint8_t*>(jpeg), len);
  if (resp) *resp = http.getString();
  http.end();
  return code;
}
}

spine::EmbedResult VisionClient::embed(const uint8_t* jpeg, size_t len) {
  spine::EmbedResult r;
  String resp;
  const int code = postJpeg("/embed", jpeg, len, &resp);
  if (code != 200) {
    Serial.printf("[vision] /embed -> %d\n", code);
    return r;
  }
  JsonDocument doc(PsramAllocator::instance());
  if (deserializeJson(doc, resp)) return r;
  r.ok = true;
  r.requestId = doc["request_id"] | "";
  r.petCount = doc["pet_count"] | 0;
  if (r.petCount == 1) {
    JsonObject det = doc["detections"][0];
    for (JsonVariant v : det["embedding"].as<JsonArray>()) r.embedding.push_back(v.as<float>());
    r.boxHeightFrac = det["box"][3] | 0.0f;
  }
  return r;
}

namespace {
std::vector<uint8_t> extractCrop(const String& resp, int from) {
  std::vector<uint8_t> out;
  const int key = resp.indexOf("\"crop_image\"", from < 0 ? 0 : from);
  if (key < 0) return out;
  const int colon = resp.indexOf(':', key);
  if (colon < 0) return out;
  int p = colon + 1;
  while (p < static_cast<int>(resp.length()) && resp[p] == ' ') p++;
  if (p >= static_cast<int>(resp.length()) || resp[p] != '"') return out;
  const int start = p + 1;
  const int end = resp.indexOf('"', start);
  if (end <= start) return out;

  size_t outLen = 0;
  const unsigned char* src = reinterpret_cast<const unsigned char*>(resp.c_str()) + start;
  mbedtls_base64_decode(nullptr, 0, &outLen, src, end - start);
  out.resize(outLen);
  if (mbedtls_base64_decode(out.data(), out.size(), &outLen, src, end - start) != 0) out.clear();
  out.resize(outLen);
  return out;
}
}

EnrollEmbed VisionClient::embedForEnroll(const uint8_t* jpeg, size_t len) {
  EnrollEmbed r;
  String resp;
  const int code = postJpeg("/embed?crops=1", jpeg, len, &resp);
  if (code != 200) {
    Serial.printf("[vision] /embed?crops=1 -> %d\n", code);
    return r;
  }

  JsonDocument filter;
  filter["pet_count"] = true;
  filter["detections"][0]["embedding"] = true;
  filter["whole_frame"]["embedding"] = true;
  JsonDocument doc(PsramAllocator::instance());
  const auto err = deserializeJson(doc, resp, DeserializationOption::Filter(filter));
  if (err != DeserializationError::Ok) {
    Serial.printf("[vision] /embed?crops=1 parse %s (resp %u bytes)\n", err.c_str(),
                  static_cast<unsigned>(resp.length()));
    return r;
  }

  r.petCount = doc["pet_count"] | 0;
  if (r.petCount == 1) {
    for (JsonVariant v : doc["detections"][0]["embedding"].as<JsonArray>())
      r.embedding.push_back(v.as<float>());
    r.cropJpeg = extractCrop(resp, 0);
  } else {
    for (JsonVariant v : doc["whole_frame"]["embedding"].as<JsonArray>())
      r.embedding.push_back(v.as<float>());
    r.cropJpeg = extractCrop(resp, resp.indexOf("\"whole_frame\""));
  }
  r.ok = !r.embedding.empty();
  return r;
}
