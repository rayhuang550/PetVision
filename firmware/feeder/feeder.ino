#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include <atomic>
#include <cmath>
#include <set>

#include "cloud_client.h"
#include "command_stream.h"
#include "diagnostics.h"
#include "enrollment.h"
#include "feed_flow.h"
#include "gallery.h"
#include "hal_real.h"
#include "hardware_config.h"
#include "identity_window.h"
#include "secrets.h"
#include "spine_config.h"
#include "vision_client.h"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static spine::SpineConfig g_cfg;
static HardwareConfig g_hwConfig;
static RealDispenser g_dispenser(&g_hwConfig);
static RealScale g_scale(&g_hwConfig);
static RealCamera g_camera;
static CloudClient g_cloud;
static CommandStream g_stream;
static VisionClient g_vision;
static spine::Gallery g_gallery;
static WiFiMulti g_wifi;

class RealClock : public spine::IClock {
 public:
  int64_t epochMs() override { return static_cast<int64_t>(time(nullptr)) * 1000; }
  void sleepMs(uint32_t ms) override { vTaskDelay(pdMS_TO_TICKS(ms)); }
};
static RealClock g_clock;
static spine::FeedFlow g_flow(&g_dispenser, &g_scale, &g_cloud, &g_clock, g_cfg);

struct FeedReq {
  char cmdKey[40];
  char petId[40];
};
static QueueHandle_t g_feedQueue;
static std::atomic<bool> g_busy{false};
static std::atomic<const char*> g_stateName{"idle"};
static std::atomic<uint32_t> g_identifyCooldownUntil{0};
static std::atomic<bool> g_scaleStable{false};
static std::atomic<bool> g_cameraChecked{false};
static std::atomic<bool> g_visionChecked{false};
static std::atomic<bool> g_stepperChecked{false};
static std::atomic<bool> g_armed{false};
static uint32_t g_bootId = 0;

static const char* readinessReason() {
  return spine::hardwareReadinessReason({
      g_scale.ready(), g_hwConfig.scaleCalibrated(), g_scale.tared(), g_scaleStable.load(),
      g_camera.ready(), g_cameraChecked.load(), g_visionChecked.load(),
      g_hwConfig.dispenserConfigured(), g_dispenser.homed(), g_stepperChecked.load(),
  });
}

static bool hardwareReady() { return readinessReason()[0] == '\0'; }

static void emitJson(JsonDocument& doc) {
  Serial.print("@pf ");
  serializeJson(doc, Serial);
  Serial.println();
}

static void emitResult(const char* id, const char* status, const char* reason = "") {
  JsonDocument doc;
  doc["v"] = 1;
  doc["type"] = "result";
  doc["id"] = id;
  doc["status"] = status;
  if (reason && reason[0]) doc["reason"] = reason;
  emitJson(doc);
}

static void emitHello() {
  JsonDocument doc;
  doc["v"] = 1;
  doc["type"] = "hello";
  doc["sketch"] = "feeder";
  doc["boot_id"] = g_bootId;
  doc["build"] = __DATE__ " " __TIME__;
  JsonArray caps = doc["capabilities"].to<JsonArray>();
  for (const char* cap : {"telemetry", "scale.tare", "scale.calibrate", "scale.stability",
                          "stepper.jog", "stepper.release", "stepper.set_home",
                          "stepper.save_hopper", "stepper.interval", "stepper.confirm",
                          "camera.capture", "commission.arm"})
    caps.add(cap);
  emitJson(doc);
}

static void emitTelemetry() {
  JsonDocument doc;
  doc["v"] = 1;
  doc["type"] = "telemetry";
  doc["boot_id"] = g_bootId;
  doc["armed"] = g_armed.load();
  doc["ready"] = hardwareReady();
  doc["readiness_reason"] = readinessReason();
  JsonObject scale = doc["scale"].to<JsonObject>();
  scale["available"] = g_scale.ready();
  scale["raw"] = g_scale.raw();
  scale["grams"] = g_scale.grams();
  scale["tared"] = g_scale.tared();
  scale["calibrated"] = g_hwConfig.scaleCalibrated();
  scale["stable"] = g_scaleStable.load();
  JsonObject stepper = doc["stepper"].to<JsonObject>();
  stepper["homed"] = g_dispenser.homed();
  stepper["configured"] = g_dispenser.configured();
  stepper["confirmed"] = g_stepperChecked.load();
  stepper["position"] = g_dispenser.position();
  JsonObject camera = doc["camera"].to<JsonObject>();
  camera["available"] = g_camera.ready();
  camera["checked"] = g_cameraChecked.load();
  JsonObject vision = doc["vision"].to<JsonObject>();
  vision["checked"] = g_visionChecked.load();
  JsonObject gallery = doc["gallery"].to<JsonObject>();
  gallery["references"] = g_gallery.size();
  emitJson(doc);
}

