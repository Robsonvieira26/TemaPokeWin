#include "present.h"
#include "hal_control.h"
#include <SDL3/SDL.h>

bool presentInsideCircle(int x, int y) {
  const int r = HAL_SCREEN_W / 2;            // 233
  int dx = x - r, dy = y - r;
  return dx * dx + dy * dy <= r * r;
}

int presentToLogical(int windowPixel, float scale) {
  if (scale <= 0.0f) scale = 1.0f;
  int v = (int)((float)windowPixel / scale);
  if (v < 0) v = 0;
  if (v > HAL_SCREEN_W - 1) v = HAL_SCREEN_W - 1;
  return v;
}

uint32_t presentRgb565ToArgb(uint16_t p) {
  uint32_t r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
  uint32_t r = (r5 << 3) | (r5 >> 2);
  uint32_t g = (g6 << 2) | (g6 >> 4);
  uint32_t b = (b5 << 3) | (b5 >> 2);
  return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void presentConvertFrame(const uint16_t* in, uint32_t* out, bool circular) {
  for (int y = 0; y < HAL_SCREEN_H; ++y) {
    for (int x = 0; x < HAL_SCREEN_W; ++x) {
      size_t i = (size_t)y * HAL_SCREEN_W + x;
      out[i] = (circular && !presentInsideCircle(x, y))
                   ? 0x00000000u : presentRgb565ToArgb(in[i]);
    }
  }
}

bool Presenter::init(SDL_Window* window) {
  ren_ = SDL_CreateRenderer(window, nullptr);
  if (!ren_) return false;
  SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
  tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ARGB8888,
                           SDL_TEXTUREACCESS_STREAMING,
                           HAL_SCREEN_W, HAL_SCREEN_H);
  if (!tex_) return false;
  SDL_SetTextureScaleMode(tex_, SDL_SCALEMODE_NEAREST);
  SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
  buf_.assign((size_t)HAL_SCREEN_W * HAL_SCREEN_H, 0);
  return true;
}

void Presenter::shutdown() {
  if (tex_) { SDL_DestroyTexture(tex_); tex_ = nullptr; }
  if (ren_) { SDL_DestroyRenderer(ren_); ren_ = nullptr; }
}

void Presenter::setScale(float s) { scale_ = s > 0.0f ? s : 1.0f; }

void Presenter::draw(const uint16_t* frame, uint8_t brightness, bool circular) {
  if (!ren_ || !tex_ || !frame) return;
  // ARGB8888 porque a silhueta redonda precisa de alfa por pixel.
  presentConvertFrame(frame, buf_.data(), circular);
  SDL_UpdateTexture(tex_, nullptr, buf_.data(), HAL_SCREEN_W * (int)sizeof(uint32_t));
  SDL_SetTextureColorMod(tex_, brightness, brightness, brightness);

  SDL_SetRenderDrawColor(ren_, 0, 0, 0, 0);
  SDL_RenderClear(ren_);
  SDL_RenderTexture(ren_, tex_, nullptr, nullptr);
  SDL_RenderPresent(ren_);
}
