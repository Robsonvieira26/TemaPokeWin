// tests/test_hal_rtc.cpp
#include "framework.h"
#include "hal_control.h"
#include "rtcbat.h"
#include <ctime>
#include <cstdlib>

namespace {
// hora local "como UTC" calculada de forma independente do HAL
long long paredeComoUtc(std::time_t utc) {
  std::tm tm{};
  localtime_s(&tm, &utc);
  tm.tm_isdst = 0;
  return (long long)_mkgmtime(&tm);
}
int horaLocal(std::time_t utc) {
  std::tm tm{};
  localtime_s(&tm, &utc);
  return tm.tm_hour;
}
}  // namespace

TEST(hal_rtc, epoch_segue_o_relogio_do_windows) {
  halSetRtcOffsetForTest(0);
  CHECK(rtcBegin());
  long long antes = paredeComoUtc(std::time(nullptr));
  long long lido = (long long)rtcEpoch();
  long long depois = paredeComoUtc(std::time(nullptr));
  ASSERT_TRUE(lido >= antes - 2 && lido <= depois + 2);
}

TEST(hal_rtc, rtcEpoch_da_a_hora_local) {
  halSetRtcOffsetForTest(0);
  CHECK(rtcBegin());
  int h0 = horaLocal(std::time(nullptr));
  int hl = (int)((rtcEpoch() / 3600) % 24);
  int h1 = horaLocal(std::time(nullptr));
  CHECK_MSG(hl == h0 || hl == h1, "hora de rtcEpoch() deve ser a hora local");
}

TEST(hal_rtc, deslocamento_utc_local_bate_com_o_windows) {
  std::time_t t = std::time(nullptr);
  CHECK_EQ(halLocalUtcOffsetSeconds((int64_t)t), paredeComoUtc(t) - (long long)t);
  CHECK_EQ(halLocalUtcOffsetSeconds(0) % 900, 0);   // fusos sao multiplos de 15 min
}

TEST(hal_rtc, offset_de_teste_avanca_o_epoch) {
  halSetRtcOffsetForTest(0);
  rtcBegin();
  long long base = (long long)rtcEpoch();
  halSetRtcOffsetForTest(7200);          // +2 h
  long long depois = (long long)rtcEpoch();
  halSetRtcOffsetForTest(0);
  ASSERT_TRUE(depois - base >= 7190 && depois - base <= 7210);
}

TEST(hal_rtc, rtcSetEpoch_ida_e_volta) {
  halSetRtcOffsetForTest(0);
  rtcBegin();
  uint32_t alvo = (uint32_t)paredeComoUtc(std::time(nullptr)) - 5u * 3600u;  // 5 h atras
  rtcSetEpoch(alvo);
  long long lido = (long long)rtcEpoch();
  ASSERT_TRUE(std::llabs(lido - (long long)alvo) <= 2);
  rtcSetEpoch((uint32_t)paredeComoUtc(std::time(nullptr)));  // restaura
}

TEST(hal_rtc, bateria_e_energia_sao_estaveis) {
  batBegin();
  ASSERT_EQ((int)batPercent(), -1);
  ASSERT_TRUE(usbPresent());
  ASSERT_TRUE(!batShouldShutdown());
}

TEST(hal_rtc, pwr_short_press_eh_consumido_uma_vez) {
  batBegin();
  pwrSetup();
  while (pwrShortPressed()) {}           // drena
  halPwrShortPress();
  ASSERT_TRUE(pwrShortPressed());
  ASSERT_TRUE(!pwrShortPressed());
}
