// tests/test_tray.cpp
// Suites "tray_menu" (modelo puro) e "tray_win32" (icone real na bandeja, sem input do usuario).
// O teste Win32 nunca chama trayShowMenuAtCursor() com a bandeja ativa (abriria um menu modal).
#include "framework.h"
#include "settings.h"
#include "win/tray.h"
#include "win/tray_menu.h"
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct FakeActions : TrayActions {
  std::vector<std::string> calls;
  void toggleVisible() override { calls.push_back("toggleVisible"); }
  void setSound(bool on) override { calls.push_back(std::string("setSound:") + (on ? "1" : "0")); }
  void setAlwaysOnTop(bool on) override { calls.push_back(std::string("setAlwaysOnTop:") + (on ? "1" : "0")); }
  void setScale(float s) override {
    calls.push_back(s == 0.5f ? "setScale:0.5" : s == 1.0f ? "setScale:1" : s == 2.0f ? "setScale:2" : "setScale:?");
  }
  void setAutostart(bool on) override { calls.push_back(std::string("setAutostart:") + (on ? "1" : "0")); }
  void quit() override { calls.push_back("quit"); }
};

const TrayMenuItem* findItem(const std::vector<TrayMenuItem>& m, int id) {
  for (const auto& it : m) if (it.id == id) return &it;
  return nullptr;
}

int countRadioChecked(const std::vector<TrayMenuItem>& m) {
  int n = 0;
  for (const auto& it : m) if (it.radio && it.checked) ++n;
  return n;
}

}  // namespace

// ---------------- modelo puro ----------------

TEST(tray_menu, default_settings_layout) {
  Settings s;  // scale 1, topo, autostart, som
  auto m = trayBuildMenu(s);
  // Ordem: Mostrar, Som, Topo, [Escala: 0,5x 1x 2x], Autostart, separador, Sair
  std::vector<std::string> labels;
  for (const auto& it : m) labels.push_back(it.separator ? "-" : it.label);
  std::vector<std::string> want = {"Mostrar/Esconder", "Som", "Sempre no topo", "Escala",
                                   "0,5x", "1x", "2x", "Iniciar com o Windows", "-", "Sair"};
  CHECK_EQ(labels.size(), want.size());
  for (size_t i = 0; i < want.size() && i < labels.size(); ++i) CHECK_EQ(labels[i], want[i]);

  CHECK(m.size() == want.size());
  if (m.size() != want.size()) return;
  CHECK(m[3].submenuStart);
  CHECK(!m[3].submenuEnd);
  CHECK(!m[4].submenuEnd && !m[5].submenuEnd);
  CHECK(m[6].submenuEnd);  // o ultimo item do submenu fecha-o
  CHECK(m[8].separator);
  CHECK_EQ(m[9].id, (int)kTrayQuit);
  CHECK_EQ(m[0].id, (int)kTrayToggleVisible);
  CHECK_EQ(m[1].id, (int)kTrayToggleSound);
  CHECK_EQ(m[2].id, (int)kTrayToggleAlwaysOnTop);
  CHECK_EQ(m[4].id, (int)kTrayScale05);
  CHECK_EQ(m[5].id, (int)kTrayScale10);
  CHECK_EQ(m[6].id, (int)kTrayScale20);
  CHECK_EQ(m[7].id, (int)kTrayToggleAutostart);
  CHECK(m[1].checked);
  CHECK(m[2].checked);
  CHECK(m[7].checked);
  CHECK(!m[0].checked);
  CHECK(!m[9].checked);
  CHECK(m[4].radio && m[5].radio && m[6].radio);
  CHECK(!m[4].checked && m[5].checked && !m[6].checked);
  CHECK_EQ(countRadioChecked(m), 1);
}

TEST(tray_menu, ids_are_unique_and_large) {
  auto m = trayBuildMenu(Settings{});
  std::vector<int> seen;
  for (const auto& it : m) {
    if (it.separator || it.submenuStart) continue;
    CHECK(it.id >= 1000);
    for (int o : seen) CHECK(o != it.id);
    seen.push_back(it.id);
  }
}

TEST(tray_menu, labels_have_accents_in_utf8_where_natural) {
  // "Iniciar com o Windows" nao precisa de acento; garante so que nada fica vazio.
  for (const auto& it : trayBuildMenu(Settings{}))
    if (!it.separator) CHECK(!it.label.empty());
}

TEST(tray_menu, variations_reflect_settings) {
  Settings s;
  s.soundEnabled = false;
  s.alwaysOnTop = false;
  s.autostart = false;
  s.scale = 0.5f;
  auto m = trayBuildMenu(s);
  CHECK(!findItem(m, kTrayToggleSound)->checked);
  CHECK(!findItem(m, kTrayToggleAlwaysOnTop)->checked);
  CHECK(!findItem(m, kTrayToggleAutostart)->checked);
  CHECK(findItem(m, kTrayScale05)->checked);
  CHECK(!findItem(m, kTrayScale10)->checked);
  CHECK(!findItem(m, kTrayScale20)->checked);
  CHECK_EQ(countRadioChecked(m), 1);

  s.scale = 2.0f;
  m = trayBuildMenu(s);
  CHECK(findItem(m, kTrayScale20)->checked);
  CHECK_EQ(countRadioChecked(m), 1);

  s.scale = 1.0f;
  m = trayBuildMenu(s);
  CHECK(findItem(m, kTrayScale10)->checked);
  CHECK_EQ(countRadioChecked(m), 1);
}

