#include "gallery.h"

#include <ArduinoJson.h>

namespace spine {

void Gallery::add(const std::string& path, const std::string& pet, std::vector<float> embedding) {
  entries_[path] = Entry{pet, std::move(embedding)};
}

void Gallery::prune(const std::vector<std::string>& paths) {
  for (const auto& p : paths) entries_.erase(p);
}

Gallery::Match Gallery::match(const std::vector<float>& embedding) const {
  Match best;
  for (const auto& [path, e] : entries_) {
    if (e.embedding.size() != embedding.size()) continue;
    float dot = 0.0f;
    for (size_t i = 0; i < embedding.size(); i++) dot += embedding[i] * e.embedding[i];
    if (dot > best.sim || best.pet.empty()) {
      best.pet = e.pet;
      best.sim = dot;
    }
  }
  return best;
}

std::string Gallery::toJson() const {
  JsonDocument doc;
  for (const auto& [path, e] : entries_) {
    JsonObject o = doc[path].to<JsonObject>();
    o["pet"] = e.pet;
    JsonArray emb = o["embedding"].to<JsonArray>();
    for (float v : e.embedding) emb.add(v);
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool Gallery::fromJson(const std::string& blob) {
  JsonDocument doc;
  if (deserializeJson(doc, blob) != DeserializationError::Ok) return false;
  entries_.clear();
  for (JsonPair kv : doc.as<JsonObject>()) {
    Entry e;
    JsonObject o = kv.value().as<JsonObject>();
    e.pet = o["pet"].as<std::string>();
    for (JsonVariant v : o["embedding"].as<JsonArray>()) e.embedding.push_back(v.as<float>());
    entries_[kv.key().c_str()] = std::move(e);
  }
  return true;
}

}
