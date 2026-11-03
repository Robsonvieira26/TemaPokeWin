#pragma once
#include <cstdint>
#include "Wire.h"

#define AXP2101_SLAVE_ADDRESS 0x34
#define XPOWERS_POWEROFF_4S 2
#define XPOWERS_AXP2101_ALL_IRQ 0xFFFFFFFFull
#define XPOWERS_AXP2101_PKEY_SHORT_IRQ (1ull << 3)

bool halConsumePwrShortPress();   // hal/stubs.cpp

// PMU simulado: sem bateria, sempre no USB. O botao PWR vem de halPwrShortPress().
class XPowersPMU {
 public:
  bool begin(TwoWire& w, uint8_t addr, int sda, int scl) { (void)w; (void)addr; (void)sda; (void)scl; return true; }
  void enableBattVoltageMeasure() {}
  void setSysPowerDownVoltage(uint16_t) {}
  void setBLDO1Voltage(uint16_t) {}
  void enableBLDO1() {}
  bool isBatteryConnect() { return false; }
  bool isCharging() { return false; }
  bool isVbusIn() { return true; }
  uint16_t getBattVoltage() { return 0; }
  int getBatteryPercent() { return -1; }
  void shutdown() {}
  void setPowerKeyPressOffTime(uint8_t) {}
  void disableIRQ(uint64_t) {}
  void enableIRQ(uint64_t) {}
  void clearIrqStatus() {}
  uint64_t getIrqStatus() { return 0; }
  bool isPekeyShortPressIrq() { return halConsumePwrShortPress(); }  // consome a flag
};
