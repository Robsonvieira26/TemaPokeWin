// tests/test_shell_logic.cpp - logica pura do shell (sem SDL de janela, sem AppShell).
#include "framework.h"
#include "wall_clock.h"
#include "shell_logic.h"
#include "game_host.h"
#include "win/tray.h"
#include "settings.h"
#include "hal_control.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

struct Ev {
  enum Kind { Tick, Save, Pwr, Down, Move, Up } k;
  int x = 0, y = 0;
};

struct FakeHost : IGameHost {
  std::vector<Ev> log;
  void boot() override {}
  void tick() override { log.push_back({Ev::Tick}); }
  bool frameDirty() const override { return false; }
  const uint16_t* frame() const override { return nullptr; }
  uint8_t brightness() const override { return 255; }
  void touchDown(int x, int y) override { log.push_back({Ev::Down, x, y}); }
  void touchMove(int x, int y) override { log.push_back({Ev::Move, x, y}); }
  void touchUp() override { log.push_back({Ev::Up}); }
  void powerShortPress() override { log.push_back({Ev::Pwr}); }
  void injectConsole(const char*) override {}
  const char* petName() const override { return ""; }
  int petLevel() const override { return 0; }
  void save() override { log.push_back({Ev::Save}); }
  void setSoundEnabled(bool) override {}
  bool wantsRestart() const override { return false; }
  void clearFrameDirty() override {}
  int count(Ev::Kind k) const {
    int n = 0;
    for (auto& e : log) n += e.k == k;
    return n;
  }
};

}  // namespace

TEST(shell_policy, valores) {
  TickPolicy p;
  CHECK_EQ(p.tickIntervalMs(ShellState::Visible), 0u);
  CHECK_EQ(p.tickIntervalMs(ShellState::Hidden), 1000u);
  CHECK(p.shouldPresent(ShellState::Visible));
  CHECK(!p.shouldPresent(ShellState::Hidden));
}

TEST(shell_state, hide_salva_uma_vez_e_eh_idempotente) {
  FakeHost h;
  ShellStateMachine sm(&h);
  CHECK(sm.state() == ShellState::Visible);
  sm.hide();
  CHECK(sm.state() == ShellState::Hidden);
  CHECK_EQ(h.count(Ev::Save), 1);
  sm.hide();
  CHECK_EQ(h.count(Ev::Save), 1);
}

TEST(shell_state, show_tica_exatamente_uma_vez) {
  FakeHost h;
  ShellStateMachine sm(&h);
  sm.hide();
  sm.show();
  CHECK(sm.state() == ShellState::Visible);
  CHECK_EQ(h.count(Ev::Tick), 1);
  sm.show();  // ja visivel: nao tica de novo
  CHECK_EQ(h.count(Ev::Tick), 1);
}

TEST(shell_state, fechar_sempre_salva_mesmo_oculto) {
  FakeHost h;
  ShellStateMachine sm(&h);
  sm.onCloseRequested();
  CHECK(sm.state() == ShellState::Hidden);
  CHECK_EQ(h.count(Ev::Save), 1);  // um unico save (hide nao duplica)
  sm.onCloseRequested();           // logoff com a janela ja oculta
  CHECK_EQ(h.count(Ev::Save), 2);
  CHECK(sm.state() == ShellState::Hidden);
  CHECK(!sm.quitRequested());
}

TEST(shell_state, save_periodico_no_limite) {
  FakeHost h;
  ShellStateMachine sm(&h);
  sm.hide();
  int base = h.count(Ev::Save);
  sm.onElapsed(300000);
  CHECK_EQ(h.count(Ev::Save), base);  // exatamente 5 min: ainda nao
  sm.onElapsed(1);
  CHECK_EQ(h.count(Ev::Save), base + 1);
  sm.onElapsed(300000);               // recomecou do zero
  CHECK_EQ(h.count(Ev::Save), base + 1);
  sm.onElapsed(1);
  CHECK_EQ(h.count(Ev::Save), base + 2);
}

