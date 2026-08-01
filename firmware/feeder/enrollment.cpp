#include "enrollment.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "psram_json.h"

namespace {
constexpr const char* kGalleryPath = "/gallery.json";
}

void galleryLoad(spine::Gallery* g) {
  File f = LittleFS.open(kGalleryPath, "r");
  if (!f) {
    Serial.println("[gallery] no gallery.json; starting empty");
    return;
  }
  std::string blob;
  blob.resize(f.size());
  f.read(reinterpret_cast<uint8_t*>(blob.data()), blob.size());
  f.close();
  if (!g->fromJson(blob)) {
    Serial.println("[gallery] gallery.json unreadable; starting empty");
    return;
  }
  Serial.printf("[gallery] loaded %u reference(s)\n", static_cast<unsigned>(g->size()));
}

void gallerySave(const spine::Gallery& g) {
  File f = LittleFS.open(kGalleryPath, "w");
  if (!f) {
    Serial.println("[gallery] save failed (LittleFS open)");
    return;
  }
  const std::string blob = g.toJson();
  f.write(reinterpret_cast<const uint8_t*>(blob.data()), blob.size());
  f.close();
}

bool handleEnroll(CloudClient* cloud, VisionClient* vision, spine::Gallery* gallery,
                  const PendingCommand& cmd) {
  JsonDocument resultDoc(PsramAllocator::instance());
  JsonArray result = resultDoc.to<JsonArray>();

  for (const auto& path : cmd.photos) {
    std::vector<uint8_t> original;
    if (!cloud->storageDownload(path, &original)) {
      Serial.printf("[enroll] %s: Storage download failed; retry later\n", path.c_str());
      return false;
    }

    EnrollEmbed r = vision->embedForEnroll(original.data(), original.size());
    if (!r.ok) {
      Serial.printf("[enroll] %s: FAILED (vision_host_unreachable)\n", cmd.pet.c_str());
      return cloud->markOutcome(cmd.key, "failed", "vision_host_unreachable", -1);
    }

    const bool stored =
        !r.cropJpeg.empty()
            ? cloud->storageUpload(path, r.cropJpeg.data(), r.cropJpeg.size(), "image/jpeg",
                                   r.petCount)
            : cloud->storageSetMetadata(path, r.petCount);
    if (!stored) {
      Serial.printf("[enroll] %s: Storage write failed; retry later\n", path.c_str());
      return false;
    }

    gallery->add(path, cmd.pet, std::move(r.embedding));
    JsonObject row = result.add<JsonObject>();
    row["path"] = path;
    row["pets_detected"] = r.petCount;
  }

  gallerySave(*gallery);
  if (!cloud->markOutcomeResult(cmd.key, result)) return false;
  int onePet = 0;
  for (JsonObject row : result)
    if ((row["pets_detected"] | 0) == 1) onePet++;
  Serial.printf("[enroll] %s: DONE (%d/%d cropped, %d flagged)\n", cmd.pet.c_str(), onePet,
                static_cast<int>(result.size()), static_cast<int>(result.size()) - onePet);
  return true;
}

bool handleUnenroll(CloudClient* cloud, spine::Gallery* gallery, const PendingCommand& cmd) {
  gallery->prune(cmd.photos);
  gallerySave(*gallery);
  if (!cloud->markOutcome(cmd.key, "done", "", -1)) return false;
  Serial.printf("[unenroll] %s: DONE (pruned %d)\n", cmd.pet.c_str(),
                static_cast<int>(cmd.photos.size()));
  return true;
}
