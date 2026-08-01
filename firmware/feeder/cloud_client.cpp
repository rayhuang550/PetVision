#include "cloud_client.h"

#include "cloud_endpoints.h"
#include "psram_json.h"
#include "secrets.h"

namespace {

String i64(int64_t v) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
  return buf;
}

}

String CloudClient::token() {
  std::lock_guard<std::mutex> lock(authMu_);
  return idToken_;
}

void CloudClient::begin() {
  configureCloudTransport(transport_);
  http_.setReuse(true);
}

bool CloudClient::signInLocked() {
  JsonDocument body;
  body["email"] = DEVICE_EMAIL;
  body["password"] = DEVICE_PASSWORD;
  body["returnSecureToken"] = true;
  String payload;
  serializeJson(body, payload);

  http_.setTimeout(15000);
  http_.begin(transport_, endpoints::authSignInUrl());
  http_.addHeader("Content-Type", "application/json");
  const int code = http_.POST(payload);
  if (code != 200) {
    Serial.printf("[cloud] sign-in failed: HTTP %d %s\n", code, http_.getString().c_str());
    http_.end();
    return false;
  }
  JsonDocument resp;
  const String body_ = http_.getString();
  const auto err = deserializeJson(resp, body_);
  http_.end();
  if (err) {
    Serial.printf("[cloud] sign-in parse error: %s\n", err.c_str());
    return false;
  }
  const String tok = resp["idToken"].as<String>();
  const long expiresIn = atol(resp["expiresIn"] | "3600");
  {
    std::lock_guard<std::mutex> lock(authMu_);
    idToken_ = tok;
    tokenExpiryMs_ = static_cast<int64_t>(time(nullptr)) * 1000 + (expiresIn - 300) * 1000L;
  }
  Serial.printf("[cloud] signed in as device %s\n", DEVICE_EMAIL);
  return tok.length() > 0;
}

bool CloudClient::signIn() {
  std::lock_guard<std::mutex> tx(txMu_);
  return signInLocked();
}

bool CloudClient::ensureAuthLocked() {
  {
    std::lock_guard<std::mutex> lock(authMu_);
    if (idToken_.length() && static_cast<int64_t>(time(nullptr)) * 1000 < tokenExpiryMs_)
      return true;
  }
  return signInLocked();
}

String CloudClient::rtdbUrl(const String& path, const String& extraQuery) {
  return endpoints::rtdbUrl(path, token(), extraQuery);
}

int CloudClient::request(const char* method, const std::function<String()>& urlFn,
                         const String& body, String* resp, const char* contentType,
                         bool bearerAuth) {
  std::lock_guard<std::mutex> tx(txMu_);
  String url;
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!ensureAuthLocked()) return -1;
    url = urlFn();
    http_.setTimeout(10000);
    http_.begin(transport_, url);
    if (body.length() || strcmp(method, "GET") != 0) http_.addHeader("Content-Type", contentType);
    if (bearerAuth) http_.addHeader("Authorization", String("Bearer ") + token());
    const int code = http_.sendRequest(
        method, reinterpret_cast<uint8_t*>(const_cast<char*>(body.c_str())), body.length());
    if (code == 401 && attempt == 0) {
      http_.end();
      std::lock_guard<std::mutex> lock(authMu_);
      idToken_ = "";
      continue;
    }
    if (resp) *resp = http_.getString();
    http_.end();
    if (verbose) {
      String path = url.substring(url.indexOf('/', 8));
      const int q = path.indexOf('?');
      if (q > 0) path = path.substring(0, q);
      Serial.printf("[cloud] %s %s -> %d\n", method, path.c_str(), code);
    }
    return code;
  }
  return -1;
}

std::string CloudClient::appendDispense(const std::string& pet, float dispensed,
                                        const std::string& hopper, int64_t tsMs) {
  JsonDocument doc;
  doc["ts"] = tsMs;
  doc["dispensed"] = dispensed;
  doc["hopper"] = hopper;
  String body;
  serializeJson(doc, body);
  String resp;
  if (request("POST", [&] { return rtdbUrl(("log/" + pet).c_str()); }, body, &resp) != 200) return "";
  JsonDocument r;
  if (deserializeJson(r, resp)) return "";
  return r["name"].as<std::string>();
}

