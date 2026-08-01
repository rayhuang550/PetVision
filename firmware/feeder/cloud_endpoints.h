#pragma once
#include <Arduino.h>

#include <string>

namespace endpoints {

const char* rtdbHost();
uint16_t rtdbPort();

String authSignInUrl();

String rtdbUrl(const String& path, const String& token, const String& extraQuery = "");

String rtdbStreamPath(const String& path, const String& token, const String& extraQuery = "");

String storageObjectUrl(const std::string& path);

String storageUploadUrl(const std::string& path);

String urlEncodePath(const std::string& s);

}
