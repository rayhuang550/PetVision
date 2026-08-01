#pragma once
#include <Arduino.h>

#include <atomic>
#include <functional>

#include "cloud_tls.h"

class CommandStream {
 public:
  void begin(std::function<String()> tokenProvider);

  bool consumeDirty() { return dirty_.exchange(false); }

  void taskLoop();

 private:
  bool connect();
  int openStream(const String& host, uint16_t port, const String& path, String* location);

  std::function<String()> token_;
  CloudTransport client_;
  std::atomic<bool> dirty_{true};
  uint32_t lastByteMs_ = 0;
};
