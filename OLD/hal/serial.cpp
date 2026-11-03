#include "HardwareSerial.h"
#include "WString.h"
#include "hal_control.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace {
std::mutex gMu;
std::deque<char> gIn;
unsigned long gTimeoutMs = 1000;  // padrao do Arduino
std::FILE* gLog = nullptr;

std::FILE* logFile() {
  if (gLog) return gLog;
  std::error_code ec;
  std::filesystem::create_directories(halDataDir(), ec);
  std::string path = std::string(halDataDir()) + "/log.txt";
  // rotacao simples em 1 MB
  if (std::filesystem::exists(path, ec) &&
      std::filesystem::file_size(path, ec) > 1024u * 1024u) {
    std::filesystem::rename(path, path + ".1", ec);
  }
  gLog = std::fopen(path.c_str(), "ab");
  return gLog;
}

void emit(const char* s, size_t n) {
  std::fwrite(s, 1, n, stdout);
  if (std::FILE* f = logFile()) { std::fwrite(s, 1, n, f); std::fflush(f); }
}
}  // namespace

HardwareSerial Serial;

void halSerialInject(const char* line) {
  if (!line) return;
  std::lock_guard<std::mutex> g(gMu);
  for (const char* p = line; *p; ++p) gIn.push_back(*p);
}

void HardwareSerial::setTimeout(unsigned long ms) {
  std::lock_guard<std::mutex> g(gMu);
  gTimeoutMs = ms;
}

int HardwareSerial::available() {
  std::lock_guard<std::mutex> g(gMu);
  return (int)gIn.size();
}

int HardwareSerial::read() {
  std::lock_guard<std::mutex> g(gMu);
  if (gIn.empty()) return -1;
  char c = gIn.front();
  gIn.pop_front();
  return (unsigned char)c;
}

size_t HardwareSerial::readBytes(char* buf, size_t len) {
  using clock = std::chrono::steady_clock;
  size_t n = 0;
  auto deadline = clock::now();
  {
    std::lock_guard<std::mutex> g(gMu);
    deadline += std::chrono::milliseconds(gTimeoutMs);
  }
  // Como no Arduino: espera ate len bytes ou ate o timeout (a thread do
  // console continua injetando enquanto isso).
  for (;;) {
    {
      std::lock_guard<std::mutex> g(gMu);
      while (n < len && !gIn.empty()) { buf[n++] = gIn.front(); gIn.pop_front(); }
    }
    if (n >= len || clock::now() >= deadline) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return n;
}

String HardwareSerial::readStringUntil(char term) {
  std::lock_guard<std::mutex> g(gMu);
  std::string out;
  while (!gIn.empty()) {
    char c = gIn.front();
    gIn.pop_front();
    if (c == term) break;
    out.push_back(c);
  }
  return String(out);
}

void HardwareSerial::println(const char* s) {
  emit(s, std::strlen(s));
  emit("\n", 1);
}

int HardwareSerial::printf(const char* fmt, ...) {
  char b[1024];
  va_list ap;
  va_start(ap, fmt);
  int n = std::vsnprintf(b, sizeof b, fmt, ap);
  va_end(ap);
  if (n > 0) emit(b, (size_t)(n < (int)sizeof b ? n : (int)sizeof b - 1));
  return n;
}
