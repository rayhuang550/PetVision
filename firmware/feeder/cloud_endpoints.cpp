#include "cloud_endpoints.h"

#include "secrets.h"

namespace endpoints {

String urlEncodePath(const std::string& s) {
  String out;
  const char* hex = "0123456789ABCDEF";
  for (char c : s) {
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

const char* rtdbHost() { return CLOUD_RTDB_HOST; }
uint16_t rtdbPort() { return 443; }

String authSignInUrl() {
  return String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") +
         CLOUD_API_KEY;
}

static String rtdbQuery(const String& token, const String& extraQuery) {
  String q = String(".json?auth=") + token;
  if (extraQuery.length()) q += "&" + extraQuery;
  return q;
}

String rtdbUrl(const String& path, const String& token, const String& extraQuery) {
  return String("https://") + CLOUD_RTDB_HOST + "/" + path + rtdbQuery(token, extraQuery);
}

String rtdbStreamPath(const String& path, const String& token, const String& extraQuery) {
  return "/" + path + rtdbQuery(token, extraQuery);
}

String storageObjectUrl(const std::string& path) {
  return String("https://firebasestorage.googleapis.com/v0/b/") + CLOUD_STORAGE_BUCKET + "/o/" +
         urlEncodePath(path);
}

String storageUploadUrl(const std::string& path) {
  return String("https://firebasestorage.googleapis.com/v0/b/") + CLOUD_STORAGE_BUCKET +
         "/o?uploadType=media&name=" + urlEncodePath(path);
}

}
