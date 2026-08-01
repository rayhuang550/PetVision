#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "cloud_tls.h"
#include "feed_flow.h"
#include "ports.h"

struct PendingCommand {
  std::string key;
  std::string type;
  std::string pet;
  std::vector<std::string> photos;
};

class CloudClient : public spine::ICloudOps {
 public:
  void begin();

  bool signIn();
  String token();

  std::string appendDispense(const std::string& pet, float dispensed, const std::string& hopper,
                             int64_t tsMs) override;
  bool recordEaten(const std::string& pet, const std::string& rowKey, float eaten) override;
  bool markOutcome(const std::string& cmdKey, const std::string& status, const std::string& reason,
                   float grams, const std::string& blockingPet = "") override;
  double dispensedToday(const std::string& pet, int64_t localMidnightMs) override;

  bool markOutcomeResult(const std::string& cmdKey, JsonArray result);

  bool getTimezone(std::string* ianaTz);
  bool getPet(const std::string& id, spine::PetCfg* out, bool* exists);

  bool getPendingCommands(std::vector<PendingCommand>* out);

  bool heartbeat(const char* state, int64_t tsMs, bool ready = true,
                 const char* readinessReason = "");
  bool publishHold(const std::string& reason, const std::string& blockingPet, float grams,
                   int64_t tsMs);
  bool clearHold();

  bool storageDownload(const std::string& path, std::vector<uint8_t>* bytes);
  bool storageUpload(const std::string& path, const uint8_t* bytes, size_t len,
                     const char* contentType, int petsDetected);
  bool storageSetMetadata(const std::string& path, int petsDetected,
                          const char* contentType = nullptr);

  bool verbose = true;

 private:
  String rtdbUrl(const String& path, const String& extraQuery = "");
  int request(const char* method, const std::function<String()>& urlFn, const String& body,
              String* resp, const char* contentType = "application/json", bool bearerAuth = false);

  bool signInLocked();
  bool ensureAuthLocked();

  std::mutex txMu_;
  CloudTransport transport_;
  HTTPClient http_;

  std::mutex authMu_;
  String idToken_;
  int64_t tokenExpiryMs_ = 0;
};
