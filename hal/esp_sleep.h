#pragma once
#include <cstdint>
typedef int esp_err_t;
#define ESP_OK 0

esp_err_t esp_light_sleep_start();
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us);
esp_err_t esp_sleep_enable_gpio_wakeup();
