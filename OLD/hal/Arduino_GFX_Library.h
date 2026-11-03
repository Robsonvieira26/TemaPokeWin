#pragma once
// Substitui o Arduino_GFX_Library.h da biblioteca (que puxa todos os paineis e
// barramentos e nao compila no Windows). Inclui so o nucleo real e define, com os
// mesmos nomes, o barramento inerte e o painel falso. 'hal' vem ANTES de
// third_party/Arduino_GFX/src no include path.
#include "Arduino_GFX.h"
#include "Arduino_G.h"
#include "Arduino_DataBus.h"
#include "canvas/Arduino_Canvas.h"

// Barramento falso: aceita qualquer lista de argumentos e nao faz nada.
class Arduino_ESP32QSPI : public Arduino_DataBus {
 public:
  template <typename... A>
  explicit Arduino_ESP32QSPI(A&&...) {}
  bool begin(int32_t = 0, int8_t = -1) override { return true; }
  void beginWrite() override {}
  void endWrite() override {}
  void writeCommand(uint8_t) override {}
  void writeCommand16(uint16_t) override {}
  void writeCommandBytes(uint8_t*, uint32_t) override {}
  void write(uint8_t) override {}
  void write16(uint16_t) override {}
  void writeRepeat(uint16_t, uint32_t) override {}
  void writeBytes(uint8_t*, uint32_t) override {}
  void writePixels(uint16_t*, uint32_t) override {}
};

// Painel falso. O Arduino_Canvas::flush() chama draw16bitRGBBitmap() nele; o
// conteudo e copiado para o framebuffer 466x466 RGB565 da HAL (hal/panel.cpp).
// Assinatura do upstream: (bus, rst, rotation, w, h, colOff1, rowOff1, colOff2, rowOff2).
class Arduino_CO5300 : public Arduino_G {
 public:
  Arduino_CO5300(Arduino_DataBus*, int8_t /*rst*/, uint8_t /*rotation*/,
                 int16_t w, int16_t h, uint8_t /*colOff1*/, uint8_t /*rowOff1*/,
                 uint8_t /*colOff2*/, uint8_t /*rowOff2*/)
      : Arduino_G(w, h) {}
  bool begin(int32_t = GFX_NOT_DEFINED) override { return true; }
  void drawBitmap(int16_t, int16_t, uint8_t*, int16_t, int16_t, uint16_t, uint16_t) override {}
  void drawIndexedBitmap(int16_t, int16_t, uint8_t*, uint16_t*, int16_t, int16_t, int16_t = 0) override {}
  void draw3bitRGBBitmap(int16_t, int16_t, uint8_t*, int16_t, int16_t) override {}
  void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h) override;
  void draw24bitRGBBitmap(int16_t, int16_t, uint8_t*, int16_t, int16_t) override {}
  void setBrightness(uint8_t b);
};
