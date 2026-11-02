#include "Arduino.h"
#include "hal_control.h"
#include <SDL3/SDL.h>
#include <random>
#include <unordered_map>
#include <string>

namespace {
uint32_t gTestOffsetMs = 0;
int64_t  gRtcOffsetSec = 0;
std::mt19937 gRng{std::random_device{}()};
std::unordered_map<int, void (*)()> gIsr;
bool gRestartRequested = false;
std::string gDataDir = ".";
std::string gSdRoot  = "./sdcard";
std::function<void(uint32_t)> gIdleWaiter;
}  // namespace

uint32_t halMillis() { return (uint32_t)SDL_GetTicks() + gTestOffsetMs; }
void halAdvanceForTest(uint32_t ms) { gTestOffsetMs = ms ? gTestOffsetMs + ms : 0; }
void halSetRtcOffsetForTest(int64_t s) { gRtcOffsetSec = s; }
int64_t halRtcOffsetSeconds() { return gRtcOffsetSec; }

void halSetDataDir(const char* d) { gDataDir = d ? d : "."; }
void halSetSdRoot(const char* d) { gSdRoot = d ? d : "./sdcard"; }
const char* halDataDir() { return gDataDir.c_str(); }
const char* halSdRoot() { return gSdRoot.c_str(); }

void halSetIdleWaiter(std::function<void(uint32_t)> w) { gIdleWaiter = std::move(w); }
void halIdleWait(uint32_t ms) {
  if (gIdleWaiter) gIdleWaiter(ms); else SDL_Delay(ms);
}

bool halRestartRequested() { return gRestartRequested; }
void halClearRestartRequest() { gRestartRequested = false; }

uint32_t millis() { return halMillis(); }
void delay(uint32_t ms) { SDL_Delay(ms); }

long random(long max) {
  if (max <= 0) return 0;
  return (long)(std::uniform_int_distribution<long>(0, max - 1)(gRng));
}

void pinMode(int, int) {}
void digitalWrite(int, int) {}
int digitalRead(int) { return LOW; }
int digitalPinToInterrupt(int pin) { return pin; }
void attachInterrupt(int irq, void (*isr)(), int) { gIsr[irq] = isr; }
void halFireInterrupt(int pin) {
  auto it = gIsr.find(pin);
  if (it != gIsr.end() && it->second) it->second();
}

EspClass ESP;
void EspClass::restart() { gRestartRequested = true; }
