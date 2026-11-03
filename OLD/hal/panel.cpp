#include "Arduino_GFX_Library.h"
#include "hal_control.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

namespace {
std::mutex gMu;
std::vector<uint16_t> gFb(HAL_SCREEN_W * HAL_SCREEN_H, 0);
std::atomic<bool> gDirty{false};
std::atomic<uint8_t> gBrightness{255};
}  // namespace

const uint16_t* halFrame() { return gFb.data(); }
bool halFrameDirty() { return gDirty.load(); }
void halClearFrameDirty() { gDirty.store(false); }
uint8_t halBrightness() { return gBrightness.load(); }

void Arduino_CO5300::setBrightness(uint8_t b) { gBrightness.store(b); }

void Arduino_CO5300::draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap,
                                        int16_t w, int16_t h) {
  if (!bitmap || w <= 0 || h <= 0) return;
  const int x0 = std::max<int>(0, x);
  const int x1 = std::min<int>(HAL_SCREEN_W, (int)x + w);
  std::lock_guard<std::mutex> g(gMu);
  if (x1 > x0) {
    for (int row = 0; row < h; ++row) {
      const int dy = y + row;
      if (dy < 0 || dy >= HAL_SCREEN_H) continue;
      std::copy(bitmap + (size_t)row * w + (x0 - x),
                bitmap + (size_t)row * w + (x1 - x),
                gFb.begin() + (size_t)dy * HAL_SCREEN_W + x0);
    }
  }
  gDirty.store(true);
}
