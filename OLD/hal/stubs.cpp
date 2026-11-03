#include "Wire.h"
#include "esp_sleep.h"
#include "SensorPCF85063.hpp"
#include "hal_control.h"
#include <atomic>
#include <ctime>

TwoWire Wire;

int64_t halLocalUtcOffsetSeconds(int64_t utc) {
  std::time_t t = (std::time_t)utc;
  std::tm tm{};
  if (localtime_s(&tm, &t) != 0) return 0;
  tm.tm_isdst = 0;   // _mkgmtime ignora o fuso: so reinterpreta os campos como UTC
  return (int64_t)_mkgmtime(&tm) - utc;
}

namespace {
std::atomic<bool> gPwrShort{false};
int64_t gUserOffset = 0;   // de setDateTime (rtcSetEpoch)

// Base do RTC emulado: "hora de parede como se fosse UTC" = UTC real + deslocamento local.
// E o que um RTC ajustado a mao pelo usuario devolve no aparelho (o firmware faz
// epoch % 86400 = hora do dia, ver sceneHour()); usar o UTC puro punha dia/noite em UTC.
// Os offsets (teste e do usuario) ficam por cima desta base. Como a conta usa so DIFERENCAS
// de epoch, o progresso offline nao muda, exceto ao cruzar horario de verao / troca de fuso
// (aparece como +-1 h offline, igual a um RTC real acertado a mao).
int64_t baseParede() {
  int64_t utc = (int64_t)std::time(nullptr);
  return utc + halLocalUtcOffsetSeconds(utc);
}

int64_t nowEpoch() { return baseParede() + halRtcOffsetSeconds() + gUserOffset; }
}  // namespace

void halPwrShortPress() { gPwrShort.store(true); }
bool halConsumePwrShortPress() { return gPwrShort.exchange(false); }

// rtcbat.cpp: rtcEpoch() = mktime(campos) (hora LOCAL, isdst=0) e
// rtcSetEpoch(e) grava gmtime(e) (UTC). Aqui "e" e a hora de parede como UTC (ver baseParede).
//  - getDateTime devolve localtime(W), a inversa exata do mktime(isdst=0): assim
//    rtcEpoch() == W (e (W/3600)%24 e a hora local) em qualquer fuso;
//  - com horario de verao ativo em W, mktime(isdst=0) leria os campos como hora padrao
//    (1 h adiante), entao os campos saem de localtime(W - 3600);
//  - setDateTime recebe campos UTC (gmtime de e) e os converte de volta com _mkgmtime.
RTC_DateTime SensorPCF85063::getDateTime() {
  std::time_t t = (std::time_t)nowEpoch();
  std::tm tm{};
  localtime_s(&tm, &t);
  if (tm.tm_isdst > 0) {   // mktime com isdst=0 leria estes campos como hora padrao
    t -= 3600;
    localtime_s(&tm, &t);
  }
  return RTC_DateTime((uint16_t)(tm.tm_year + 1900), (uint8_t)(tm.tm_mon + 1), (uint8_t)tm.tm_mday,
                      (uint8_t)tm.tm_hour, (uint8_t)tm.tm_min, (uint8_t)tm.tm_sec);
}

void SensorPCF85063::setDateTime(const RTC_DateTime& dt) {
  std::tm tm{};
  tm.tm_year = dt.getYear() - 1900; tm.tm_mon = dt.getMonth() - 1; tm.tm_mday = dt.getDay();
  tm.tm_hour = dt.getHour(); tm.tm_min = dt.getMinute(); tm.tm_sec = dt.getSecond();
  int64_t alvo = (int64_t)_mkgmtime(&tm);
  gUserOffset = alvo - (baseParede() + halRtcOffsetSeconds());
}

esp_err_t esp_light_sleep_start() { halIdleWait(50); return ESP_OK; }
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t) { return ESP_OK; }
esp_err_t esp_sleep_enable_gpio_wakeup() { return ESP_OK; }