TEST(shell_state, timer_reinicia_no_show_e_ignora_visivel) {
  FakeHost h;
  ShellStateMachine sm(&h);
  sm.onElapsed(999999);  // Visible: ignorado
  CHECK_EQ(h.count(Ev::Save), 0);
  sm.hide();
  sm.onElapsed(290000);
  sm.show();
  sm.hide();
  int base = h.count(Ev::Save);
  sm.onElapsed(290000);  // se nao tivesse reiniciado, passaria de 300000
  CHECK_EQ(h.count(Ev::Save), base);
}

TEST(shell_state, entrada_pede_tick_so_oculto_e_consome) {
  FakeHost h;
  ShellStateMachine sm(&h);
  sm.onInput();
  CHECK(!sm.consumeImmediateTick());  // visivel: nunca
  sm.hide();
  CHECK(!sm.consumeImmediateTick());
  sm.onInput();
  CHECK(sm.consumeImmediateTick());
  CHECK(!sm.consumeImmediateTick());
}

TEST(shell_state, power_repassa_sem_mudar_estado) {
  FakeHost h;
  ShellStateMachine sm(&h);
  sm.powerButton();
  CHECK_EQ(h.count(Ev::Pwr), 1);
  CHECK(sm.state() == ShellState::Visible);
  sm.hide();
  sm.powerButton();
  CHECK_EQ(h.count(Ev::Pwr), 2);
  CHECK(sm.state() == ShellState::Hidden);
}

TEST(shell_state, esc_nao_faz_nada_e_quit_salva) {
  FakeHost h;
  ShellStateMachine sm(&h);
  sm.onEscape();
  CHECK(h.log.empty());
  CHECK(sm.state() == ShellState::Visible);
  CHECK(!sm.quitRequested());
  sm.requestQuit();
  CHECK_EQ(h.count(Ev::Save), 1);
  CHECK(sm.quitRequested());
}

namespace {
struct Run {
  std::vector<Ev> ev;
  std::vector<uint32_t> t;
};
// roda o simulador a cada 5 ms ate acabar
Run rodar(SwipeSimulator& s, FakeHost& h, uint32_t t0) {
  Run r;
  size_t seen = 0;
  for (uint32_t t = t0; t < t0 + 2000 && (s.active() || t == t0); t += 5) {
    s.step(t, &h);
    for (; seen < h.log.size(); ++seen) { r.ev.push_back(h.log[seen]); r.t.push_back(t); }
  }
  return r;
}
void checaSwipe(SwipeDir d, bool horizontal) {
  FakeHost h;
  SwipeSimulator s;
  s.start(d, 1000);
  CHECK(s.active());
  Run r = rodar(s, h, 1000);
  CHECK(!s.active());
  CHECK(r.ev.size() >= 4);
  CHECK(r.ev.front().k == Ev::Down);
  CHECK(r.ev.back().k == Ev::Up);
  int moves = 0;
  for (size_t i = 1; i + 1 < r.ev.size(); ++i) {
    CHECK(r.ev[i].k == Ev::Move);
    ++moves;
    CHECK_MSG(r.t[i] - r.t[i - 1] >= 20, "moves espacados >= 20 ms");
  }
  CHECK(moves >= 5);
  CHECK(r.t.back() - r.t.front() < 800);
  // geometria aceita pelo firmware: do toque inicial ao ultimo move
  const Ev& a = r.ev.front();
  const Ev& b = r.ev[r.ev.size() - 2];
  int dx = b.x - a.x, dy = b.y - a.y;
  if (horizontal) CHECK(std::abs(dx) > 80 && std::abs(dy) < 70);
  else CHECK(std::abs(dy) > 80 && std::abs(dx) < 70);
  if (d == SwipeDir::Left) CHECK(dx < 0);
  if (d == SwipeDir::Right) CHECK(dx > 0);
  if (d == SwipeDir::Up) CHECK(dy < 0);
  if (d == SwipeDir::Down) CHECK(dy > 0);
  for (auto& e : r.ev)
    if (e.k != Ev::Up) CHECK(e.x >= 0 && e.x < 466 && e.y >= 0 && e.y < 466);
}
}  // namespace

