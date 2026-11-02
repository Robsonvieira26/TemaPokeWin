#include "Wire.h"
#include "esp_sleep.h"
#include "SensorPCF85063.hpp"
#include "hal_control.h"
#include <atomic>
#include <ctime>

TwoWire Wire;

namespace {
std::atomic<bool> gPwrShort{false};
int64_t gUserOffset = 0;   // de setDateTime (rtcSetEpoch)

int64_t nowEpoch() {
  return (int64_t)std::time(nullptr) + halRtcOffsetSeconds() + gUserOffset;
}
}  // namespace

void halPwrShortPress() { gPwrShort.store(true); }
bool halConsumePwrShortPress() { return gPwrShort.exchange(false); }

// rtcbat.cpp: rtcEpoch() = mktime(campos) (hora LOCAL, isdst=0) e
// rtcSetEpoch() grava gmtime(e) (UTC). Para rtcEpoch() ~ epoch real em qualquer fuso:
//  - getDateTime devolve localtime(epoch), a inversa exata do mktime;
//  - setDateTime recebe campos UTC e os converte de volta com _mkgmtime.
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
  gUserOffset = alvo - ((int64_t)std::time(nullptr) + halRtcOffsetSeconds());
}

esp_err_t esp_light_sleep_start() { halIdleWait(50); return ESP_OK; }
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t) { return ESP_OK; }
esp_err_t esp_sleep_enable_gpio_wakeup() { return ESP_OK; }