TEST(tray_menu, odd_scale_falls_back_to_1x) {
  Settings s;
  s.scale = 3.7f;
  auto m = trayBuildMenu(s);
  CHECK(findItem(m, kTrayScale10)->checked);
  CHECK_EQ(countRadioChecked(m), 1);
}

TEST(tray_menu, dispatch_every_command) {
  Settings s;  // som ligado, topo ligado, autostart ligado
  {
    FakeActions a; trayDispatchCommand(kTrayToggleVisible, s, &a);
    CHECK_EQ(a.calls.size(), (size_t)1); CHECK_EQ(a.calls[0], "toggleVisible");
  }
  {
    FakeActions a; trayDispatchCommand(kTrayToggleSound, s, &a);
    CHECK_EQ(a.calls.size(), (size_t)1); CHECK_EQ(a.calls[0], "setSound:0");
  }
  {
    FakeActions a; trayDispatchCommand(kTrayToggleAlwaysOnTop, s, &a);
    CHECK_EQ(a.calls.size(), (size_t)1); CHECK_EQ(a.calls[0], "setAlwaysOnTop:0");
  }
  {
    FakeActions a; trayDispatchCommand(kTrayToggleAutostart, s, &a);
    CHECK_EQ(a.calls.size(), (size_t)1); CHECK_EQ(a.calls[0], "setAutostart:0");
  }
  {
    FakeActions a; trayDispatchCommand(kTrayScale05, s, &a);
    CHECK_EQ(a.calls.size(), (size_t)1); CHECK_EQ(a.calls[0], "setScale:0.5");
  }
  {
    FakeActions a; trayDispatchCommand(kTrayScale20, s, &a);
    CHECK_EQ(a.calls.size(), (size_t)1); CHECK_EQ(a.calls[0], "setScale:2");
  }
  {
    FakeActions a; trayDispatchCommand(kTrayQuit, s, &a);
    CHECK_EQ(a.calls.size(), (size_t)1); CHECK_EQ(a.calls[0], "quit");
  }
}

TEST(tray_menu, dispatch_toggles_read_current_settings) {
  Settings s;
  s.soundEnabled = false; s.alwaysOnTop = false; s.autostart = false;
  FakeActions a;
  trayDispatchCommand(kTrayToggleSound, s, &a);
  trayDispatchCommand(kTrayToggleAlwaysOnTop, s, &a);
  trayDispatchCommand(kTrayToggleAutostart, s, &a);
  CHECK_EQ(a.calls.size(), (size_t)3);
  if (a.calls.size() == 3) {
    CHECK_EQ(a.calls[0], "setSound:1");
    CHECK_EQ(a.calls[1], "setAlwaysOnTop:1");
    CHECK_EQ(a.calls[2], "setAutostart:1");
  }
}

TEST(tray_menu, dispatch_reselecting_current_scale_still_forwards) {
  Settings s;  // 1x
  FakeActions a;
  trayDispatchCommand(kTrayScale10, s, &a);
  CHECK_EQ(a.calls.size(), (size_t)1);
  CHECK_EQ(a.calls[0], "setScale:1");
}

TEST(tray_menu, dispatch_unknown_and_null_are_noops) {
  Settings s;
  FakeActions a;
  trayDispatchCommand(0, s, &a);
  trayDispatchCommand(-5, s, &a);
  trayDispatchCommand(99999, s, &a);
  trayDispatchCommand(kTrayScale20 + 1000, s, &a);
  CHECK(a.calls.empty());
  trayDispatchCommand(kTrayQuit, s, nullptr);  // nao pode quebrar
}

TEST(tray_menu, tip_short_untouched_and_empty) {
  CHECK(trayClampTip(L"TamaPoke") == L"TamaPoke");
  CHECK(trayClampTip(L"").empty());
  CHECK(trayClampTip(std::wstring(127, L'a')) == std::wstring(127, L'a'));
}

TEST(tray_menu, tip_long_is_clamped_to_127) {
  std::wstring r = trayClampTip(std::wstring(300, L'x'));
  CHECK_EQ(r.size(), (size_t)127);
}

TEST(tray_menu, tip_does_not_split_surrogate_pair) {
  // U+1F600 = D83D DE00. 63 pares = 126 unidades; a 127a seria o high surrogate do 64o par.
  std::wstring in;
  for (int i = 0; i < 100; ++i) { in.push_back(0xD83D); in.push_back(0xDE00); }
  std::wstring r = trayClampTip(in);
  CHECK_EQ(r.size(), (size_t)126);
  CHECK(r.back() == 0xDE00);
  // Com um caractere BMP antes, a 127a unidade cai exatamente no fim de um par: cabe.
  std::wstring in2 = L"a";
  for (int i = 0; i < 100; ++i) { in2.push_back(0xD83D); in2.push_back(0xDE00); }
  std::wstring r2 = trayClampTip(in2);
  CHECK_EQ(r2.size(), (size_t)127);
  CHECK(r2.back() == 0xDE00);
}

