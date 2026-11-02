#include "TouchDrvCSTXXX.hpp"
#include "Arduino.h"
#include "hal_control.h"
#include <mutex>

namespace {
std::mutex gMu;
int gX = 0, gY = 0;
bool gDown = false;
int gIrqPin = -1;  // definido por setPins; -1 = nenhum
}  // namespace

void halSetTouchIrqPin(int pin) { std::lock_guard<std::mutex> g(gMu); gIrqPin = pin; }
int halTouchIrqPin() { std::lock_guard<std::mutex> g(gMu); return gIrqPin; }
bool halTouchIsDown() { std::lock_guard<std::mutex> g(gMu); return gDown; }
void TouchDrvCST92xx::setPins(int, int irq) { halSetTouchIrqPin(irq); }

// Chamadas da thread de UI. A ISR e disparada fora do mutex.
void halTouchDown(int x, int y) {
  int pin;
  { std::lock_guard<std::mutex> g(gMu); gX = x; gY = y; gDown = true; pin = gIrqPin; }
  if (pin >= 0) halFireInterrupt(digitalPinToInterrupt(pin));  // FALLING: dedo desceu
}
void halTouchMove(int x, int y) {
  int pin; bool down;
  { std::lock_guard<std::mutex> g(gMu); gX = x; gY = y; down = gDown; pin = gIrqPin; }
  if (down && pin >= 0) halFireInterrupt(digitalPinToInterrupt(pin));  // novo relatorio
}
void halTouchUp() {
  // Borda de subida: sem ISR. O firmware trata a soltura pelo estado "wasPressed".
  std::lock_guard<std::mutex> g(gMu);
  gDown = false;
}

uint8_t TouchDrvCST92xx::getPoint(int16_t* x, int16_t* y, uint8_t n) {
  if (n == 0) return 0;
  std::lock_guard<std::mutex> g(gMu);
  if (!gDown) return 0;
  if (x) *x = (int16_t)gX;
  if (y) *y = (int16_t)gY;
  return 1;
}
