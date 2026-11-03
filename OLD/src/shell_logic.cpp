#include "shell_logic.h"
#include "game_host.h"
#include <string>

void ShellStateMachine::hide() {
  if (state_ == ShellState::Hidden) return;
  state_ = ShellState::Hidden;
  elapsed_ = 0;
  immediate_ = false;
  host_->save();
}

void ShellStateMachine::show() {
  if (state_ == ShellState::Visible) return;
  state_ = ShellState::Visible;
  elapsed_ = 0;
  immediate_ = false;
  host_->tick();  // o firmware ve a nova epoca do RTC apos ficar oculto
}

void ShellStateMachine::onCloseRequested() {
  host_->save();  // sempre: logoff com a janela oculta tambem precisa salvar
  if (state_ == ShellState::Visible) {
    state_ = ShellState::Hidden;
    elapsed_ = 0;
    immediate_ = false;
  }
}

void ShellStateMachine::onElapsed(uint32_t ms) {
  if (state_ != ShellState::Hidden) return;
  elapsed_ += ms;
  if (elapsed_ > kSaveEveryMs) {
    host_->save();
    elapsed_ = 0;
  }
}

void ShellStateMachine::onInput() {
  if (state_ == ShellState::Hidden) immediate_ = true;
}

bool ShellStateMachine::consumeImmediateTick() {
  bool r = immediate_;
  immediate_ = false;
  return r;
}

void ShellStateMachine::powerButton() { host_->powerShortPress(); }

void ShellStateMachine::requestQuit() {
  host_->save();
  quit_ = true;
}

// ---- SwipeSimulator ----

void SwipeSimulator::start(SwipeDir d, uint32_t nowMs) {
  if (active_) return;
  const int c = 233, half = 100;  // centro da tela 466x466; deslocamento de 200 px
  x0_ = x1_ = y0_ = y1_ = c;
  switch (d) {
    case SwipeDir::Left:  x0_ = c + half; x1_ = c - half; break;
    case SwipeDir::Right: x0_ = c - half; x1_ = c + half; break;
    case SwipeDir::Up:    y0_ = c + half; y1_ = c - half; break;
    case SwipeDir::Down:  y0_ = c - half; y1_ = c + half; break;
  }
  active_ = true;
  phase_ = 0;
  due_ = nowMs;
}

void SwipeSimulator::step(uint32_t nowMs, IGameHost* host) {
  if (!active_ || (int32_t)(nowMs - due_) < 0) return;
  if (phase_ == 0) {
    host->touchDown(x0_, y0_);
  } else if (phase_ <= kMoves) {
    host->touchMove(x0_ + (x1_ - x0_) * phase_ / kMoves, y0_ + (y1_ - y0_) * phase_ / kMoves);
  } else {
    host->touchUp();
    active_ = false;
    return;
  }
  ++phase_;
  due_ = nowMs + kStepMs;
}

// ---- RightClickTracker ----

void RightClickTracker::down(int gx, int gy, uint32_t nowMs) {
  down_ = true;
  dragging_ = false;
  sx_ = lx_ = gx;
  sy_ = ly_ = gy;
  t0_ = nowMs;
}

bool RightClickTracker::move(int gx, int gy, int* dx, int* dy) {
  if (!down_) return false;
  if (!dragging_) {
    int ax = gx - sx_, ay = gy - sy_;
    if (ax * ax + ay * ay < 16) return false;
    dragging_ = true;
  }
  *dx = gx - lx_;
  *dy = gy - ly_;
  lx_ = gx;
  ly_ = gy;
  return true;
}

bool RightClickTracker::up(uint32_t nowMs) {
  bool click = down_ && !dragging_ && nowMs - t0_ <= 300;
  down_ = false;
  dragging_ = false;
  return click;
}

// ---- posicao ----

bool shellCenterVisible(int winX, int winY, int winW, int winH,
                        const std::vector<ShellRect>& displays) {
  int cx = winX + winW / 2, cy = winY + winH / 2;
  for (const ShellRect& d : displays)
    if (cx >= d.x && cx < d.x + d.w && cy >= d.y && cy < d.y + d.h) return true;
  return false;
}

void shellCenterOn(const ShellRect& d, int winW, int winH, int* x, int* y) {
  *x = d.x + (d.w - winW) / 2;
  *y = d.y + (d.h - winH) / 2;
}

void shellResizeKeepCenter(int x, int y, int oldSide, int newSide, int* nx, int* ny) {
  int cx = x + oldSide / 2, cy = y + oldSide / 2;
  *nx = cx - newSide / 2;
  *ny = cy - newSide / 2;
}

std::string shellTooltipText(const char* name, int level) {
  if (!name || !*name) return "TamaPoke";
  std::string s = std::string("TamaPoke \xE2\x80\x94 ") + name;
  if (level >= 1) s += ", n\xC3\xADvel " + std::to_string(level);
  return s;
}
