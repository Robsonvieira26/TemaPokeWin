// tests/test_host.cpp - o firmware real (setup/loop) rodando headless sobre a HAL.
#include "framework.h"
#include "game_host.h"
#include "hal_control.h"
#include "Arduino.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

std::string dirLimpo(const char* nome) {
  auto d = fs::temp_directory_path() / nome;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d.string();
}

void removerDir(const std::string& d) {
  std::error_code ec;
  fs::remove_all(d, ec);
}

const char* kSd = "upstream/tools/sdcard";

struct HostGuard {
  IGameHost* h;
  explicit HostGuard(const std::string& data) {
    // sem som real nos testes (antes do primeiro audioBegin() do setup())
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    halTouchUp();
    halSetRtcOffsetForTest(0);
    h = createGameHost(data.c_str(), kSd);
    h->boot();
  }
  ~HostGuard() { destroyGameHost(h); }
};

// Chama tick() ate pred() ou estourar o prazo (o loop e em tempo real, ~10 fps).
template <class P>
bool ate(IGameHost* h, P pred, int ms = 4000) {
  auto fim = Clock::now() + std::chrono::milliseconds(ms);
  while (Clock::now() < fim) {
    h->tick();
    if (pred()) return true;
  }
  return false;
}

uint64_t fnv(const IGameHost* h) {
  const uint16_t* f = h->frame();
  uint64_t x = 1469598103934665603ULL;
  for (int i = 0; i < HAL_SCREEN_W * HAL_SCREEN_H; ++i) {
    x ^= f[i];
    x *= 1099511628211ULL;
  }
  return x;
}

size_t coresDistintas(const IGameHost* h) {
  std::set<uint16_t> s;
  const uint16_t* f = h->frame();
  for (int i = 0; i < HAL_SCREEN_W * HAL_SCREEN_H; ++i) s.insert(f[i]);
  return s.size();
}

// Deixa a tela assentar: ticks por um tempo, descartando frames.
void assentar(IGameHost* h, int ms = 600) {
  auto fim = Clock::now() + std::chrono::milliseconds(ms);
  while (Clock::now() < fim) h->tick();
}

void tocar(IGameHost* h, int x, int y) {
  h->touchDown(x, y);
  assentar(h, 150);
  h->touchUp();
  assentar(h, 150);
}

// Centro da 1a linha da tela de escolha do inicial (TamaPoke.ino: STARTER_ROW_Y=110,
// ROW_H=70; hit-area x 70..396).
constexpr int kStarterX = 233, kStarterY = 110 + 35;

}  // namespace

TEST(host, boot_e_ticks_produzem_frame) {
  std::string data = dirLimpo("tamapoke_host_a");
  {
    HostGuard g(data);
    g.h->clearFrameDirty();
    ASSERT_TRUE(ate(g.h, [&] { return g.h->frameDirty(); }));
    ASSERT_TRUE(g.h->frame() != nullptr);
    assentar(g.h);
    ASSERT_TRUE(coresDistintas(g.h) > 1);
  }
  removerDir(data);
}

TEST(host, toque_escolhe_o_inicial_e_muda_a_tela) {
  std::string data = dirLimpo("tamapoke_host_b");
  {
    HostGuard g(data);
    assentar(g.h);
    uint64_t antes = fnv(g.h);

    // toque fora de qualquer botao (canto): continua na tela de escolha
    tocar(g.h, 5, 5);
    assentar(g.h, 400);
    // a tela de escolha e estatica, entao o hash nao muda ao tocar fora
    ASSERT_EQ(fnv(g.h), antes);

    tocar(g.h, kStarterX, kStarterY);
    ASSERT_TRUE(ate(g.h, [&] { return fnv(g.h) != antes; }));
  }
  removerDir(data);
}

TEST(host, comando_de_console_eh_consumido) {
  std::string data = dirLimpo("tamapoke_host_c");
  {
    HostGuard g(data);
    assentar(g.h, 300);
    g.h->injectConsole("STATS");
    ASSERT_TRUE(Serial.available() > 0);
    ASSERT_TRUE(ate(g.h, [&] { return Serial.available() == 0; }));
  }
  removerDir(data);
}

TEST(host, save_e_recarga_preservam_o_bicho) {
  std::string data = dirLimpo("tamapoke_host_d");
  uint64_t starter = 0;
  {
    HostGuard g(data);
    assentar(g.h);
    starter = fnv(g.h);
    tocar(g.h, kStarterX, kStarterY);
    ASSERT_TRUE(ate(g.h, [&] { return fnv(g.h) != starter; }));
    g.h->save();
  }
  ASSERT_TRUE(fs::exists(data + "/nvs.json"));
  {
    HostGuard g(data);   // nao recomeca do zero: nao mostra a escolha de inicial
    assentar(g.h, 1000);
    ASSERT_TRUE(fnv(g.h) != starter);
    ASSERT_TRUE(coresDistintas(g.h) > 1);
  }
  removerDir(data);
}

TEST(host, dois_diretorios_nao_compartilham_save) {
  std::string a = dirLimpo("tamapoke_host_e1");
  std::string b = dirLimpo("tamapoke_host_e2");
  uint64_t starter = 0;
  {
    HostGuard g(a);
    assentar(g.h);
    starter = fnv(g.h);
    tocar(g.h, kStarterX, kStarterY);
    g.h->save();
  }
  {
    HostGuard g(b);   // diretorio virgem: volta a pedir o inicial
    assentar(g.h);
    ASSERT_EQ(fnv(g.h), starter);
  }
  removerDir(a);
  removerDir(b);
}
