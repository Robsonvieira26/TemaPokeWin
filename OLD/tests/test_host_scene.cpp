// tests/test_host_scene.cpp - o cenario do bicho segue a hora LOCAL (nao a UTC).
#include "framework.h"
#include "game_host.h"
#include "hal_control.h"
#include "wall_clock.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

// mesmo empacotamento RGB565 do macro C565 de TamaPoke.ino
constexpr uint16_t c565(int r, int g, int b) {
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void assentar(IGameHost* h, int ms) {
  auto fim = Clock::now() + std::chrono::milliseconds(ms);
  while (Clock::now() < fim) h->tick();
}

// Cor do topo do ceu (pixel 0,0) da cena do ovo com a hora local = hh:30.
uint16_t topoDoCeuAs(int hh, const char* nome) {
  auto dir = fs::temp_directory_path() / nome;
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  halTouchUp();
  RtcHoraLocalGuard hora(hh, 30);        // ANTES do boot: o firmware le o RTC no setup()
  IGameHost* h = createGameHost(dir.string().c_str(), "upstream/tools/sdcard");
  h->boot();
  assentar(h, 600);
  h->touchDown(233, 145); assentar(h, 150); h->touchUp(); assentar(h, 150);
  assentar(h, 1500);
  uint16_t px = h->frame()[0];
  destroyGameHost(h);
  fs::remove_all(dir, ec);
  return px;
}
}  // namespace

TEST(host_scene, ceu_segue_a_hora_local) {
  const uint16_t amanhecer = c565(0xd1, 0x6a, 0x86);   // h < 8
  const uint16_t dia       = c565(0x8f, 0xc8, 0xea);   // 8 <= h < 18
  const uint16_t entardecer = c565(0xc7, 0x5a, 0x4a);  // h >= 18
  const uint16_t noite     = c565(0x0c, 0x12, 0x24);   // h < 6 || h >= 20
  CHECK_EQ(topoDoCeuAs(7, "tamapoke_scene_07"), amanhecer);
  CHECK_EQ(topoDoCeuAs(12, "tamapoke_scene_12"), dia);
  CHECK_EQ(topoDoCeuAs(19, "tamapoke_scene_19"), entardecer);
  CHECK_EQ(topoDoCeuAs(23, "tamapoke_scene_23"), noite);
  CHECK_EQ(topoDoCeuAs(3, "tamapoke_scene_03"), noite);
}