TEST(shell_swipe, quatro_direcoes) {
  checaSwipe(SwipeDir::Left, true);
  checaSwipe(SwipeDir::Right, true);
  checaSwipe(SwipeDir::Up, false);
  checaSwipe(SwipeDir::Down, false);
}

TEST(shell_swipe, silencioso_cedo_ignora_start_e_reusa) {
  FakeHost h;
  SwipeSimulator s;
  s.step(0, &h);  // inativo: nada
  CHECK(h.log.empty());
  s.start(SwipeDir::Left, 100);
  s.step(100, &h);
  CHECK_EQ(h.log.size(), 1u);
  s.step(110, &h);  // cedo demais
  CHECK_EQ(h.log.size(), 1u);
  s.start(SwipeDir::Right, 110);  // ignorado
  s.step(125, &h);
  CHECK(h.log.size() == 2 && h.log[1].k == Ev::Move);
  CHECK(h.log[1].x < h.log[0].x);  // continua sendo Left
  for (uint32_t t = 130; s.active(); t += 5) s.step(t, &h);
  size_t n = h.log.size();
  s.start(SwipeDir::Up, 1000);
  CHECK(s.active());
  s.step(1000, &h);
  CHECK_EQ(h.log.size(), n + 1);
}

TEST(shell_rightclick, clique_e_arrasto) {
  RightClickTracker c;
  int dx = 99, dy = 99;
  c.down(100, 100, 1000);
  CHECK(!c.move(102, 101, &dx, &dy));
  CHECK(!c.dragging());
  CHECK(c.up(1100));

  c.down(100, 100, 1000);  // soltar devagar
  CHECK(!c.up(1400));

  c.down(100, 100, 1000);  // arrasto
  CHECK(c.move(104, 100, &dx, &dy));
  CHECK(c.dragging());
  CHECK_EQ(dx, 4);
  CHECK_EQ(dy, 0);
  CHECK(c.move(110, 95, &dx, &dy));
  CHECK_EQ(dx, 6);
  CHECK_EQ(dy, -5);
  CHECK(!c.up(1050));  // arrasto nunca eh clique

  CHECK(!c.dragging());
  CHECK(!c.move(500, 500, &dx, &dy));  // sem botao
}

TEST(shell_place, centro_visivel) {
  std::vector<ShellRect> d = {{0, 0, 1920, 1080}};
  CHECK(shellCenterVisible(100, 100, 466, 466, d));
  CHECK(!shellCenterVisible(3000, 100, 466, 466, d));
  CHECK(!shellCenterVisible(-500, 100, 466, 466, d));  // centro em x = -267
  CHECK(shellCenterVisible(-200, 100, 466, 466, d));   // centro em x = 33
  CHECK(!shellCenterVisible(1700, 100, 466, 466, d));  // centro em x = 1933
}

TEST(shell_place, segundo_monitor_negativo) {
  std::vector<ShellRect> d = {{0, 0, 1920, 1080}, {-1920, 0, 1920, 1080}};
  CHECK(shellCenterVisible(-1500, 200, 466, 466, d));
  CHECK(!shellCenterVisible(-4000, 200, 466, 466, d));
  CHECK(!shellCenterVisible(0, 5000, 466, 466, d));
  CHECK(!shellCenterVisible(0, 0, 466, 466, {}));
}

TEST(shell_place, centralizar_e_redimensionar) {
  int x, y;
  shellCenterOn({-1920, 100, 1920, 1080}, 466, 466, &x, &y);
  CHECK_EQ(x, -1920 + (1920 - 466) / 2);
  CHECK_EQ(y, 100 + (1080 - 466) / 2);
  int nx, ny;
  shellResizeKeepCenter(100, 200, 240, 480, &nx, &ny);
  CHECK_EQ(nx, -20);
  CHECK_EQ(ny, 80);
  shellResizeKeepCenter(nx, ny, 480, 960, &nx, &ny);
  CHECK_EQ(nx, -260);
  CHECK_EQ(ny, -160);
  shellResizeKeepCenter(nx, ny, 960, 480, &nx, &ny);
  shellResizeKeepCenter(nx, ny, 480, 240, &nx, &ny);
  CHECK_EQ(nx, 100);
  CHECK_EQ(ny, 200);
}

