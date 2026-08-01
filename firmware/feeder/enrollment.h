#pragma once
#include "cloud_client.h"
#include "gallery.h"
#include "vision_client.h"

void galleryLoad(spine::Gallery* g);
void gallerySave(const spine::Gallery& g);

bool handleEnroll(CloudClient* cloud, VisionClient* vision, spine::Gallery* gallery,
                  const PendingCommand& cmd);

bool handleUnenroll(CloudClient* cloud, spine::Gallery* gallery, const PendingCommand& cmd);