static void handleDiagnosticCommand(const String& line) {
  if (!line.startsWith("@pf ")) return;
  JsonDocument doc;
  if (deserializeJson(doc, line.substring(4))) return;
  if (strcmp(doc["type"] | "", "command") != 0) return;
  const char* id = doc["id"] | "";
  const char* name = doc["name"] | "";
  const uint32_t bootId = doc["boot_id"] | 0u;
  if (!id[0] || !name[0]) return;
  if (!strcmp(name, "system.hello")) {
    emitHello();
    emitResult(id, "done");
    return;
  }
  if (bootId != g_bootId) {
    emitResult(id, "failed", "stale_boot");
    return;
  }
  if (g_busy.load() && strcmp(name, "stepper.release") != 0) {
    emitResult(id, "held", "feeder_busy");
    return;
  }

  if (!strcmp(name, "scale.tare")) {
    g_armed = false;
    g_scaleStable = false;
    g_scale.tare();
    emitResult(id, "done");
  } else if (!strcmp(name, "scale.calibrate")) {
    g_armed = false;
    const float grams = doc["args"]["grams"] | 0.0f;
    const bool ok = g_scale.calibrate(grams);
    emitResult(id, ok ? "done" : "failed", ok ? "" : "calibration_failed");
  } else if (!strcmp(name, "scale.stability")) {
    g_armed = false;
    float peakToPeak = 0.0f;
    const bool stable = g_scale.stability(&peakToPeak);
    g_scaleStable = stable;
    JsonDocument result;
    result["v"] = 1;
    result["type"] = "result";
    result["id"] = id;
    result["status"] = stable ? "done" : "held";
    result["reason"] = stable ? "" : "scale_unstable";
    result["peak_to_peak_grams"] = peakToPeak;
    emitJson(result);
  } else if (!strcmp(name, "stepper.jog")) {
    g_armed = false;
    g_stepperChecked = false;
    const int32_t steps = doc["args"]["steps"] | 0;
    const bool ok = g_dispenser.jog(steps);
    emitResult(id, ok ? "done" : "failed", ok ? "" : "invalid_jog");
  } else if (!strcmp(name, "stepper.release")) {
    g_dispenser.release();
    emitResult(id, "done");
  } else if (!strcmp(name, "stepper.set_home")) {
    g_armed = false;
    g_stepperChecked = false;
    g_dispenser.setHome();
    emitResult(id, "done");
  } else if (!strcmp(name, "stepper.confirm")) {
    g_armed = false;
    if (!g_dispenser.homed()) {
      emitResult(id, "held", "home_required");
    } else {
      g_stepperChecked = true;
      emitResult(id, "done");
    }
  } else if (!strcmp(name, "stepper.interval")) {
    g_armed = false;
    const uint32_t value = doc["args"]["microseconds"] | 0;
    const bool ok = g_dispenser.setStepInterval(value);
    emitResult(id, ok ? "done" : "failed", ok ? "" : "invalid_step_interval");
  } else if (!strcmp(name, "stepper.save_hopper")) {
    g_armed = false;
    const int hopper = doc["args"]["hopper"] | 0;
    const int32_t closed = doc["args"]["closed"] | 0;
    const int32_t open = doc["args"]["open"] | 0;
    const bool ok = g_dispenser.saveHopper(hopper, closed, open);
    emitResult(id, ok ? "done" : "failed", ok ? "" : "invalid_hopper_offsets");
  } else if (!strcmp(name, "camera.capture")) {
    std::vector<uint8_t> jpeg;
    if (!g_camera.capture(&jpeg)) {
      emitResult(id, "failed", "camera_capture_failed");
      return;
    }
    g_cameraChecked = true;
    const spine::EmbedResult er = g_vision.embed(jpeg.data(), jpeg.size());
    g_visionChecked = er.ok;
    JsonDocument result;
    result["v"] = 1;
    result["type"] = "result";
    result["id"] = id;
    result["status"] = er.ok ? "done" : "failed";
    if (!er.ok) result["reason"] = "vision_host_unreachable";
    result["request_id"] = er.requestId;
    result["pet_count"] = er.petCount;
    result["box_height_fraction"] = er.boxHeightFrac;
    if (er.ok && er.petCount == 1) {
      const auto match = g_gallery.match(er.embedding);
      result["match"]["pet"] = match.pet;
      result["match"]["similarity"] = match.sim;
      result["match"]["known"] = match.sim >= g_cfg.matchThreshold;
    }
    emitJson(result);
  } else if (!strcmp(name, "commission.arm")) {
    if (!hardwareReady()) {
      emitResult(id, "held", readinessReason());
    } else {
      g_armed = true;
      emitResult(id, "done");
    }
  } else {
    emitResult(id, "failed", "unsupported_command");
  }
}

