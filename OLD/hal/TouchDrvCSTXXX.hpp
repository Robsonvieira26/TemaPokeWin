#pragma once
#include <cstdint>
#include "Wire.h"

// Emulacao do controlador de toque CST92xx. Somente o que o upstream chama.
// Em hardware o driver desgira o painel (setMirrorXY); aqui as coordenadas ja
// chegam no espaco do display, entao setMirrorXY e no-op.
class TouchDrvCST92xx {
 public:
  void setPins(int rst, int irq);
  bool begin(TwoWire& w, uint8_t addr, int sda, int scl) {
    (void)w; (void)addr; (void)sda; (void)scl; return true;
  }
  void reset() {}
  void setMaxCoordinates(uint16_t, uint16_t) {}
  void setMirrorXY(bool, bool) {}
  uint8_t getPoint(int16_t* x, int16_t* y, uint8_t n = 1);
};
