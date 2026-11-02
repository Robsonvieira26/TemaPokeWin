#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include "WString.h"
#include "HardwareSerial.h"
#include "pgmspace.h"
#include "Print.h"
#include "freertos_shim.h"   // no ESP32 o core do Arduino ja traz o FreeRTOS

// MSVC nao entende atributos GNU (Arduino_GFX usa __attribute__((always_inline)))
#ifdef _MSC_VER
#define __attribute__(x)
#endif

#define IRAM_ATTR
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define FALLING 2

// MSVC nao tem gmtime_r (POSIX)
inline std::tm* gmtime_r(const std::time_t* t, std::tm* out) { return gmtime_s(out, t) == 0 ? out : nullptr; }

// Arduino.h real define min/max como macros; aqui, as templates da std.
using std::min;
using std::max;

uint32_t millis();
void delay(uint32_t ms);

long random(long max);

void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int digitalPinToInterrupt(int pin);
void attachInterrupt(int irq, void (*isr)(), int mode);

inline void* ps_malloc(size_t n) { return std::malloc(n); }

struct EspClass {
  uint32_t getFreeHeap() const { return 200u * 1024u; }
  uint32_t getMinFreeHeap() const { return 180u * 1024u; }
  void restart();
};
extern EspClass ESP;

// O touch registra a ISR por attachInterrupt; hal/touch.cpp precisa dispara-la.
void halFireInterrupt(int pin);