static void pollDiagnosticSerial() {
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length()) handleDiagnosticCommand(line);
      line = "";
    } else if (line.length() < 2048) {
      line += c;
    }
  }
}

static void controlTask(void*) {
  for (;;) {
    FeedReq req;
    if (xQueueReceive(g_feedQueue, &req, pdMS_TO_TICKS(250)) != pdTRUE) continue;
    g_busy = true;

    if (!g_armed.load()) {
      if (req.cmdKey[0])
        g_cloud.markOutcome(req.cmdKey, "held", "hardware_not_ready", -1.0f, "");
      Serial.printf("[control] feed held: %s\n", readinessReason());
      g_busy = false;
      continue;
    }

    std::string tz;
    if (!g_cloud.getTimezone(&tz)) tz = "America/Toronto";
    spine::PetCfg pet;
    bool exists = false;
    if (!g_cloud.getPet(req.petId, &pet, &exists)) {
      Serial.printf("[control] pet %s unreadable; leaving command pending\n", req.petId);
      g_busy = false;
      continue;
    }
    if (!exists) {
      if (req.cmdKey[0]) g_cloud.markOutcome(req.cmdKey, "failed", "unknown pet", -1);
      g_busy = false;
      continue;
    }

    const bool manual = req.cmdKey[0];
    spine::FlowResult res = manual ? g_flow.runManualFeed(req.cmdKey, pet, tz)
                                   : g_flow.runIdentifiedFeed(pet, tz);
    Serial.printf("[control] %s feed %s: %s%s%s (%.0f g)\n", req.cmdKey[0] ? "manual" : "identified",
                  pet.name.c_str(), res.status.c_str(), res.reason.empty() ? "" : ": ",
                  res.reason.c_str(), static_cast<double>(res.grams));

    if (res.status == "held" && res.reason == "bowl_not_empty") {
      g_cloud.publishHold(res.reason, res.blockingPet, res.grams, g_clock.epochMs());
    } else if (res.status == "done" || res.reason == "portion_in_bowl") {
      g_cloud.clearHold();
    }

    g_busy = false;
  }
}

static void streamTask(void*) { g_stream.taskLoop(); }

static void identifyTick(spine::IdentityWindow* window, int* framesInWindow) {
  if (!g_armed || g_busy || millis() < g_identifyCooldownUntil.load() || g_gallery.size() == 0)
    return;

  std::vector<uint8_t> jpeg;
  if (!g_camera.capture(&jpeg)) return;

  const spine::EmbedResult er = g_vision.embed(jpeg.data(), jpeg.size());
  if (!er.ok) {
    window->reset();
    *framesInWindow = 0;
    return;
  }

  spine::FrameObs obs;
  obs.petCount = er.petCount;
  obs.boxHeightFrac = er.boxHeightFrac;
  if (er.petCount == 1) {
    const auto m = g_gallery.match(er.embedding);
    obs.embedding = er.embedding;
    obs.topPet = m.pet;
    obs.topSim = m.sim;
    Serial.printf("[identify] 1 pet, box %.2f, top %s (%.3f)\n", er.boxHeightFrac, m.pet.c_str(),
                  m.sim);
  } else if (er.petCount >= 2) {
    Serial.printf("[identify] %d pets; window reset (safety)\n", er.petCount);
  }
  window->append(obs);
  if (er.petCount >= 1) {
    g_stateName = "identify";
    (*framesInWindow)++;
  }

  std::string pet;
  std::vector<float> avg;
  if (window->candidate(&pet, &avg)) {
    const auto m = g_gallery.match(avg);
    if (m.pet == pet && m.sim >= g_cfg.matchThreshold) {
      Serial.printf("[identify] CONFIRMED %s (avg sim %.3f); feeding\n", pet.c_str(), m.sim);
      FeedReq req{};
      strlcpy(req.petId, pet.c_str(), sizeof(req.petId));
      xQueueSend(g_feedQueue, &req, 0);
      g_identifyCooldownUntil = millis() + 20000;
    } else {
      Serial.printf("[identify] averaged match failed (%s %.3f); window reset\n", m.pet.c_str(),
                    m.sim);
    }
    window->reset();
    *framesInWindow = 0;
    g_stateName = "idle";
    return;
  }

  if (*framesInWindow >= g_cfg.maxIdentifyFrames) {
    window->reset();
    *framesInWindow = 0;
    g_stateName = "idle";
  }
}