// (o antigo teste do stub da bandeja foi substituido por tests/test_tray.cpp)

// ---- contra o firmware real ----
namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

uint64_t hashFrame(const IGameHost* h) {
  const uint16_t* f = h->frame();
  uint64_t x = 1469598103934665603ULL;
  for (int i = 0; i < HAL_SCREEN_W * HAL_SCREEN_H; ++i) { x ^= f[i]; x *= 1099511628211ULL; }
  return x;
}
void assentar(IGameHost* h, int ms) {
  auto fim = Clock::now() + std::chrono::milliseconds(ms);
  while (Clock::now() < fim) h->tick();
}
// roda o swipe em tempo real (relogio local), ticando o host como o laco do shell
void nadar(IGameHost* h, SwipeDir d) {
  SwipeSimulator s;
  auto t0 = Clock::now();
  auto agora = [&] {
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
  };
  s.start(d, agora());
  while (s.active()) { s.step(agora(), h); h->tick(); }
}
}  // namespace

TEST(shell_swipe, firmware_real_abre_a_dex) {
  auto dir = fs::temp_directory_path() / "tamapoke_shell_swipe";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  halTouchUp();
  // meio-dia local: nuvens animadas (h < 18) em qualquer hora real do dia
  RtcHoraLocalGuard meioDia(12, 30);
  IGameHost* h = createGameHost(dir.string().c_str(), "upstream/tools/sdcard");
  h->boot();
  assentar(h, 600);
  // escolhe o inicial (1a linha, como em test_host.cpp)
  h->touchDown(233, 145); assentar(h, 150); h->touchUp(); assentar(h, 150);
  assentar(h, 1500);
  uint64_t antes = hashFrame(h);
  assentar(h, 800);
  // controle: a tela do bicho anima sozinha (hash instavel); a galeria e estatica
  CHECK_MSG(hashFrame(h) != antes, "controle: tela principal anima sem swipe");
  nadar(h, SwipeDir::Left);
  assentar(h, 800);
  uint64_t depois = hashFrame(h);
  CHECK_MSG(depois != antes, "swipe Left deve abrir a galeria");
  assentar(h, 500);
  CHECK_MSG(hashFrame(h) == depois, "galeria aberta e estatica");
  // Right a partir da pagina 0 sai da galeria
  nadar(h, SwipeDir::Right);
  assentar(h, 800);
  CHECK_MSG(hashFrame(h) != depois, "swipe Right deve fechar a galeria");
  assentar(h, 500);
  uint64_t r1 = hashFrame(h);
  assentar(h, 500);
  CHECK_MSG(hashFrame(h) != r1, "de volta a tela principal animada");
  destroyGameHost(h);
  fs::remove_all(dir, ec);
}

TEST(shell_host, nome_e_nivel_do_bicho) {
  auto dir = fs::temp_directory_path() / "tamapoke_shell_petname";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  halTouchUp();
  halSetRtcOffsetForTest(0);
  IGameHost* h = createGameHost(dir.string().c_str(), "upstream/tools/sdcard");
  CHECK_STREQ(h->petName(), "");  // antes do boot
  CHECK_EQ(h->petLevel(), 0);
  h->boot();
  assentar(h, 600);
  CHECK_STREQ(h->petName(), "");  // sem inicial escolhido: ovo
  CHECK_EQ(h->petLevel(), 0);
  h->touchDown(233, 145); assentar(h, 150); h->touchUp(); assentar(h, 150);
  assentar(h, 1500);
  h->injectConsole("HATCH");
  assentar(h, 1500);
  std::string n = h->petName();
  CHECK(!n.empty());
  CHECK(h->petLevel() >= 1);
  h->injectConsole("NICK Bolt");
  assentar(h, 300);
  CHECK_STREQ(h->petName(), "Bolt");
  destroyGameHost(h);
  fs::remove_all(dir, ec);
}
