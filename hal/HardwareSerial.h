#pragma once
#include <cstddef>
#include <cstdint>
class String;

// Somente o que o upstream usa de Serial (ver grep da Task 6).
class HardwareSerial {
 public:
  void begin(unsigned long = 115200) {}
  void setRxBufferSize(size_t) {}
  void setTxTimeoutMs(uint32_t) {}
  void setTimeout(unsigned long ms);  // readBytes espera ate este prazo

  int available();
  int read();  // -1 se vazio (usado tambem para esvaziar a fila nos testes)
  size_t readBytes(char* buf, size_t len);
  String readStringUntil(char terminator);

  void println(const char* s = "");
  int printf(const char* fmt, ...);
};
extern HardwareSerial Serial;