static void netTask(void*) {
  std::set<std::string> handled;
  spine::IdentityWindow window(g_cfg);
  int framesInWindow = 0;
  uint32_t lastBeat = 0, lastIdentify = 0, lastTelemetry = 0, forceRefetchAt = 0;

  for (;;) {
    pollDiagnosticSerial();

    const bool refetch = forceRefetchAt && millis() > forceRefetchAt;
    if (g_stream.consumeDirty() || refetch) {
      if (refetch) forceRefetchAt = 0;
      std::vector<PendingCommand> pending;
      if (g_cloud.getPendingCommands(&pending)) {
        for (const auto& cmd : pending) {
          if (handled.count(cmd.key)) continue;
          handled.insert(cmd.key);
          bool ok = true;
          if (cmd.type == "feed") {
            FeedReq req{};
            strlcpy(req.cmdKey, cmd.key.c_str(), sizeof(req.cmdKey));
            strlcpy(req.petId, cmd.pet.c_str(), sizeof(req.petId));
            ok = xQueueSend(g_feedQueue, &req, 0) == pdTRUE;
          } else if (cmd.type == "enroll") {
            ok = handleEnroll(&g_cloud, &g_vision, &g_gallery, cmd);
          } else if (cmd.type == "unenroll") {
            ok = handleUnenroll(&g_cloud, &g_gallery, cmd);
          } else {
            Serial.printf("[net] ignoring unknown command type: %s\n", cmd.type.c_str());
          }
          if (!ok) {
            handled.erase(cmd.key);
            forceRefetchAt = millis() + 5000;
          }
        }
        std::set<std::string> stillPending;
        for (const auto& cmd : pending) stillPending.insert(cmd.key);
        for (auto it = handled.begin(); it != handled.end();)
          it = stillPending.count(*it) ? std::next(it) : handled.erase(it);
      } else {
        forceRefetchAt = millis() + 5000;
      }
    }

    if (millis() - lastBeat > static_cast<uint32_t>(g_cfg.heartbeatMs)) {
      lastBeat = millis();
      const bool ready = g_armed.load();
      g_cloud.heartbeat(g_stateName.load(), g_clock.epochMs(), ready,
                        ready ? "" : readinessReason());
    }

    if (millis() - lastTelemetry > 500) {
      lastTelemetry = millis();
      emitTelemetry();
    }

    if (millis() - lastIdentify > 1000) {
      lastIdentify = millis();
      identifyTick(&window, &framesInWindow);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static void pumpDelay(uint32_t ms) {
  const uint32_t until = millis() + ms;
  while (static_cast<int32_t>(until - millis()) > 0) {
    pollDiagnosticSerial();
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n[feeder] boot; PSRAM %s (%u KB free)\n",
                psramFound() ? "OK" : "MISSING",
                static_cast<unsigned>(ESP.getFreePsram() / 1024));

  if (!LittleFS.begin(true)) Serial.println("[feeder] LittleFS mount failed");
  galleryLoad(&g_gallery);
  g_bootId = esp_random();
  g_hwConfig.load();
  g_dispenser.begin();
  const bool scaleReady = g_scale.begin();
  const bool cameraReady = g_camera.begin();
  Serial.printf("[feeder] hardware scale=%s camera=%s calibration=%s hopper offsets=%s\n",
                scaleReady ? "ready" : "missing", cameraReady ? "ready" : "missing",
                g_hwConfig.scaleCalibrated() ? "saved" : "required",
                g_hwConfig.dispenserConfigured() ? "saved" : "required");
  emitHello();

  WiFi.mode(WIFI_STA);
  g_wifi.addAP(WIFI_SSID, WIFI_PASSWORD);
#ifdef WIFI_SSID_2
  g_wifi.addAP(WIFI_SSID_2, WIFI_PASSWORD_2);
#endif
  Serial.print("[feeder] joining configured Wi-Fi");
  while (g_wifi.run() != WL_CONNECTED) {
    pumpDelay(500);
    Serial.print(".");
  }
  Serial.printf("\n[feeder] wifi up, ssid=%s ip=%s rssi=%d\n", WiFi.SSID().c_str(),
                WiFi.localIP().toString().c_str(), WiFi.RSSI());

  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.print("[feeder] waiting for SNTP");
  while (time(nullptr) < 1700000000) {
    pumpDelay(250);
    Serial.print(".");
  }
  Serial.printf("\n[feeder] clock ok: %lld\n", static_cast<long long>(time(nullptr)));

  g_cloud.begin();
  while (!g_cloud.signIn()) {
    Serial.println("[feeder] sign-in retry in 5 s");
    pumpDelay(5000);
  }
  g_flow.onState = [](spine::State s) {
    g_stateName = spine::stateName(s);
  };
  g_stream.begin([] { return g_cloud.token(); });
  g_feedQueue = xQueueCreate(8, sizeof(FeedReq));

  xTaskCreatePinnedToCore(streamTask, "sse", 16384, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(netTask, "net", 20480, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(controlTask, "control", 16384, nullptr, 2, nullptr, 1);

  Serial.printf("[feeder] watching /commands · gallery=%u refs · vision=http://%s:%d\n",
                static_cast<unsigned>(g_gallery.size()), VISION_HOST, VISION_PORT);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
