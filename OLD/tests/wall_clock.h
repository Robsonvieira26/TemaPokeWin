// tests/wall_clock.h - fixa a hora local do RTC emulado em testes que dependem do dia/noite.
#pragma once
#include "hal_control.h"
#include <cstdint>
#include <ctime>

// segundos a somar ao relogio de parede de agora para que a hora local seja hh:mm:00 de hoje
inline int64_t offsetParaHoraLocal(int hh, int mm) {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
  localtime_s(&tm, &t);
  int agora = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
  return (int64_t)(hh * 3600 + mm * 60) - agora;
}

// Poe o RTC em hh:mm local; volta a 0 ao sair (mesmo se um CHECK/ASSERT abortar o teste).
struct RtcHoraLocalGuard {
  RtcHoraLocalGuard(int hh, int mm) { halSetRtcOffsetForTest(offsetParaHoraLocal(hh, mm)); }
  ~RtcHoraLocalGuard() { halSetRtcOffsetForTest(0); }
  RtcHoraLocalGuard(const RtcHoraLocalGuard&) = delete;
  RtcHoraLocalGuard& operator=(const RtcHoraLocalGuard&) = delete;
};
