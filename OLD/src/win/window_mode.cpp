// src/win/window_mode.cpp
#include "win/window_mode.h"
#include "present.h"
#include <SDL3/SDL.h>
#include <windows.h>

bool windowModeHit(int windowX, int windowY, float scale) {
  return presentInsideCircle(presentToLogical(windowX, scale), presentToLogical(windowY, scale));
}

void windowModeApplyRoundHwnd(void* hwndPtr) {
  HWND hwnd = (HWND)hwndPtr;
  if (!hwnd || !IsWindow(hwnd)) return;
  RECT r;
  if (!GetWindowRect(hwnd, &r)) return;
  int w = r.right - r.left, h = r.bottom - r.top;
  if (w <= 0 || h <= 0) return;
  // Elipse GDI exclui a borda direita/inferior: +1 para o pixel w-1 / h-1 do meio ficar dentro.
  HRGN rgn = CreateEllipticRgn(0, 0, w + 1, h + 1);
  if (!rgn) return;
  if (!SetWindowRgn(hwnd, rgn, TRUE)) DeleteObject(rgn);  // sucesso: o sistema e dono
}

void windowModeApplyRound(SDL_Window* window) {
  if (!window) return;
  void* hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                      SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
  windowModeApplyRoundHwnd(hwnd);
}

void windowModeSetAlwaysOnTop(SDL_Window* window, bool onTop) {
  if (!window) return;
  SDL_SetWindowAlwaysOnTop(window, onTop);
}

void windowModeMove(SDL_Window* window, int dx, int dy) {
  if (!window) return;
  int x = 0, y = 0;
  if (!SDL_GetWindowPosition(window, &x, &y)) return;
  SDL_SetWindowPosition(window, x + dx, y + dy);
}