bool CloudClient::recordEaten(const std::string& pet, const std::string& rowKey, float eaten) {
  JsonDocument doc;
  doc["eaten"] = eaten;
  String body;
  serializeJson(doc, body);
  return request("PATCH", [&] { return rtdbUrl(("log/" + pet + "/" + rowKey).c_str()); }, body, nullptr) == 200;
}

bool CloudClient::markOutcome(const std::string& cmdKey, const std::string& status,
                              const std::string& reason, float grams,
                              const std::string& blockingPet) {
  JsonDocument doc;
  doc["status"] = status;
  if (!reason.empty()) doc["reason"] = reason;
  if (grams >= 0) doc["grams"] = grams;
  if (!blockingPet.empty()) doc["blocking_pet"] = blockingPet;
  String body;
  serializeJson(doc, body);
  return request("PATCH", [&] { return rtdbUrl(("commands/" + cmdKey).c_str()); }, body, nullptr) == 200;
}

bool CloudClient::markOutcomeResult(const std::string& cmdKey, JsonArray result) {
  JsonDocument doc;
  doc["status"] = "done";
  doc["result"] = result;
  String body;
  serializeJson(doc, body);
  return request("PATCH", [&] { return rtdbUrl(("commands/" + cmdKey).c_str()); }, body, nullptr) == 200;
}

double CloudClient::dispensedToday(const std::string& pet, int64_t localMidnightMs) {
  const String q = String("orderBy=%22ts%22&startAt=") + i64(localMidnightMs);
  String resp;
  if (request("GET", [&] { return rtdbUrl(("log/" + pet).c_str(), q); }, "", &resp) != 200) return -1;
  JsonDocument doc(PsramAllocator::instance());
  if (deserializeJson(doc, resp)) return -1;
  double total = 0;
  for (JsonPair kv : doc.as<JsonObject>()) total += kv.value()["dispensed"] | 0.0;
  return total;
}

bool CloudClient::getTimezone(std::string* ianaTz) {
  String resp;
  if (request("GET", [&] { return rtdbUrl("config/timezone"); }, "", &resp) != 200) return false;
  JsonDocument doc;
  if (deserializeJson(doc, resp) || doc.isNull()) {
    *ianaTz = "America/Toronto";
    return true;
  }
  *ianaTz = doc.as<std::string>();
  return true;
}

bool CloudClient::getPet(const std::string& id, spine::PetCfg* out, bool* exists) {
  String resp;
  if (request("GET", [&] { return rtdbUrl(("pets/" + id).c_str()); }, "", &resp) != 200) return false;
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;
  if (doc.isNull()) {
    *exists = false;
    return true;
  }
  *exists = true;
  out->id = id;
  out->name = doc["name"] | "";
  if (doc["hopper"].is<const char*>())
    out->hopper = doc["hopper"].as<std::string>();
  else
    out->hopper = std::to_string(doc["hopper"] | 0);
  out->targetGrams = doc["target_grams"] | 0.0f;
  out->dailyAllotment = doc["daily_allotment"] | 0.0f;
  out->schedule.clear();
  for (JsonObject w : doc["schedule"].as<JsonArray>())
    out->schedule.push_back({spine::toMin(w["start"] | "00:00"), spine::toMin(w["end"] | "00:00")});
  return true;
}

bool CloudClient::getPendingCommands(std::vector<PendingCommand>* out) {
  String resp;
  if (request("GET", [&] { return rtdbUrl("commands", "orderBy=%22status%22&equalTo=%22pending%22"); }, "",
              &resp) !=
      200)
    return false;
  JsonDocument doc(PsramAllocator::instance());
  if (deserializeJson(doc, resp)) return false;
  out->clear();
  for (JsonPair kv : doc.as<JsonObject>()) {
    PendingCommand c;
    c.key = kv.key().c_str();
    c.type = kv.value()["type"] | "";
    c.pet = kv.value()["pet"] | "";
    for (JsonVariant p : kv.value()["photos"].as<JsonArray>())
      c.photos.push_back(p.as<std::string>());
    out->push_back(std::move(c));
  }
  return true;
}

