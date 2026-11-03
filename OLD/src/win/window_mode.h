// src/win/window_mode.h
#pragma once
// Modo de janela do pet: silhueta redonda por regiao eliptica (SetWindowRgn), caminho unico.
struct SDL_Window;

// Pura: o pixel de janela (windowX, windowY) cai dentro do circulo de 466 px na escala dada?
bool windowModeHit(int windowX, int windowY, float scale);
// Win32: regiao eliptica a partir do tamanho REAL da janela. HWND nulo/invalido = no-op.
// Reaplicar apos cada mudanca de tamanho.
void windowModeApplyRoundHwnd(void* hwnd);
// Obtem o HWND da janela SDL e delega. Janela nula = no-op.
void windowModeApplyRound(SDL_Window* window);
void windowModeSetAlwaysOnTop(SDL_Window* window, bool onTop);
// Arrasta a janela por um delta relativo.
void windowModeMove(SDL_Window* window, int dx, int dy);
