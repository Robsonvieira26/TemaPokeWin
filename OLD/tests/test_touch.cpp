// tests/test_touch.cpp
#include "framework.h"
#include "Arduino.h"
#include "TouchDrvCSTXXX.hpp"
#include "hal_control.h"
#include "../upstream/pin_config.h"

static int gIsrCalls = 0;
static void IRAM_ATTR isrDeTeste() { ++gIsrCalls; }

// Replica o que o firmware faz em setup(): setPins + begin + ISR em FALLING.
static void preparar(TouchDrvCST92xx& touch) {
  halTouchUp();
  touch.setPins(TP_RESET, TP_INT);
  touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), isrDeTeste, FALLING);
  gIsrCalls = 0;
}

TEST(touch, down_dispara_a_isr_e_getPoint_devolve_o_ponto) {
  TouchDrvCST92xx touch;
  preparar(touch);
  halTouchDown(100, 200);
  ASSERT_EQ(gIsrCalls, 1);
  int16_t x = 0, y = 0;
  ASSERT_EQ((int)touch.getPoint(&x, &y, 1), 1);
  ASSERT_EQ((int)x, 100);
  ASSERT_EQ((int)y, 200);
  halTouchUp();
}

TEST(touch, move_atualiza_o_ponto_e_dispara_a_isr) {
  TouchDrvCST92xx touch;
  preparar(touch);
  halTouchDown(10, 10);
  halTouchMove(50, 60);
  ASSERT_EQ(gIsrCalls, 2);
  int16_t x = 0, y = 0;
  ASSERT_EQ((int)touch.getPoint(&x, &y, 1), 1);
  ASSERT_EQ((int)x, 50);
  ASSERT_EQ((int)y, 60);
  halTouchUp();
}

TEST(touch, move_sem_dedo_nao_dispara_a_isr) {
  TouchDrvCST92xx touch;
  preparar(touch);
  halTouchMove(5, 5);
  ASSERT_EQ(gIsrCalls, 0);
  int16_t x = 0, y = 0;
  ASSERT_EQ((int)touch.getPoint(&x, &y, 1), 0);
}

TEST(touch, up_zera_getPoint_e_nao_dispara_a_isr) {
  TouchDrvCST92xx touch;
  preparar(touch);
  halTouchDown(10, 10);
  gIsrCalls = 0;
  halTouchUp();
  ASSERT_EQ(gIsrCalls, 0);
  int16_t x = 0, y = 0;
  ASSERT_EQ((int)touch.getPoint(&x, &y, 1), 0);
}

TEST(touch, digitalRead_do_pino_de_irq_e_ativo_em_baixo) {
  TouchDrvCST92xx touch;
  preparar(touch);
  ASSERT_EQ(digitalRead(TP_INT), HIGH);
  halTouchDown(1, 1);
  ASSERT_EQ(digitalRead(TP_INT), LOW);
  halTouchUp();
  ASSERT_EQ(digitalRead(TP_INT), HIGH);
}

TEST(touch, digitalRead_de_outros_pinos_e_sempre_high) {
  TouchDrvCST92xx touch;
  preparar(touch);
  halTouchDown(1, 1);
  ASSERT_EQ(digitalRead(TP_RESET), HIGH);
  ASSERT_EQ(digitalRead(0), HIGH);
  halTouchUp();
  ASSERT_EQ(digitalRead(TP_RESET), HIGH);
}
