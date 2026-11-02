#include "win/tray_menu.h"
#include <cmath>

namespace {
TrayMenuItem item(int id, const char* label, bool checked = false, bool radio = false) {
  TrayMenuItem it;
  it.id = id;
  it.label = label;
  it.checked = checked;
  it.radio = radio;
  return it;
}
}  // namespace

std::vector<TrayMenuItem> trayBuildMenu(const Settings& s) {
  bool s05 = std::fabs(s.scale - 0.5f) < 1e-3f;
  bool s20 = std::fabs(s.scale - 2.0f) < 1e-3f;
  bool s10 = !s05 && !s20;
  std::vector<TrayMenuItem> m;
  m.push_back(item(kTrayToggleVisible, "Mostrar/Esconder"));
  m.push_back(item(kTrayToggleSound, "Som", s.soundEnabled));
  m.push_back(item(kTrayToggleAlwaysOnTop, "Sempre no topo", s.alwaysOnTop));
  TrayMenuItem sub = item(0, "Escala");
  sub.submenuStart = true;
  m.push_back(sub);
  m.push_back(item(kTrayScale05, "0,5x", s05, true));
  m.push_back(item(kTrayScale10, "1x", s10, true));
  TrayMenuItem last = item(kTrayScale20, "2x", s20, true);
  last.submenuEnd = true;
  m.push_back(last);
  m.push_back(item(kTrayToggleAutostart, "Iniciar com o Windows", s.autostart));
  TrayMenuItem sep;
  sep.separator = true;
  m.push_back(sep);
  m.push_back(item(kTrayQuit, "Sair"));
  return m;
}

void trayDispatchCommand(int id, const Settings& cur, TrayActions* a) {
  if (!a) return;
  switch (id) {
    case kTrayToggleVisible: a->toggleVisible(); break;
    case kTrayToggleSound: a->setSound(!cur.soundEnabled); break;
    case kTrayToggleAlwaysOnTop: a->setAlwaysOnTop(!cur.alwaysOnTop); break;
    case kTrayScale05: a->setScale(0.5f); break;
    case kTrayScale10: a->setScale(1.0f); break;
    case kTrayScale20: a->setScale(2.0f); break;
    case kTrayToggleAutostart: a->setAutostart(!cur.autostart); break;
    case kTrayQuit: a->quit(); break;
    default: break;
  }
}

std::wstring trayClampTip(const std::wstring& tip) {
  const size_t kMax = 127;
  if (tip.size() <= kMax) return tip;
  size_t n = kMax;
  // Se o ultimo caractere mantido e um high surrogate, o par seria cortado: descarta-o.
  if (tip[n - 1] >= 0xD800 && tip[n - 1] <= 0xDBFF) --n;
  return tip.substr(0, n);
}
