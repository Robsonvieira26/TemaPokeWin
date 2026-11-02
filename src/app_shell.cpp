#include "app_shell.h"
#include <SDL3/SDL.h>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "game_host.h"
#include "hal_control.h"
#include "win/autostart.h"
#include "win/single_instance.h"
#include "win/window_mode.h"

namespace {
constexpr const char* kAutostartName = "TamaPoke";
constexpr uint32_t kHiddenWaitCapMs = 100;  // teto da espera: mantem showRequested() responsivo

int sideFor(float scale) { return (int)std::lround(HAL_SCREEN_W * scale); }

std::vector<ShellRect> displayRects() {
  std::vector<ShellRect> out;
  int n = 0;
  SDL_DisplayID* ids = SDL_GetDisplays(&n);
  for (int i = 0; ids && i < n; ++i) {
    SDL_Rect r;
    if (SDL_GetDisplayBounds(ids[i], &r)) out.push_back({r.x, r.y, r.w, r.h});
  }
  SDL_free(ids);
  return out;
}
}  // namespace

std::string appAutostartCommand() {
  wchar_t w[MAX_PATH * 2] = {0};
  DWORD n = GetModuleFileNameW(nullptr, w, (DWORD)(sizeof w / sizeof w[0]));
  if (n == 0 || n >= sizeof w / sizeof w[0]) return std::string();
  char u8[MAX_PATH * 6] = {0};
  if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, u8, (int)sizeof u8, nullptr, nullptr))
    return std::string();
  return std::string("\"") + u8 + "\" --hidden";
}

AppShell::AppShell() = default;
AppShell::~AppShell() { shutdown(); }

int AppShell::side() const { return sideFor(settings_.scale); }

bool AppShell::init(IGameHost* host, const Settings& settings, SingleInstance* single) {
  host_ = host;
  single_ = single;
  settings_ = settings;
  const int sd = side();

  SDL_WindowFlags flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_UTILITY;
  if (settings_.alwaysOnTop) flags |= SDL_WINDOW_ALWAYS_ON_TOP;
  if (settings_.startHidden) flags |= SDL_WINDOW_HIDDEN;
  window_ = SDL_CreateWindow("TamaPoke", sd, sd, flags);
  if (!window_) return false;

  std::vector<ShellRect> displays = displayRects();
  int x = 0, y = 0;
  if (settings_.hasPosition &&
      shellCenterVisible(settings_.windowX, settings_.windowY, sd, sd, displays)) {
    x = settings_.windowX;
    y = settings_.windowY;
  } else {
    SDL_Rect r{0, 0, 1920, 1080};
    SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &r);
    shellCenterOn({r.x, r.y, r.w, r.h}, sd, sd, &x, &y);
  }
  SDL_SetWindowPosition(window_, x, y);
  SDL_SyncWindow(window_);
  windowModeApplyRound(window_);

  if (!presenter_.init(window_)) return false;
  presenter_.setScale(settings_.scale);

  sm_ = std::make_unique<ShellStateMachine>(host_);
  host_->setSoundEnabled(settings_.soundEnabled);
  if (settings_.startHidden) sm_->hide();
  trayUp_ = trayInit(this, &settings_);
  return true;
}

void AppShell::save() {
  if (window_) {
    int x = 0, y = 0;
    if (SDL_GetWindowPosition(window_, &x, &y)) {
      settings_.windowX = x;
      settings_.windowY = y;
      settings_.hasPosition = true;
    }
  }
  settingsSave(settings_);
}

void AppShell::hideWindow() {
  sm_->hide();
  SDL_HideWindow(window_);
}

void AppShell::showWindow() {
  SDL_ShowWindow(window_);
  SDL_RaiseWindow(window_);
  sm_->show();
}

void AppShell::run() {
  uint32_t last = (uint32_t)SDL_GetTicks();
  uint32_t lastTick = last - 1000;
  while (!sm_->quitRequested()) {
    uint32_t now = (uint32_t)SDL_GetTicks();
    sm_->onElapsed(now - last);
    last = now;

    SDL_PumpEvents();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) handleEvent(ev);
    if (sm_->quitRequested()) break;
    if (single_ && single_->showRequested()) showWindow();
    swipe_.step((uint32_t)SDL_GetTicks(), host_);

    if (sm_->state() == ShellState::Hidden) {
      uint32_t every = policy_.tickIntervalMs(ShellState::Hidden);
      uint32_t since = now - lastTick;
      if (!sm_->consumeImmediateTick() && since < every) {
        // Nao gira a 100%: dorme ate o proximo evento ou o proximo tick.
        uint32_t wait = std::min(every - since, kHiddenWaitCapMs);
        if (SDL_WaitEventTimeout(&ev, (Sint32)wait)) SDL_PushEvent(&ev);  // so se recebeu
        continue;
      }
    }
    lastTick = now;
    host_->tick();
    if (host_->wantsRestart()) restartHost();
    present();
  }
}

void AppShell::present() {
  if (!policy_.shouldPresent(sm_->state()) || !host_->frameDirty()) return;
  presenter_.draw(host_->frame(), host_->brightness(), false);  // a regiao recorta o circulo
  host_->clearFrameDirty();
}

