// Bandeja do sistema: Shell_NotifyIconW + TrackPopupMenu.
//
// Janela: uma janela oculta de NIVEL SUPERIOR (WS_POPUP, WS_EX_TOOLWINDOW, nunca mostrada),
// nao message-only, porque so janelas de nivel superior recebem o broadcast "TaskbarCreated"
// (usado para recolocar o icone quando o Explorer reinicia). As mensagens sao bombeadas pelo
// loop de eventos do SDL na mesma thread; nao ha thread nem loop proprio.
// Mensagens: WM_APP+1 (callback do icone, semantica legada: lParam = mensagem do mouse),
// WM_COMMAND (mesmo caminho de dispatch que TPM_RETURNCMD), TaskbarCreated.
#include "win/tray.h"
#include "win/tray_menu.h"
#include "settings.h"
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

namespace {

const wchar_t* kClassName = L"TamaPokeTrayWindow";
const UINT kCallbackMsg = WM_APP + 1;
const UINT kIconId = 1;

HWND g_hwnd = nullptr;
ATOM g_class = 0;
TrayActions* g_actions = nullptr;
Settings g_settings;
UINT g_taskbarCreated = 0;
std::wstring g_tip = L"TamaPoke";
bool g_added = false;
bool g_menuOpen = false;

std::wstring utf8ToWide(const char* s) {
  if (!s || !*s) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
  if (n <= 1) return std::wstring();
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
  w.resize(static_cast<size_t>(n - 1));
  return w;
}

NOTIFYICONDATAW baseData() {
  NOTIFYICONDATAW d = {};
  d.cbSize = sizeof(d);
  d.hWnd = g_hwnd;
  d.uID = kIconId;
  return d;
}

HICON loadIcon() {
  HICON ic = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
  if (!ic) ic = LoadIconW(nullptr, MAKEINTRESOURCEW(32512) /* IDI_APPLICATION */);
  return ic;
}

void fillTip(NOTIFYICONDATAW& d) {
  std::wstring t = trayClampTip(g_tip);
  wcsncpy_s(d.szTip, ARRAYSIZE(d.szTip), t.c_str(), _TRUNCATE);
}

bool addIcon() {
  NOTIFYICONDATAW d = baseData();
  d.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  d.uCallbackMessage = kCallbackMsg;
  d.hIcon = loadIcon();
  fillTip(d);
  // Se um icone antigo com o mesmo id sobrou (ex.: reinicio do Explorer), o ADD falha; tenta limpar.
  if (!Shell_NotifyIconW(NIM_ADD, &d)) {
    NOTIFYICONDATAW del = baseData();
    Shell_NotifyIconW(NIM_DELETE, &del);
    if (!Shell_NotifyIconW(NIM_ADD, &d)) return false;
  }
  g_added = true;
  return true;
}

void removeIcon() {
  if (g_added && g_hwnd) {
    NOTIFYICONDATAW d = baseData();
    Shell_NotifyIconW(NIM_DELETE, &d);
  }
  g_added = false;
}

// Converte o modelo (lista plana) em HMENU. `i` avanca; `sub` e verdadeiro dentro de submenu.
HMENU buildMenu(const std::vector<TrayMenuItem>& model) {
  HMENU root = CreatePopupMenu();
  if (!root) return nullptr;
  HMENU cur = root;
  HMENU sub = nullptr;
  std::wstring subLabel;
  UINT firstRadio = 0, lastRadio = 0, checkedRadio = 0;
  for (const TrayMenuItem& it : model) {
    if (it.separator) { AppendMenuW(cur, MF_SEPARATOR, 0, nullptr); continue; }
    if (it.submenuStart) {
      sub = CreatePopupMenu();
      subLabel = utf8ToWide(it.label.c_str());
      cur = sub;
      firstRadio = lastRadio = checkedRadio = 0;
      continue;
    }
    std::wstring w = utf8ToWide(it.label.c_str());
    UINT flags = MF_STRING | (it.checked ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(cur, flags, static_cast<UINT_PTR>(it.id), w.c_str());
    if (it.radio) {
      if (!firstRadio) firstRadio = it.id;
      lastRadio = it.id;
      if (it.checked) checkedRadio = it.id;
    }
    if (it.submenuEnd && sub) {
      if (firstRadio && checkedRadio) CheckMenuRadioItem(sub, firstRadio, lastRadio, checkedRadio, MF_BYCOMMAND);
      AppendMenuW(root, MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(sub), subLabel.c_str());
      sub = nullptr;
      cur = root;
    }
  }
  if (sub) DestroyMenu(sub);  // submenu mal fechado: nao vaza
  return root;
}

void dispatch(int id) { trayDispatchCommand(id, g_settings, g_actions); }

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == kCallbackMsg) {
    switch (LOWORD(lp)) {
      case WM_LBUTTONUP:
        if (g_actions) g_actions->toggleVisible();
        break;
      case WM_RBUTTONUP:
      case WM_CONTEXTMENU:
        trayShowMenuAtCursor();
        break;
    }
    return 0;
  }
  if (msg == WM_COMMAND) {
    dispatch(static_cast<int>(LOWORD(wp)));
    return 0;
  }
  if (g_taskbarCreated && msg == g_taskbarCreated) {
    g_added = false;
    addIcon();
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool trayInit(TrayActions* actions, const Settings& settings) {
  if (g_hwnd) return true;
  g_actions = actions;
  g_settings = settings;
  g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = wndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kClassName;
  g_class = RegisterClassExW(&wc);
  if (!g_class) { g_actions = nullptr; return false; }

  g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"TamaPokeTray", WS_POPUP,
                           0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);
  if (!g_hwnd || !addIcon()) {
    if (g_hwnd) DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
    UnregisterClassW(kClassName, wc.hInstance);
    g_class = 0;
    g_actions = nullptr;
    return false;
  }
  return true;
}

void trayShutdown() {
  if (!g_hwnd && !g_class) return;
  removeIcon();
  if (g_hwnd) DestroyWindow(g_hwnd);
  g_hwnd = nullptr;
  if (g_class) UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
  g_class = 0;
  g_actions = nullptr;
  g_menuOpen = false;
}

void trayShowMenuAtCursor() {
  if (!g_hwnd || g_menuOpen) return;
  HMENU menu = buildMenu(trayBuildMenu(g_settings));
  if (!menu) return;
  POINT pt;
  GetCursorPos(&pt);
  g_menuOpen = true;
  // Sem isto o menu nao fecha ao clicar fora (comportamento documentado do TrackPopupMenu).
  SetForegroundWindow(g_hwnd);
  int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_BOTTOMALIGN,
                           pt.x, pt.y, 0, g_hwnd, nullptr);
  PostMessageW(g_hwnd, WM_NULL, 0, 0);
  DestroyMenu(menu);
  g_menuOpen = false;
  if (cmd > 0) dispatch(cmd);
}

void traySetTooltip(const char* text) {
  g_tip = utf8ToWide(text);
  if (!g_hwnd || !g_added) return;
  NOTIFYICONDATAW d = baseData();
  d.uFlags = NIF_TIP;
  fillTip(d);
  Shell_NotifyIconW(NIM_MODIFY, &d);
}

void traySyncChecks(const Settings& settings) { g_settings = settings; }

void* trayWindowForTest() { return g_hwnd; }
unsigned trayIconIdForTest() { return g_hwnd ? kIconId : 0; }
