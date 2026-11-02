#pragma once
// Cola SDL do desktop pet: janela, laco de eventos, mouse/teclado e acoes da bandeja.
// A logica de estados/gestos vive em shell_logic.h (pura e testada); aqui so ha SDL.
#include <memory>
#include <string>
#include "present.h"
#include "settings.h"
#include "shell_logic.h"
#include "win/tray.h"

struct IGameHost;
class SingleInstance;
struct SDL_Window;
union SDL_Event;

class AppShell : public TrayActions {
 public:
  AppShell();
  ~AppShell() override;

  // `host` e `single` pertencem ao chamador (single pode ser nulo). Em reinicio pedido
  // pelo firmware o host e recriado: use host() para destruir o atual.
  bool init(IGameHost* host, const Settings& settings, SingleInstance* single);
  void run();
  void shutdown();
  IGameHost* host() const { return host_; }
  // Desliga as chamadas ao registro Run (--no-autostart).
  void setAutostartEnabled(bool allowed) { autostartAllowed_ = allowed; }

  // TrayActions
  void toggleVisible() override;
  void setSound(bool on) override;
  void setAlwaysOnTop(bool on) override;
  void setScale(float scale) override;
  void setAutostart(bool on) override;
  void quit() override;

 private:
  void handleEvent(const SDL_Event& ev);
  void handleMouseButton(const SDL_Event& ev);
  void handleKey(const SDL_Event& ev);
  void restartHost();
  void present();
  void save();
  void hideWindow();
  void showWindow();
  int side() const;

  IGameHost* host_ = nullptr;
  SingleInstance* single_ = nullptr;
  SDL_Window* window_ = nullptr;
  Presenter presenter_;
  TickPolicy policy_;
  std::unique_ptr<ShellStateMachine> sm_;
  SwipeSimulator swipe_;
  RightClickTracker rightClick_;
  Settings settings_;
  bool leftDown_ = false;
  bool autostartAllowed_ = true;
  bool trayUp_ = false;
};

// Linha de comando gravada no registro Run: "<caminho do exe>" --hidden (UTF-8).
std::string appAutostartCommand();
