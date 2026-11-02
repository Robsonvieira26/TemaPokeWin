#pragma once
#include <cstdint>
#include <functional>

// Ponto unico de acoplamento entre hal/, src/ e tests/.
// Nada em src/ inclui outro header de hal/. Nada em hal/ inclui SDL ou windows.h
// fora do seu proprio .cpp.

// --- caminhos ---
void halSetDataDir(const char* dir);   // onde vivem nvs.json, settings.json, log.txt
void halSetSdRoot(const char* dir);    // raiz do "cartao SD" (contem mons/)
const char* halDataDir();
const char* halSdRoot();

// --- relogio (gancho de teste) ---
uint32_t halMillis();
void halAdvanceForTest(uint32_t ms);   // soma um offset ao relogio; 0 = tempo real
void halSetRtcOffsetForTest(int64_t seconds);

// --- espera ociosa: o shell liga isso a SDL_WaitEventTimeout ---
void halSetIdleWaiter(std::function<void(uint32_t)> waiter);
void halIdleWait(uint32_t ms);

// --- serial / console ---
void halSerialInject(const char* line);

// --- toque ---
void halTouchDown(int x, int y);
void halTouchMove(int x, int y);
void halTouchUp();

// --- tela ---
const uint16_t* halFrame();            // 466*466 RGB565, nunca nullptr
bool halFrameDirty();                  // true se houve flush() desde o ultimo clear
void halClearFrameDirty();
uint8_t halBrightness();               // 0..255

// --- audio ---
uint64_t halAudioBytesWritten();

// --- energia ---
void halPwrShortPress();               // F2 / bandeja: o upstream le via pwrShortPressed()

// --- reinicio pedido pelo upstream (ESP.restart()) ---
bool halRestartRequested();
void halClearRestartRequest();

static constexpr int HAL_SCREEN_W = 466;
static constexpr int HAL_SCREEN_H = 466;
