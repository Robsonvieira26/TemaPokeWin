// tests/test_window_mode.cpp
#include "framework.h"
#include "win/window_mode.h"
#include <windows.h>
#include <string>

TEST(window_mode, hit_test_segue_o_circulo_na_escala) {
  // 1x: centro (233,233)
  CHECK(windowModeHit(233, 233, 1.0f));
  CHECK(!windowModeHit(0, 0, 1.0f));
  CHECK(!windowModeHit(465, 0, 1.0f));
  CHECK(!windowModeHit(0, 465, 1.0f));
  CHECK(!windowModeHit(465, 465, 1.0f));
  // 2x: centro em (466,466)
  CHECK(windowModeHit(466, 466, 2.0f));
  CHECK(!windowModeHit(0, 0, 2.0f));
  CHECK(!windowModeHit(931, 0, 2.0f));
  CHECK(!windowModeHit(0, 931, 2.0f));
  CHECK(!windowModeHit(931, 931, 2.0f));
  // 0.5x: centro real 116.5; 116 -> logico 232 e 117 -> 234 (truncamento inteiro), ambos dentro
  CHECK(windowModeHit(116, 116, 0.5f));
  CHECK(windowModeHit(117, 117, 0.5f));
  CHECK(!windowModeHit(0, 0, 0.5f));
  CHECK(!windowModeHit(232, 0, 0.5f));
  CHECK(!windowModeHit(0, 232, 0.5f));
  CHECK(!windowModeHit(232, 232, 0.5f));
}

TEST(window_mode, hit_test_na_borda_do_circulo) {
  // Esquerda (9 horas): x=0 -> dx=-233, borda inclusiva, dentro.
  CHECK(windowModeHit(0, 233, 1.0f));
  // Direita: x e limitado a 465 (dx=232), entao a direita nunca fica fora do circulo.
  CHECK(windowModeHit(465, 233, 1.0f));
  CHECK(windowModeHit(1000, 233, 1.0f));
  // Logo fora: 1 px alem do raio na diagonal e na vertical.
  CHECK(windowModeHit(233 + 164, 233 + 164, 1.0f));   // 2*164^2 = 53792 <= 54289
  CHECK(!windowModeHit(233 + 165, 233 + 165, 1.0f));  // 2*165^2 = 54450 > 54289
  CHECK(windowModeHit(233, 0, 1.0f));                 // 12 horas, dy=-233 inclusivo
  CHECK(!windowModeHit(1, 100, 1.0f));                // 232^2+133^2 > 233^2
}

namespace {
struct HiddenWindow {
  ATOM atom = 0;
  HWND hwnd = nullptr;
  HINSTANCE inst = GetModuleHandleW(nullptr);
  DWORD err = 0;
  bool create(int w, int h) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = inst;
    wc.lpszClassName = L"TamaPokeWindowModeTest";
    atom = RegisterClassW(&wc);
    if (!atom) { err = GetLastError(); return false; }
    hwnd = CreateWindowExW(0, L"TamaPokeWindowModeTest", L"t", WS_POPUP, 0, 0, w, h,
                           nullptr, nullptr, inst, nullptr);
    if (!hwnd) { err = GetLastError(); return false; }
    return true;
  }
  ~HiddenWindow() {
    if (hwnd) DestroyWindow(hwnd);
    if (atom) UnregisterClassW(L"TamaPokeWindowModeTest", inst);
  }
};

struct RgnGuard {
  HRGN r;
  RgnGuard() : r(CreateRectRgn(0, 0, 0, 0)) {}
  ~RgnGuard() { if (r) DeleteObject(r); }
};

void checkRound(HWND hwnd, int w, int h) {
  RgnGuard g;
  CHECK(g.r != nullptr);
  CHECK(GetWindowRgn(hwnd, g.r) != ERROR);
  CHECK(PtInRegion(g.r, w / 2, h / 2));
  CHECK(!PtInRegion(g.r, 0, 0));
  CHECK(!PtInRegion(g.r, w - 1, 0));
  CHECK(!PtInRegion(g.r, 0, h - 1));
  CHECK(!PtInRegion(g.r, w - 1, h - 1));
  CHECK(PtInRegion(g.r, 0, h / 2));
  CHECK(PtInRegion(g.r, w - 1, h / 2));
  CHECK(PtInRegion(g.r, w / 2, 0));
  CHECK(PtInRegion(g.r, w / 2, h - 1));
}
}  // namespace

TEST(window_mode, regiao_eliptica_segue_a_janela_e_o_resize) {
  HiddenWindow win;
  if (!win.create(466, 466)) {
    CHECK_MSG(false, std::string("CreateWindowExW/RegisterClassW falhou, GetLastError=") + std::to_string(win.err));
    return;
  }
  windowModeApplyRoundHwnd(win.hwnd);
  checkRound(win.hwnd, 466, 466);

  CHECK(SetWindowPos(win.hwnd, nullptr, 0, 0, 233, 233, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
  windowModeApplyRoundHwnd(win.hwnd);
  checkRound(win.hwnd, 233, 233);
}

TEST(window_mode, nulos_sao_no_op) {
  windowModeApplyRoundHwnd(nullptr);
  windowModeApplyRoundHwnd((void*)(UINT_PTR)0x1234);  // HWND invalido
  windowModeApplyRound(nullptr);
  windowModeSetAlwaysOnTop(nullptr, true);
  windowModeMove(nullptr, 1, 1);
  CHECK(true);
}
