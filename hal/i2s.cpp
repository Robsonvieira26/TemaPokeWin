#include "ESP_I2S.h"
#include "hal_control.h"
#include <SDL3/SDL.h>
#include <atomic>

namespace {
SDL_AudioStream* gStream = nullptr;
std::atomic<uint64_t> gBytes{0};
int gRate = 16000, gChannels = 2;
}  // namespace

uint64_t halAudioBytesWritten() { return gBytes.load(); }

bool I2SClass::begin(i2s_mode_t, uint32_t rate, i2s_data_bit_width_t,
                     i2s_slot_mode_t slots, i2s_std_slot_mask_t) {
  if (gStream) { SDL_DestroyAudioStream(gStream); gStream = nullptr; }
  gRate = (int)rate;
  gChannels = (slots == I2S_SLOT_MODE_STEREO) ? 2 : 1;
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;   // com contagem de referencias
  SDL_AudioSpec spec{};
  spec.format = SDL_AUDIO_S16;
  spec.channels = gChannels;
  spec.freq = gRate;
  gStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (!gStream) return false;                 // sem audio: o jogo segue mudo
  SDL_ResumeAudioStreamDevice(gStream);
  return true;
}

size_t I2SClass::write(const uint8_t* data, size_t len) {
  if (!gStream || !data || len == 0) return 0;

  // Segura o produtor quando a fila passa de ~100 ms, para o playTone do
  // upstream manter o ritmo em vez de encher a memoria.
  const int bytesPorSegundo = gRate * gChannels * 2;
  const int limite = bytesPorSegundo / 10;
  for (int espera = 0; espera < 50; ++espera) {
    if (SDL_GetAudioStreamQueued(gStream) <= limite) break;
    SDL_Delay(5);
  }

  if (!SDL_PutAudioStreamData(gStream, data, (int)len)) return 0;
  gBytes.fetch_add(len);
  return len;
}
