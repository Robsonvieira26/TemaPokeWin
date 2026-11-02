#pragma once
// Bandeja do sistema. Sem SDL nem windows.h aqui: o AppShell implementa TrayActions
// e a Task 21 implementa de verdade as funcoes abaixo (Shell_NotifyIconW + TrackPopupMenu).
struct Settings;

struct TrayActions {
  virtual ~TrayActions() = default;
  virtual void toggleVisible() = 0;
  virtual void setSound(bool on) = 0;
  virtual void setAlwaysOnTop(bool on) = 0;
  virtual void setScale(float scale) = 0;
  virtual void setAutostart(bool on) = 0;
  virtual void quit() = 0;
};

bool trayInit(TrayActions* actions, const Settings& settings);
void trayShutdown();
void trayShowMenuAtCursor();
void traySetTooltip(const char* text);   // UTF-8
void traySyncChecks(const Settings& settings);
