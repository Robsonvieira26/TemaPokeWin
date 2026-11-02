#pragma once
#include <cstdint>
#include "Wire.h"

struct RTC_DateTime {
  RTC_DateTime() = default;
  RTC_DateTime(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s)
      : year(y), month(mo), day(d), hour(h), minute(mi), second(s) {}
  uint16_t getYear() const { return year; }
  uint8_t getMonth() const { return month; }
  uint8_t getDay() const { return day; }
  uint8_t getHour() const { return hour; }
  uint8_t getMinute() const { return minute; }
  uint8_t getSecond() const { return second; }
  uint16_t year = 1970; uint8_t month = 1, day = 1, hour = 0, minute = 0, second = 0;
};

// Campos de data/hora como o rtcbat.cpp espera: ver hal/stubs.cpp.
class SensorPCF85063 {
 public:
  bool begin(TwoWire& w, int sda, int scl) { (void)w; (void)sda; (void)scl; return true; }
  RTC_DateTime getDateTime();
  void setDateTime(const RTC_DateTime& dt);
};
