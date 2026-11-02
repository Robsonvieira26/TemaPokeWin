// tests/test_hal_serial.cpp
#include "framework.h"
#include "Arduino.h"
#include "hal_control.h"
#include <filesystem>

// Fila de entrada e global: esvazia e aponta o log para um diretorio temporario
// (nenhum log.txt no cwd/repo).
static void serialSetup() {
  static bool once = false;
  if (!once) {
    once = true;
    auto dir = std::filesystem::temp_directory_path() / "tamapoke_serial_test";
    std::filesystem::create_directories(dir);
    static std::string s = dir.string();
    halSetDataDir(s.c_str());
  }
  while (Serial.read() != -1) {}
}

TEST(hal_serial, linha_injetada_chega_por_readStringUntil) {
  serialSetup();
  halSerialInject("STATS\n");
  ASSERT_TRUE(Serial.available() > 0);
  String line = Serial.readStringUntil('\n');
  line.trim();
  // O String da HAL nao tem operator==: compare pelo c_str() de um String LOCAL.
  CHECK_STREQ(line.c_str(), "STATS");
  ASSERT_EQ(Serial.available(), 0);
}

TEST(hal_serial, readBytes_le_o_que_foi_injetado) {
  serialSetup();
  halSerialInject("ABCD");
  char buf[4] = {0};
  size_t n = Serial.readBytes(buf, 4);
  ASSERT_EQ((int)n, 4);
  ASSERT_EQ(buf[0], 'A');
  ASSERT_EQ(buf[3], 'D');
}

TEST(hal_serial, readBytes_respeita_setTimeout_quando_faltam_bytes) {
  serialSetup();
  Serial.setTimeout(20);
  halSerialInject("xy");
  char buf[8] = {0};
  size_t n = Serial.readBytes(buf, 8);
  ASSERT_EQ((int)n, 2);
  Serial.setTimeout(1000);
}

TEST(hal_serial, read_devolve_menos_um_quando_vazio) {
  serialSetup();
  ASSERT_EQ(Serial.read(), -1);
}

TEST(hal_serial, config_e_saida_nao_quebram_sem_console) {
  serialSetup();
  Serial.setRxBufferSize(8192);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  Serial.printf("nivel=%d\n", 7);
  Serial.println("ok");
  Serial.println();
}
