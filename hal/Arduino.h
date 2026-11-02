#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include "WString.h"
#include "HardwareSerial.h"

#define IRAM_ATTR
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define FALLING 2

// MSVC nao tem gmtime_r (POSIX)
inline std::tm* gmtime_r(const std::time_t* t, std::tm* out) { return gmtime_s(out, t) == 0 ? out : nullptr; }

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
