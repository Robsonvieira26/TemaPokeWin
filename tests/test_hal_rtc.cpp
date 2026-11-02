// tests/test_hal_rtc.cpp
#include "framework.h"
#include "hal_control.h"
#include "rtcbat.h"
#include <ctime>
#include <cstdlib>

TEST(hal_rtc, epoch_segue_o_relogio_do_windows) {
  halSetRtcOffsetForTest(0);
  CHECK(rtcBegin());
  long long agora = (long long)std::time(nullptr);
  long long lido = (long long)rtcEpoch();
  ASSERT_TRUE(std::llabs(lido - agora) <= 2);
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
  uint32_t alvo = (uint32_t)std::time(nullptr) - 5u * 3600u;  // 5 h atras
  rtcSetEpoch(alvo);
  long long lido = (long long)rtcEpoch();
  ASSERT_TRUE(std::llabs(lido - (long long)alvo) <= 2);
  rtcSetEpoch((uint32_t)std::time(nullptr));  // restaura
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
