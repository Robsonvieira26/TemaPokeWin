#pragma once
// Modelo PURO do menu da bandeja (sem windows.h): construcao a partir das Settings,
// despacho de comandos para TrayActions e ajuste do tooltip. Testado em tests/test_tray.cpp.
#include <string>
#include <vector>
#include "win/tray.h"
#include "settings.h"

// IDs de comando (>= 1000 para nao colidir com comandos de sistema).
enum TrayCommand {
  kTrayToggleVisible = 1000,
  kTrayToggleSound,
  kTrayToggleAlwaysOnTop,
  kTrayScale05,
  kTrayScale10,
  kTrayScale20,
  kTrayToggleAutostart,
  kTrayQuit,
};

// Lista plana. Um submenu e: um item com submenuStart (id 0, so o rotulo), seguido dos seus
// itens, o ultimo deles com submenuEnd = true. Separador: separator = true (resto ignorado).
struct TrayMenuItem {
  int id = 0;
  std::string label;          // UTF-8
  bool checked = false;
  bool radio = false;         // item de radio (marca em bolinha)
  bool separator = false;
  bool submenuStart = false;
  bool submenuEnd = false;
};

// Ordem: Mostrar/Esconder, Som, Sempre no topo, Escala{0,5x 1x 2x}, Iniciar com o Windows,
// separador, Sair. Escala diferente de 0.5/1/2 marca 1x (mesma regra do Settings).
std::vector<TrayMenuItem> trayBuildMenu(const Settings& current);

// Mapeia o comando para a acao. Toggles leem `current`. id desconhecido ou actions nulo: no-op.
// Escolher a escala ja ativa ainda chama setScale (inofensivo; o AppShell ignora repeticao).
void trayDispatchCommand(int id, const Settings& current, TrayActions* actions);

// Limita o tooltip a 127 unidades UTF-16 (NOTIFYICONDATAW::szTip tem 128 com o terminador),
// sem cortar um par substituto ao meio.
std::wstring trayClampTip(const std::wstring& tip);
