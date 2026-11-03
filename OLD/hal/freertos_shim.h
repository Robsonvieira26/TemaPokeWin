#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

// Shim de FreeRTOS sobre std::thread. Cobre so o que upstream/audio.cpp usa:
// xQueueCreate/Send/Receive, xTaskCreatePinnedToCore, vTaskDelay,
// pdMS_TO_TICKS e portMAX_DELAY (mais vQueueDelete, para os testes).
// No ESP32 isto chega via Arduino.h, por isso Arduino.h inclui este header.
// 1 tick = 1 ms.
typedef void* TaskHandle_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef uint32_t UBaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

struct QueueDef {
  std::mutex mu;
  std::condition_variable cv;
  std::deque<std::vector<uint8_t>> items;
  size_t itemSize = 0;
  size_t capacity = 0;
};
typedef QueueDef* QueueHandle_t;

// O estado da fila e alocado no heap e, se ninguem chamar vQueueDelete (o
// audio.cpp nunca chama), nunca e destruido: a tarefa destacada, bloqueada em
// xQueueReceive(portMAX_DELAY), nao toca em memoria liberada na saida do processo.
inline QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize) {
  auto* q = new QueueDef();
  q->itemSize = itemSize;
  q->capacity = len;
  return q;
}
inline void vQueueDelete(QueueHandle_t q) { delete q; }

inline BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t waitTicks) {
  if (!q || !item) return pdFAIL;
  std::unique_lock<std::mutex> lk(q->mu);
  if (q->items.size() >= q->capacity) {
    if (waitTicks == 0) return pdFAIL;
    auto temEspaco = [q] { return q->items.size() < q->capacity; };
    if (waitTicks == portMAX_DELAY) q->cv.wait(lk, temEspaco);
    else if (!q->cv.wait_for(lk, std::chrono::milliseconds(waitTicks), temEspaco)) return pdFAIL;
  }
  std::vector<uint8_t> buf(q->itemSize);
  std::memcpy(buf.data(), item, q->itemSize);
  q->items.push_back(std::move(buf));
  lk.unlock();
  q->cv.notify_all();
  return pdPASS;
}

inline BaseType_t xQueueReceive(QueueHandle_t q, void* out, TickType_t waitTicks) {
  if (!q || !out) return pdFAIL;
  std::unique_lock<std::mutex> lk(q->mu);
  if (q->items.empty()) {
    if (waitTicks == 0) return pdFAIL;
    auto temItem = [q] { return !q->items.empty(); };
    if (waitTicks == portMAX_DELAY) q->cv.wait(lk, temItem);
    else if (!q->cv.wait_for(lk, std::chrono::milliseconds(waitTicks), temItem)) return pdFAIL;
  }
  std::memcpy(out, q->items.front().data(), q->itemSize);
  q->items.pop_front();
  lk.unlock();
  q->cv.notify_all();
  return pdPASS;
}

// Tarefa = thread destacada (o upstream a cria uma vez e nunca a encerra).
inline BaseType_t xTaskCreatePinnedToCore(void (*fn)(void*), const char* /*nome*/,
                                          uint32_t /*stack*/, void* arg,
                                          UBaseType_t /*prio*/, TaskHandle_t* handle,
                                          int /*core*/) {
  std::thread(fn, arg).detach();
  if (handle) *handle = nullptr;
  return pdPASS;
}

inline void vTaskDelay(TickType_t ticks) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}
