#pragma once
// Logica pura do shell desktop-pet: sem SDL, sem windows.h. Testavel sem janela.
#include <cstdint>
#include <vector>

struct IGameHost;

enum class ShellState { Visible, Hidden };

struct TickPolicy {
  // 0 = chamar host->tick() continuamente: o firmware se auto-pacea (render ~10 fps, 85 ms no
  // minijogo, delay(1) entre passadas, toque lido a cada ~20 ms; ver upstream/power.h
  // frameIntervalMs/idleDelayMs e TamaPoke.ino loop()). Oculto: 1 Hz basta para o relogio.
  uint32_t tickIntervalMs(ShellState s) const { return s == ShellState::Visible ? 0u : 1000u; }
  bool shouldPresent(ShellState s) const { return s == ShellState::Visible; }
};

class ShellStateMachine {
 public:
  explicit ShellStateMachine(IGameHost* host) : host_(host) {}
  ShellState state() const { return state_; }
  void hide();                    // Visible->Hidden, salva (idempotente)
  void show();                    // ->Visible, zera o timer, um tick() se estava oculto
  void onCloseRequested();        // sempre salva, depois oculta
  void onElapsed(uint32_t ms);    // oculto: save a cada > 5 min
  void onInput();                 // oculto: pede um tick imediato
  bool consumeImmediateTick();
  void powerButton();             // so repassa ao firmware (ele e dono do screenOff)
  void onEscape() {}              // Esc nunca fecha o app
  void requestQuit();
  bool quitRequested() const { return quit_; }

  static constexpr uint32_t kSaveEveryMs = 300000;

 private:
  IGameHost* host_;
  ShellState state_ = ShellState::Visible;
  uint32_t elapsed_ = 0;
  bool immediate_ = false;
  bool quit_ = false;
};

enum class SwipeDir { Left, Right, Up, Down };

// Gesto de teclado com estado. Geometria/tempo escolhidos para o handleTouch do firmware
// (TamaPoke.ino): toque lido a cada 20 ms; ao soltar usa (ultimo - primeiro ponto) com
// |d|>80 no eixo e |d|<70 no outro e dt<800 ms. Nao ha zona de inicio exigida para swipes
// (inPetZone so vale para o toque longo). Aqui: 200 px no eixo, 0 no outro, 6 moves a
// cada 25 ms, soltar 25 ms depois do ultimo (~175 ms no total).
class SwipeSimulator {
 public:
  void start(SwipeDir d, uint32_t nowMs);   // ignorado enquanto ativo
  bool active() const { return active_; }
  void step(uint32_t nowMs, IGameHost* host);

  static constexpr int kMoves = 6;
  static constexpr uint32_t kStepMs = 25;

 private:
  bool active_ = false;
  int phase_ = 0;  // 0 = down, 1..kMoves = moves, kMoves+1 = up
  uint32_t due_ = 0;
  int x0_ = 0, y0_ = 0, x1_ = 0, y1_ = 0;
};

// Clique vs arrasto do botao direito, em coordenadas globais de tela.
class RightClickTracker {
 public:
  void down(int gx, int gy, uint32_t nowMs);
  // true (com delta) quando virou arrasto (>= 4 px do ponto de press). O 1o delta e desde o
  // press, os seguintes desde o ponto anterior.
  bool move(int gx, int gy, int* dx, int* dy);
  bool up(uint32_t nowMs);  // true sse foi clique (sem arrasto e <= 300 ms)
  bool dragging() const { return down_ && dragging_; }

 private:
  bool down_ = false, dragging_ = false;
  int sx_ = 0, sy_ = 0, lx_ = 0, ly_ = 0;
  uint32_t t0_ = 0;
};

struct ShellRect { int x, y, w, h; };

bool shellCenterVisible(int winX, int winY, int winW, int winH,
                        const std::vector<ShellRect>& displays);
void shellCenterOn(const ShellRect& display, int winW, int winH, int* x, int* y);
void shellResizeKeepCenter(int x, int y, int oldSide, int newSide, int* nx, int* ny);