bool CloudClient::heartbeat(const char* state, int64_t tsMs, bool ready,
                            const char* readinessReason) {
  JsonDocument doc;
  doc["online"] = true;
  doc["ts"] = tsMs;
  doc["state"] = state;
  doc["ready"] = ready;
  if (ready || !readinessReason || !readinessReason[0])
    doc["readiness_reason"] = nullptr;
  else
    doc["readiness_reason"] = readinessReason;
  String body;
  serializeJson(doc, body);
  const bool wasVerbose = verbose;
  verbose = false;
  const bool ok = request("PATCH", [&] { return rtdbUrl("status/feeder"); }, body, nullptr) == 200;
  verbose = wasVerbose;
  return ok;
}

bool CloudClient::publishHold(const std::string& reason, const std::string& blockingPet, float grams,
                              int64_t tsMs) {
  JsonDocument doc;
  doc["reason"] = reason;
  if (!blockingPet.empty()) doc["pet"] = blockingPet;
  doc["grams"] = grams;
  doc["ts"] = tsMs;
  String body;
  serializeJson(doc, body);
  return request("PUT", [&] { return rtdbUrl("status/feeder/hold"); }, body, nullptr) == 200;
}

bool CloudClient::clearHold() {
  return request("DELETE", [&] { return rtdbUrl("status/feeder/hold"); }, "", nullptr) == 200;
}

bool CloudClient::storageDownload(const std::string& path, std::vector<uint8_t>* bytes) {
  std::lock_guard<std::mutex> tx(txMu_);
  if (!ensureAuthLocked()) return false;
  http_.setTimeout(15000);
  http_.begin(transport_, endpoints::storageObjectUrl(path) + "?alt=media");
  http_.addHeader("Authorization", String("Bearer ") + token());
  const int code = http_.GET();
  if (code != 200) {
    Serial.printf("[cloud] storage GET %s -> %d\n", path.c_str(), code);
    http_.end();
    return false;
  }
  const int len = http_.getSize();
  bytes->clear();
  bytes->reserve(len > 0 ? len : 64 * 1024);
  NetworkClient* stream = http_.getStreamPtr();
  uint8_t buf[1024];
  int remaining = len;
  while (http_.connected() && (remaining > 0 || len < 0)) {
    const size_t avail = stream->available();
    if (!avail) {
      if (len < 0 && !stream->connected()) break;
      delay(5);
      continue;
    }
    const int n = stream->readBytes(buf, avail < sizeof(buf) ? avail : sizeof(buf));
    bytes->insert(bytes->end(), buf, buf + n);
    if (len > 0) remaining -= n;
  }
  http_.end();
  return len < 0 || remaining == 0;
}

bool CloudClient::storageUpload(const std::string& path, const uint8_t* bytes, size_t len,
                                const char* contentType, int petsDetected) {
  {
    std::lock_guard<std::mutex> tx(txMu_);
    if (!ensureAuthLocked()) return false;
    http_.setTimeout(15000);
    http_.begin(transport_, endpoints::storageUploadUrl(path));
    http_.addHeader("Authorization", String("Bearer ") + token());
    http_.addHeader("Content-Type", contentType);
    const int code = http_.sendRequest("POST", const_cast<uint8_t*>(bytes), len);
    http_.end();
    if (code != 200) {
      Serial.printf("[cloud] storage upload %s -> %d\n", path.c_str(), code);
      return false;
    }
  }
  return storageSetMetadata(path, petsDetected, contentType);
}

bool CloudClient::storageSetMetadata(const std::string& path, int petsDetected,
                                     const char* contentType) {
  const String url = endpoints::storageObjectUrl(path);
  JsonDocument meta;
  if (contentType) meta["contentType"] = contentType;
  meta["metadata"]["pets_detected"] = String(petsDetected);
  String body;
  serializeJson(meta, body);
  String resp;
  return request("PATCH", [&] { return url; }, body, &resp, "application/json",
                 true) == 200;
}
