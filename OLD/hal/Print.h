#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include "WString.h"
#include "pgmspace.h"

// Print do Arduino: o Arduino_GFX herda disso para text rendering.
class Print {
 public:
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* buf, size_t n) {
    size_t w = 0;
    for (size_t i = 0; i < n; ++i) w += write(buf[i]);
    return w;
  }
  size_t print(const char* s) {
    size_t w = 0;
    for (const char* p = s; p && *p; ++p) w += write((uint8_t)*p);
    return w;
  }
  size_t print(const String& s) { return print(s.c_str()); }
  size_t print(const __FlashStringHelper* s) { return print(reinterpret_cast<const char*>(s)); }
  size_t print(int v) { char b[32]; int n = std::snprintf(b, sizeof b, "%d", v); return write((const uint8_t*)b, (size_t)n); }
  size_t println(const char* s = "") { size_t w = print(s); w += write((uint8_t)'\n'); return w; }
  int printf(const char* fmt, ...) {
    char b[512]; va_list ap; va_start(ap, fmt);
    int n = std::vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    if (n > 0) write((const uint8_t*)b, (size_t)n);
    return n;
  }
  virtual ~Print() = default;
};