// ---------------- Win32 real ----------------

namespace {
bool iconExists(HWND hwnd, UINT uid) {
  NOTIFYICONIDENTIFIER id = {};
  id.cbSize = sizeof(id);
  id.hWnd = hwnd;
  id.uID = uid;
  RECT rc;
  return Shell_NotifyIconGetRect(&id, &rc) == S_OK;
}
}  // namespace

TEST(tray_win32, icon_lifecycle_and_routing) {
  // Seguro antes do init.
  trayShutdown();
  trayShowMenuAtCursor();
  traySetTooltip("antes do init");

  FakeActions fake;
  Settings s;
  if (!trayInit(&fake, &s)) {
    std::printf("  SKIP: sem shell/bandeja\n");
    return;
  }
  HWND hwnd = (HWND)trayWindowForTest();
  UINT uid = trayIconIdForTest();
  CHECK(hwnd != nullptr);
  if (!hwnd) { trayShutdown(); return; }
  CHECK(IsWindow(hwnd));
  CHECK_MSG(iconExists(hwnd, uid), "icone deveria existir apos trayInit");

  // WM_COMMAND -> dispatch
  SendMessageW(hwnd, WM_COMMAND, kTrayToggleSound, 0);
  SendMessageW(hwnd, WM_COMMAND, kTrayScale20, 0);
  SendMessageW(hwnd, WM_COMMAND, kTrayQuit, 0);
  SendMessageW(hwnd, WM_COMMAND, 424242, 0);  // desconhecido: no-op
  CHECK_EQ(fake.calls.size(), (size_t)3);
  if (fake.calls.size() == 3) {
    CHECK_EQ(fake.calls[0], "setSound:0");
    CHECK_EQ(fake.calls[1], "setScale:2");
    CHECK_EQ(fake.calls[2], "quit");
  }

  // A bandeja le a Settings viva por ponteiro: mudar o objeto vale sem nenhum sync.
  s.soundEnabled = false;
  fake.calls.clear();
  SendMessageW(hwnd, WM_COMMAND, kTrayToggleSound, 0);
  CHECK_EQ(fake.calls.size(), (size_t)1);
  if (!fake.calls.empty()) CHECK_EQ(fake.calls[0], "setSound:1");

  // clique esquerdo no icone
  fake.calls.clear();
  SendMessageW(hwnd, WM_APP + 1, uid, WM_LBUTTONUP);
  CHECK_EQ(fake.calls.size(), (size_t)1);
  if (!fake.calls.empty()) CHECK_EQ(fake.calls[0], "toggleVisible");

  // Explorer reiniciou
  SendMessageW(hwnd, RegisterWindowMessageW(L"TaskbarCreated"), 0, 0);
  CHECK_MSG(iconExists(hwnd, uid), "icone deveria existir apos TaskbarCreated");

  // tooltips: normal, longo, vazio, UTF-8 invalido, nulo
  traySetTooltip("TamaPoke - feliz");
  traySetTooltip(std::string(400, 'z').c_str());
  traySetTooltip("");
  traySetTooltip("\xff\xfe\xc3");
  traySetTooltip(nullptr);
  CHECK(iconExists(hwnd, uid));

  trayShutdown();
  CHECK_MSG(!iconExists(hwnd, uid), "icone deveria sumir apos trayShutdown");
  CHECK(!IsWindow(hwnd));

  // idempotente / seguro apos shutdown
  trayShutdown();
  trayShowMenuAtCursor();
  traySetTooltip("depois");
  CHECK(trayWindowForTest() == nullptr);

  // reinit funciona e novamente limpa
  if (trayInit(&fake, &s)) {
    HWND h2 = (HWND)trayWindowForTest();
    CHECK(iconExists(h2, trayIconIdForTest()));
    trayShutdown();
    CHECK(!iconExists(h2, uid));
  }
}

// Regressao: a bandeja nao pode guardar copia das settings (ficava desatualizada e o
// usuario nunca conseguia religar o som pelo menu).
TEST(tray_win32, settings_vivas_sem_sync) {
  FakeActions fake;
  Settings live;
  if (!trayInit(&fake, &live)) {
    std::printf("  SKIP: sem shell/bandeja\n");
    return;
  }
  HWND hwnd = (HWND)trayWindowForTest();
  CHECK(hwnd != nullptr);
  if (hwnd) {
    live.soundEnabled = false;
    live.scale = 2.0f;
    SendMessageW(hwnd, WM_COMMAND, kTrayToggleSound, 0);
    CHECK_EQ(fake.calls.size(), (size_t)1);
    if (!fake.calls.empty()) CHECK_EQ(fake.calls[0], "setSound:1");

    bool soundChecked = true;
    for (const TrayMenuItem& it : trayBuildMenu(live))
      if (it.id == kTrayToggleSound) soundChecked = it.checked;
    CHECK(!soundChecked);
  }
  trayShutdown();
}
