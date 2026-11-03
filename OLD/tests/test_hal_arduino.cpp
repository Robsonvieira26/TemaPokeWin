// tests/test_hal_arduino.cpp
#include "framework.h"
#include "Arduino.h"
#include "hal_control.h"

static int gIsrCalls = 0;
static void testIsr() { ++gIsrCalls; }

TEST(hal_arduino, millis_eh_monotonico_e_avanca_com_o_gancho) {
  halAdvanceForTest(0);
  uint32_t a = millis();
  halAdvanceForTest(5000);
  uint32_t b = millis();
  ASSERT_TRUE(b >= a + 5000);
  halAdvanceForTest(0);
}

TEST(hal_arduino, delay_e_relogio_real_avancam) {
  halAdvanceForTest(0);
  uint32_t a = millis();
  delay(20);
  ASSERT_TRUE(millis() >= a + 15);
}

TEST(hal_arduino, random_fica_no_intervalo) {
  for (int i = 0; i < 1000; ++i) {
    long v = random(10);
    ASSERT_TRUE(v >= 0 && v < 10);
  }
  ASSERT_EQ(random(1), 0);
}

TEST(hal_arduino, string_cobre_o_que_o_upstream_usa) {
  String s("  PUT /mons/a.bin 12  ");
  s.trim();
  ASSERT_EQ((int)s.length(), 18);
  ASSERT_TRUE(s.startsWith("PUT "));
  ASSERT_TRUE(!s.startsWith("LS"));
  int sp = s.lastIndexOf(' ');
  ASSERT_EQ(sp, 15);
  String path = s.substring(4, sp);  // local: CHECK_STREQ guarda o ponteiro
  CHECK_STREQ(path.c_str(), "/mons/a.bin");
  ASSERT_EQ(s.substring(sp + 1).toInt(), 12);
  ASSERT_EQ(s.indexOf("/mons/"), 4);
  ASSERT_EQ(String("a/../b").indexOf(".."), 2);
  ASSERT_EQ(s.indexOf("zzz"), -1);
  ASSERT_EQ(s.lastIndexOf('#'), -1);
  String blank("  \r\n");
  blank.trim();
  ASSERT_EQ((int)blank.length(), 0);
  ASSERT_EQ((int)s.substring(100).length(), 0);
}

TEST(hal_arduino, esp_restart_pede_reinicio_sem_matar_o_processo) {
  halClearRestartRequest();
  ASSERT_TRUE(!halRestartRequested());
  ESP.restart();
  ASSERT_TRUE(halRestartRequested());
  halClearRestartRequest();
  ASSERT_TRUE(ESP.getFreeHeap() > 0);
  ASSERT_TRUE(ESP.getMinFreeHeap() > 0);
}

TEST(hal_arduino, interrupcao_e_disparada_pelo_pino) {
  gIsrCalls = 0;
  attachInterrupt(digitalPinToInterrupt(7), testIsr, FALLING);
  halFireInterrupt(7);
  halFireInterrupt(8);
  ASSERT_EQ(gIsrCalls, 1);
}
