#pragma once
#include <cstddef>
#include <cstdint>

// So o que upstream/audio.cpp usa:
//   i2s.setPins(BCK, WS, DO, DI, MCK);
//   i2s.begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
//   i2s.write((uint8_t*)buf, bytes);
enum i2s_mode_t { I2S_MODE_STD = 0 };
enum i2s_data_bit_width_t { I2S_DATA_BIT_WIDTH_16BIT = 16 };
enum i2s_slot_mode_t { I2S_SLOT_MODE_STEREO = 2 };
enum i2s_std_slot_mask_t { I2S_STD_SLOT_BOTH = 3 };

class I2SClass {
 public:
  void setPins(int bclk, int ws, int dout, int din = -1, int mclk = -1) {
    (void)bclk; (void)ws; (void)dout; (void)din; (void)mclk;
  }
  // false se nenhum dispositivo de audio abrir: o jogo segue mudo.
  bool begin(i2s_mode_t mode, uint32_t rate, i2s_data_bit_width_t bits,
             i2s_slot_mode_t slots, i2s_std_slot_mask_t mask = I2S_STD_SLOT_BOTH);
  size_t write(const uint8_t* data, size_t len);
};