void AppShell::restartHost() {
  const std::string dataDir = halDataDir(), sdRoot = halSdRoot();
  const bool wasHidden = sm_->state() == ShellState::Hidden;
  host_->save();
  destroyGameHost(host_);
  host_ = createGameHost(dataDir.c_str(), sdRoot.c_str());
  host_->boot();
  halClearRestartRequest();
  host_->setSoundEnabled(settings_.soundEnabled);
  sm_ = std::make_unique<ShellStateMachine>(host_);
  if (wasHidden) sm_->hide();
}

void AppShell::handleEvent(const SDL_Event& ev) {
  switch (ev.type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_QUIT:
      sm_->onCloseRequested();  // sempre salva; so o "Sair" da bandeja encerra o processo
      SDL_HideWindow(window_);
      break;
    case SDL_EVENT_TERMINATING:
      sm_->requestQuit();  // fim de sessao do Windows: salva e sai
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      sm_->onInput();
      handleMouseButton(ev);
      break;
    case SDL_EVENT_MOUSE_MOTION: {
      sm_->onInput();
      if (leftDown_) {
        const float s = presenter_.scale();
        host_->touchMove(presentToLogical((int)ev.motion.x, s),
                         presentToLogical((int)ev.motion.y, s));
      }
      if (rightClick_.dragging() || (ev.motion.state & SDL_BUTTON_RMASK)) {
        float gx, gy;
        SDL_GetGlobalMouseState(&gx, &gy);
        int dx, dy;
        if (rightClick_.move((int)gx, (int)gy, &dx, &dy)) windowModeMove(window_, dx, dy);
      }
      break;
    }
    case SDL_EVENT_KEY_DOWN:
      sm_->onInput();
      handleKey(ev);
      break;
    default:
      break;
  }
}

void AppShell::handleMouseButton(const SDL_Event& ev) {
  const bool down = ev.button.down;
  const uint32_t now = (uint32_t)SDL_GetTicks();
  if (ev.button.button == SDL_BUTTON_LEFT) {
    const float s = presenter_.scale();
    const int px = (int)ev.button.x, py = (int)ev.button.y;
    if (down && windowModeHit(px, py, s)) {
      leftDown_ = true;
      host_->touchDown(presentToLogical(px, s), presentToLogical(py, s));
    } else if (!down && leftDown_) {
      leftDown_ = false;
      host_->touchUp();
    }
  } else if (ev.button.button == SDL_BUTTON_RIGHT) {
    if (down) {
      float gx, gy;
      SDL_GetGlobalMouseState(&gx, &gy);
      rightClick_.down((int)gx, (int)gy, now);
    } else {
      const bool wasDrag = rightClick_.dragging();
      if (rightClick_.up(now)) trayShowMenuAtCursor();
      if (wasDrag) save();  // grava a posicao nova
    }
  }
}

void AppShell::handleKey(const SDL_Event& ev) {
  if (ev.key.repeat) return;
  const uint32_t now = (uint32_t)SDL_GetTicks();
  switch (ev.key.key) {
    case SDLK_F2: sm_->powerButton(); break;
    case SDLK_ESCAPE: sm_->onEscape(); break;
    case SDLK_LEFT: swipe_.start(SwipeDir::Left, now); break;
    case SDLK_RIGHT: swipe_.start(SwipeDir::Right, now); break;
    case SDLK_UP: swipe_.start(SwipeDir::Up, now); break;
    case SDLK_DOWN: swipe_.start(SwipeDir::Down, now); break;
    default: break;  // F12: Task 22b
  }
}

// ---- TrayActions ----

void AppShell::toggleVisible() {
  if (sm_->state() == ShellState::Visible) hideWindow(); else showWindow();
}

void AppShell::setSound(bool on) {
  settings_.soundEnabled = on;
  host_->setSoundEnabled(on);
  save();
}

void AppShell::setAlwaysOnTop(bool on) {
  settings_.alwaysOnTop = on;
  windowModeSetAlwaysOnTop(window_, on);
  save();
}

void AppShell::setScale(float scale) {
  const int oldSide = side(), newSide = sideFor(scale);
  int x = 0, y = 0, nx = 0, ny = 0;
  SDL_GetWindowPosition(window_, &x, &y);
  shellResizeKeepCenter(x, y, oldSide, newSide, &nx, &ny);
  settings_.scale = scale;
  presenter_.setScale(scale);
  SDL_SetWindowSize(window_, newSide, newSide);
  SDL_SetWindowPosition(window_, nx, ny);
  SDL_SyncWindow(window_);
  windowModeApplyRound(window_);  // a regiao depende do tamanho real
  save();
}

void AppShell::setAutostart(bool on) {
  settings_.autostart = on;
  if (autostartAllowed_) {
    if (on) {
      const std::string cmd = appAutostartCommand();
      if (!cmd.empty()) autostartEnable(kAutostartName, cmd.c_str());
    } else {
      autostartDisable(kAutostartName);
    }
  }
  save();
}

void AppShell::quit() { sm_->requestQuit(); }

void AppShell::shutdown() {
  if (!window_) return;
  save();
  if (trayUp_) trayShutdown();
  trayUp_ = false;
  presenter_.shutdown();
  SDL_DestroyWindow(window_);
  window_ = nullptr;
}
