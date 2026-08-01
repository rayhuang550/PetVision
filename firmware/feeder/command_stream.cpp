#include "command_stream.h"

#include "cloud_endpoints.h"
#include "secrets.h"

namespace {
constexpr uint32_t kQuietReconnectMs = 90 * 1000;
constexpr uint32_t kRetryDelayMs = 3000;
constexpr uint32_t kHeadTimeoutMs = 10000;

void splitUrl(const String& url, String* host, String* path) {
  const int schemeEnd = url.indexOf("://");
  const int hostStart = schemeEnd < 0 ? 0 : schemeEnd + 3;
  const int pathStart = url.indexOf('/', hostStart);
  if (pathStart < 0) {
    *host = url.substring(hostStart);
    *path = "/";
  } else {
    *host = url.substring(hostStart, pathStart);
    *path = url.substring(pathStart);
  }
}
}

void CommandStream::begin(std::function<String()> tokenProvider) {
  token_ = tokenProvider;
  configureCloudTransport(client_);
}

int CommandStream::openStream(const String& host, uint16_t port, const String& path,
                              String* location) {
  if (!client_.connect(host.c_str(), port)) return -1;
  client_.print(String("GET ") + path + " HTTP/1.1\r\nHost: " + host +
                "\r\nAccept: text/event-stream\r\nConnection: keep-alive\r\n\r\n");

  int status = -1;
  String line;
  const uint32_t deadline = millis() + kHeadTimeoutMs;
  for (;;) {
    if (millis() > deadline) return -1;
    if (!client_.connected() && !client_.available()) return -1;
    if (!client_.available()) {
      delay(5);
      continue;
    }
    const char c = static_cast<char>(client_.read());
    if (c == '\r') continue;
    if (c != '\n') {
      if (line.length() < 512) line += c;
      continue;
    }
    if (line.length() == 0) return status;
    if (status < 0) {
      const int sp = line.indexOf(' ');
      if (sp > 0) status = line.substring(sp + 1, sp + 4).toInt();
    } else if (location) {
      const int colon = line.indexOf(':');
      if (colon > 0 && line.substring(0, colon).equalsIgnoreCase("Location")) {
        *location = line.substring(colon + 1);
        location->trim();
      }
    }
    line = "";
  }
}

bool CommandStream::connect() {
  client_.stop();
  const String tok = token_();
  if (!tok.length()) return false;

  const String path = endpoints::rtdbStreamPath(
      "commands", tok, "orderBy=%22status%22&equalTo=%22pending%22");

  String location;
  int status = openStream(endpoints::rtdbHost(), endpoints::rtdbPort(), path, &location);

  if ((status == 307 || status == 301 || status == 302) && location.length()) {
    String host, redirectPath;
    splitUrl(location, &host, &redirectPath);
    client_.stop();
    Serial.printf("[stream] redirected to %s\n", host.c_str());
    status = openStream(host, 443, redirectPath, nullptr);
  }

  if (status != 200) {
    Serial.printf("[stream] connect failed: status %d\n", status);
    client_.stop();
    return false;
  }
  lastByteMs_ = millis();
  Serial.println("[stream] /commands SSE connected");
  return true;
}

void CommandStream::taskLoop() {
  String line;
  String eventType;
  for (;;) {
    if (!client_.connected()) {
      if (!connect()) {
        delay(kRetryDelayMs);
        continue;
      }
      dirty_ = true;
    }
    while (client_.available()) {
      const char c = static_cast<char>(client_.read());
      lastByteMs_ = millis();
      if (c == '\r') continue;
      if (c != '\n') {
        if (line.length() < 512) line += c;
        continue;
      }
      if (line.startsWith("event:")) {
        eventType = line.substring(6);
        eventType.trim();
      } else if (line.startsWith("data:")) {
        if (eventType == "put" || eventType == "patch") {
          dirty_ = true;
        } else if (eventType == "auth_revoked" || eventType == "cancel") {
          Serial.printf("[stream] %s -> reconnect\n", eventType.c_str());
          client_.stop();
        }
      }
      line = "";
    }
    if (millis() - lastByteMs_ > kQuietReconnectMs) {
      Serial.println("[stream] quiet too long -> reconnect");
      client_.stop();
    }
    delay(20);
  }
}
