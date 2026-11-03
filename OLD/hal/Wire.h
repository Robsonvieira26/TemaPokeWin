#pragma once
#include <cstdint>
#include <cstddef>

// I2C stub: nenhum dispositivo responde (o upstream so le Wire em touch/audio).
class TwoWire {
 public:
  bool begin(int sda, int scl) { (void)sda; (void)scl; return true; }
  void setTimeOut(uint16_t) {}
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool stop = true) { (void)stop; return 0; }  // 0 = sucesso
  size_t write(uint8_t) { return 1; }
  uint8_t requestFrom(uint8_t, size_t) { return 0; }
  int available() { return 0; }
  int read() { return -1; }
};
extern TwoWire Wire;
