#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct PsramAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) override {
    return psramFound() ? ps_malloc(size) : malloc(size);
  }
  void deallocate(void* p) override { free(p); }
  void* reallocate(void* p, size_t size) override {
    return psramFound() ? ps_realloc(p, size) : realloc(p, size);
  }
  static PsramAllocator* instance() {
    static PsramAllocator a;
    return &a;
  }
};

inline JsonDocument psramJsonDoc() { return JsonDocument(PsramAllocator::instance()); }
