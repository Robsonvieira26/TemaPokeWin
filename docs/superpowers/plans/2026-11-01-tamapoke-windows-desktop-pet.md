# TamaPoke Windows Desktop Pet — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Use superpowers:test-driven-development on every code step.

**Goal:** Rodar o firmware do TamaPoke como app nativo de Windows — janela redonda sem borda na área de trabalho, bandeja, autostart — sem alterar uma linha do upstream.

**Architecture:** O upstream entra como submódulo git em `upstream/` e seus `.cpp`/`.ino` são compilados como estão. `hal/` fornece versões de desktop de todos os headers de hardware que eles incluem. `src/game_host` é dono do ciclo `setup()`/`loop()` e do framebuffer, exposto pela interface estreita `IGameHost`; `src/app_shell` (+ `src/win/`) cuida de janela, bandeja, autostart e input, e nunca toca em `hal/` nem no upstream. Isso deixa o jogo testável headless.

**Tech Stack:** C++17, CMake ≥ 3.24, MSVC 2022, SDL3 via `FetchContent`, nlohmann/json, Win32 (Shell_NotifyIcon, registro), micro-framework de testes do upstream, GitHub Actions `windows-latest`, Inno Setup.

**Spec:** `docs/superpowers/specs/2026-11-01-tamapoke-windows-port-design.md` — leia antes da Tarefa 1.

---

## Estrutura de arquivos

Criados por este plano, com a responsabilidade de cada um:

| Arquivo | Responsabilidade |
|---|---|
| `CMakeLists.txt` | alvos `tamapoke`, `upstream_tests`, `tamapoke_tests`; FetchContent do SDL3; cópia dos sprites |
| `upstream/` | submódulo, somente leitura |
| `hal/hal_control.h` | **único** header de ganchos de teste/integração da HAL |
| `hal/Arduino.h`, `hal/arduino.cpp`, `hal/WString.h` | relógio, random, GPIO, `ESP`, `String` |
| `hal/HardwareSerial.h`, `hal/serial.cpp` | console de debug injetável |
| `hal/Preferences.h`, `hal/preferences.cpp` | save em JSON com tipagem NVS |
| `hal/FS.h`, `hal/SD_MMC.h`, `hal/sd.cpp` | sistema de arquivos com raiz configurável |
| `hal/Wire.h`, `hal/esp_sleep.h`, `hal/driver/gpio.h`, `hal/SensorPCF85063.hpp`, `hal/XPowersLib.h`, `hal/stubs.cpp` | I2C no-op, RTC, PMU, sleep |
| `hal/Arduino_GFX_Library.h`, `hal/panel.cpp` (ou `hal/canvas.cpp`) | framebuffer 466×466 RGB565 e brilho |
| `hal/TouchDrvCSTXXX.hpp`, `hal/touch.cpp` | toque vindo do mouse, dispara a ISR |
| `hal/ESP_I2S.h`, `hal/i2s.cpp`, `hal/freertos_shim.h` | áudio e tasks/filas |
| `hal/U8g2lib.h`, `hal/u8g2_fonts.c` | fontes CJK |
| `src/game_host.h` | interface `IGameHost` (só a interface, nada de implementação) |
| `src/game_host.cpp` | `GameHost`: implementa `IGameHost` sobre a HAL + upstream |
| `src/app_shell.h`, `src/app_shell.cpp` | máquina de estados `Visible`/`Hidden`/`ScreenOff`, eventos, input → touch lógico |
| `src/present.h`, `src/present.cpp` | textura RGB565, escala, brilho, máscara circular |
| `src/settings.h`, `src/settings.cpp` | `settings.json`: posição, escala, som, always-on-top, autostart |
| `src/win/window_mode.cpp` | transparência vs. `SetWindowRgn`, hit-test, arrastar |
| `src/win/tray.cpp` | ícone e menu da bandeja |
| `src/win/autostart.cpp` | chave `Run` do registro |
| `src/win/single_instance.cpp` | mutex nomeado + "mostrar" para a instância viva |
| `src/main.cpp` | monta shell + host, roda, sai |
| `tools/gen_protos.py` | gera protótipos do `.ino` |
| `tests/` | testes de HAL (nível 2) e de host headless (nível 3) |

Nenhum arquivo de `src/` passa de ~300 linhas. Assunto novo vira arquivo novo em `src/win/`.

---

## Fase 0: esqueleto

### Task 1: Repo, submódulo e janela SDL vazia

**Files:**
- Create: `CMakeLists.txt`, `.gitignore`, `.gitmodules` (via git), `src/main.cpp`, `README.md`

- [ ] **Step 1: Adicionar o submódulo e fixar o commit**

```bash
cd /c/Users/robso/Code/TemaPokeWin
git submodule add https://github.com/socquique/TamaPoke upstream
cd upstream && git rev-parse HEAD && cd ..
```

Anote o hash impresso no `README.md` numa linha `Upstream fixado em: <hash>`.

- [ ] **Step 2: Escrever `.gitignore`**

```gitignore
build/
out/
.vs/
*.user
settings.json
```

- [ ] **Step 3: Escrever `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.24)
project(tamapoke CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

if (MSVC)
  add_compile_options(/utf-8 /Zc:__cplusplus /permissive- /W3)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

include(FetchContent)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.2.10
  GIT_SHALLOW TRUE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

add_executable(tamapoke WIN32 src/main.cpp)
target_link_libraries(tamapoke PRIVATE SDL3::SDL3)
```

- [ ] **Step 4: Escrever `src/main.cpp`**

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int, char**) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TamaPoke", SDL_GetError(), nullptr);
    return 1;
  }
  SDL_Window* win = SDL_CreateWindow("TamaPoke", 466, 466, 0);
  if (!win) { SDL_Quit(); return 1; }

  bool running = true;
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_EVENT_QUIT) running = false;
    }
    SDL_Delay(16);
  }
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
```

- [ ] **Step 5: Configurar e compilar**

Run:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```
Expected: build sem erro; `build/Debug/tamapoke.exe` existe.

- [ ] **Step 6: Rodar e conferir**

Run: `./build/Debug/tamapoke.exe`
Expected: janela 466×466 que fecha no X.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt .gitignore .gitmodules upstream src/main.cpp README.md
git commit -m "chore: esqueleto CMake + SDL3 + submodulo upstream"
```

### Task 2: Suíte de testes do upstream rodando com MSVC

Isto prova que a lógica do upstream compila com MSVC antes de mexer em qualquer coisa. É o canário de todas as fases seguintes.

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Levantar os arquivos da suíte do upstream**

Run:
```bash
ls upstream/test/ && ls upstream/test/shim/
```
Expected: lista de `.cpp` de teste, `framework.h` e os headers de shim. Anote os nomes reais — os passos abaixo assumem `upstream/test/*.cpp` e `upstream/test/shim/shim.cpp`. Se a estrutura diferir, ajuste os globs mantendo a intenção: compilar os testes do upstream com o shim do upstream.

- [ ] **Step 2: Adicionar o alvo `upstream_tests` ao `CMakeLists.txt`**

```cmake
file(GLOB UPSTREAM_TEST_SRC CONFIGURE_DEPENDS upstream/test/*.cpp)
add_executable(upstream_tests
  ${UPSTREAM_TEST_SRC}
  upstream/test/shim/shim.cpp
  upstream/pet.cpp
  upstream/i18n.cpp)
target_include_directories(upstream_tests PRIVATE upstream/test/shim upstream)
add_test(NAME upstream COMMAND upstream_tests)
```

- [ ] **Step 3: Compilar e rodar**

Run:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target upstream_tests
ctest --test-dir build -C Debug --output-on-failure
```
Expected: teste `upstream` passa. Testes marcados `TEST_KNOWN_ISSUE` são informados pelo próprio framework e não quebram.

- [ ] **Step 4: Se o MSVC reclamar, corrigir só com flags/defines**

Nada de patch no upstream (invariante do spec). Sintomas e remédios, todos no `CMakeLists.txt`:

| Erro | Remédio |
|---|---|
| `min`/`max` de `windows.h` | `NOMINMAX` (já está) |
| `__cplusplus` errado | `/Zc:__cplusplus` (já está) |
| acentos em string literal | `/utf-8` (já está) |
| VLA (`int a[n]`) | `target_compile_options(upstream_tests PRIVATE /clang:-Wno-vla)` com clang-cl, ou `add_compile_definitions` do que o upstream usar pra tamanho fixo |
| `__attribute__((x))` | `target_compile_definitions(upstream_tests PRIVATE "__attribute__(x)=")` |

Se nenhum remédio de flag resolver, pare e reporte — não edite `upstream/`.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt
git commit -m "test: suite do upstream rodando com MSVC"
```

### Task 3: CI

**Files:**
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Escrever o workflow**

```yaml
name: ci
on: [push, pull_request]
jobs:
  windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Configure
        run: cmake -B build -G "Visual Studio 17 2022" -A x64
      - name: Build
        run: cmake --build build --config Release
      - name: Test
        run: ctest --test-dir build -C Release --output-on-failure
```

- [ ] **Step 2: Commit e push**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: build e ctest no windows-latest"
git push
```
Expected: job verde no GitHub. Se o repo ainda não tem remote, pule o push e anote no README que o CI entra junto com o remote.

---

## Fase 1: HAL base (sem tela)

Cada tarefa desta fase é TDD com o micro-framework do upstream (`upstream/test/framework.h`, macro `TEST(suite, name)`). Os testes novos vão em `tests/` e entram no alvo `tamapoke_tests`, que compila contra `hal/` — **nunca** contra `upstream/test/shim`.

### Task 4: `hal_control.h` e o alvo `tamapoke_tests`

Este é o ponto único de acoplamento entre HAL, host e testes (invariante do spec §3.2). Ele nasce antes de qualquer implementação.

**Files:**
- Create: `hal/hal_control.h`, `tests/framework.h`, `tests/test_main.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Escrever `hal/hal_control.h`**

```cpp
#pragma once
#include <cstdint>
#include <functional>

// Ponto unico de acoplamento entre hal/, src/ e tests/.
// Nada em src/ inclui outro header de hal/. Nada em hal/ inclui SDL ou windows.h
// fora do seu proprio .cpp.

// --- caminhos ---
void halSetDataDir(const char* dir);   // onde vivem nvs.json, settings.json, log.txt
void halSetSdRoot(const char* dir);    // raiz do "cartao SD" (contem mons/)
const char* halDataDir();
const char* halSdRoot();

// --- relogio (gancho de teste) ---
uint32_t halMillis();
void halAdvanceForTest(uint32_t ms);   // soma um offset ao relogio; 0 = tempo real
void halSetRtcOffsetForTest(int64_t seconds);

// --- espera ociosa: o shell liga isso a SDL_WaitEventTimeout ---
void halSetIdleWaiter(std::function<void(uint32_t)> waiter);
void halIdleWait(uint32_t ms);

// --- serial / console ---
void halSerialInject(const char* line);

// --- toque ---
void halTouchDown(int x, int y);
void halTouchMove(int x, int y);
void halTouchUp();

// --- tela ---
const uint16_t* halFrame();            // 466*466 RGB565, nunca nullptr
bool halFrameDirty();                  // true se houve flush() desde o ultimo clear
void halClearFrameDirty();
uint8_t halBrightness();               // 0..255

// --- audio ---
uint64_t halAudioBytesWritten();

// --- energia ---
void halPwrShortPress();               // F2 / bandeja: o upstream le via pwrShortPressed()

// --- reinicio pedido pelo upstream (ESP.restart()) ---
bool halRestartRequested();
void halClearRestartRequest();

static constexpr int HAL_SCREEN_W = 466;
static constexpr int HAL_SCREEN_H = 466;
```

- [ ] **Step 2: Escrever `tests/framework.h` (adaptador)**

O framework do upstream expõe `TEST`, `TEST_KNOWN_ISSUE` e `CHECK`, `CHECK_EQ`, `CHECK_STREQ`, `CHECK_RANGE`, `CHECK_MSG` — **não** `ASSERT_*`. Os testes deste plano usam `ASSERT_TRUE`/`ASSERT_EQ`; este adaptador os mapeia, sem copiar o framework.

```cpp
// tests/framework.h
#pragma once
#include "../upstream/test/framework.h"

#define ASSERT_TRUE(cond) CHECK(cond)
#define ASSERT_EQ(a, b)   CHECK_EQ(a, b)
```

Os testes fazem `#include "framework.h"`, e um include com aspas procura primeiro na pasta do arquivo que inclui, então resolvem para este adaptador. `upstream/test` **não** entra no include path do alvo (veja o Step 5), para o shim do upstream nunca ficar ao alcance.

- [ ] **Step 3: Escrever `tests/test_main.cpp` (runner próprio)**

O `upstream/test/main.cpp` **não serve** aqui: ele inclui o shim (`mockSerialSilence`, `mockSetMillis`, `mockClearForcedRandom`). Este runner replica a lógica dele sem o shim.

```cpp
// tests/test_main.cpp
#include "framework.h"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

std::vector<TestCase>& testRegistry() {
  static std::vector<TestCase> reg;
  return reg;
}

int gAssertCount = 0;
static std::vector<std::string> gFailures;

void testFail(const char* file, int line, const std::string& msg) {
  const char* a = std::strrchr(file, '/');
  const char* b = std::strrchr(file, '\\');
  const char* base = a > b ? a : b;
  std::ostringstream os;
  os << (base ? base + 1 : file) << ":" << line << ": " << msg;
  gFailures.push_back(os.str());
}

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int passed = 0, failed = 0, skipped = 0, xfail = 0, xpass = 0;
  std::string lastSuite;
  std::vector<std::string> failedNames;

  for (TestCase& tc : testRegistry()) {
    std::string full = tc.suite + "." + tc.name;
    if (filter && full.find(filter) == std::string::npos) { ++skipped; continue; }
    if (tc.suite != lastSuite) {
      std::printf("\n%s\n", tc.suite.c_str());
      lastSuite = tc.suite;
    }
    gFailures.clear();
    tc.fn();
    if (tc.knownIssue) {
      if (gFailures.empty()) {
        std::printf("  XPASS %s (remova TEST_KNOWN_ISSUE)\n", tc.name.c_str());
        ++xpass;
      } else {
        std::printf("  KNOWN %s -- %s\n", tc.name.c_str(), tc.note.c_str());
        ++xfail;
      }
      continue;
    }
    if (gFailures.empty()) {
      std::printf("  ok   %s\n", tc.name.c_str());
      ++passed;
    } else {
      std::printf("  FAIL %s\n", tc.name.c_str());
      for (const auto& f : gFailures) std::printf("        %s\n", f.c_str());
      ++failed;
      failedNames.push_back(full);
    }
  }

  std::printf("\n%d tests, %d asserts, %d passed, %d failed",
              passed + failed + xfail + xpass, gAssertCount, passed, failed);
  if (xfail) std::printf(", %d bugs conhecidos", xfail);
  if (skipped) std::printf(", %d filtrados", skipped);
  std::printf("\n");
  for (const auto& n : failedNames) std::printf("  - %s\n", n.c_str());
  return failed ? 1 : 0;
}
```

Diferença deliberada em relação ao runner do upstream: **não há reset de estado entre testes** (o do upstream zera o relógio e o random do shim). Aqui cada teste que depende de relógio, offset de RTC ou diretório de dados deve zerar o próprio estado no início — os testes deste plano já fazem isso (`halAdvanceForTest(0)`, `halSetRtcOffsetForTest(0)`, `halSetDataDir(...)`).

- [ ] **Step 4: Adicionar o alvo `tamapoke_tests` ao `CMakeLists.txt`**

```cmake
file(GLOB HAL_SRC CONFIGURE_DEPENDS hal/*.cpp)
file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
add_executable(tamapoke_tests ${TEST_SRC} ${HAL_SRC})
target_include_directories(tamapoke_tests PRIVATE hal upstream src)
target_link_libraries(tamapoke_tests PRIVATE SDL3::SDL3)
add_test(NAME tamapoke COMMAND tamapoke_tests)

# SDL3 e DLL compartilhada: sem isto o exe de teste nao abre ("Erro do sistema").
add_custom_command(TARGET tamapoke_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:tamapoke_tests>)
```

`upstream/test` **não** entra no include path: o `framework.h` do upstream chega pelo adaptador `tests/framework.h`, e o shim nunca fica ao alcance.

- [ ] **Step 5: Compilar e rodar**

Run:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target tamapoke_tests
ctest --test-dir build -C Debug -R tamapoke --output-on-failure
```
Expected: roda com 0 testes e sai com código 0.

- [ ] **Step 6: Commit**

```bash
git add hal/hal_control.h tests/framework.h tests/test_main.cpp CMakeLists.txt
git commit -m "feat(hal): hal_control.h e alvo de testes contra a HAL"
```

### Task 5: `Arduino.h`, `String` e relógio

**Files:**
- Create: `hal/Arduino.h`, `hal/WString.h`, `hal/arduino.cpp`
- Test: `tests/test_hal_arduino.cpp`

- [ ] **Step 1: Levantar exatamente o que o upstream usa de `String`**

Run:
```bash
grep -ohE '\.(trim|startsWith|endsWith|substring|toInt|toFloat|indexOf|lastIndexOf|equals|equalsIgnoreCase|toUpperCase|toLowerCase|charAt|length|c_str|concat|replace)\(' upstream/*.ino upstream/*.cpp | sort -u
grep -ohE '\b(millis|micros|delay|delayMicroseconds|random|randomSeed|pinMode|digitalWrite|digitalRead|attachInterrupt|detachInterrupt|digitalPinToInterrupt|ps_malloc|heap_caps_malloc)\b' upstream/*.ino upstream/*.cpp | sort -u
```
Expected: a lista fechada de métodos e funções. Implemente exatamente o que aparecer — nada além (YAGNI).

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_hal_arduino.cpp
#include "framework.h"
#include "Arduino.h"
#include "hal_control.h"

TEST(hal_arduino, millis_eh_monotonico_e_avanca_com_o_gancho) {
  halAdvanceForTest(0);
  uint32_t a = millis();
  halAdvanceForTest(5000);
  uint32_t b = millis();
  ASSERT_TRUE(b >= a + 5000);
  halAdvanceForTest(0);
}

TEST(hal_arduino, random_fica_no_intervalo) {
  randomSeed(42);
  for (int i = 0; i < 1000; ++i) {
    long v = random(10);
    ASSERT_TRUE(v >= 0 && v < 10);
  }
  for (int i = 0; i < 1000; ++i) {
    long v = random(5, 8);
    ASSERT_TRUE(v >= 5 && v < 8);
  }
}

TEST(hal_arduino, string_cobre_o_que_o_upstream_usa) {
  String s("  PUT x  ");
  s.trim();
  ASSERT_TRUE(s == String("PUT x"));
  ASSERT_TRUE(s.startsWith("PUT"));
  ASSERT_EQ(s.indexOf(' '), 3);
  ASSERT_TRUE(s.substring(4) == String("x"));
  ASSERT_EQ(String("42").toInt(), 42);
  ASSERT_TRUE(String("abc").equalsIgnoreCase("ABC"));
  ASSERT_EQ((int)String("abc").length(), 3);
}

TEST(hal_arduino, esp_restart_pede_reinicio_sem_matar_o_processo) {
  halClearRestartRequest();
  ASSERT_TRUE(!halRestartRequested());
  ESP.restart();
  ASSERT_TRUE(halRestartRequested());
  halClearRestartRequest();
}
```

Ajuste os nomes das macros de assert (`ASSERT_TRUE`/`ASSERT_EQ`) para os que `upstream/test/framework.h` realmente define — confira com `grep -n "define ASSERT" upstream/test/framework.h`.

- [ ] **Step 2b: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA de compilação — `Arduino.h` não existe.

- [ ] **Step 3: Escrever `hal/WString.h`**

```cpp
#pragma once
#include <string>
#include <cstdlib>
#include <cstring>
#include <cctype>

// String do Arduino sobre std::string. So os metodos que o upstream usa.
class String {
 public:
  String() = default;
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  explicit String(int v) : s_(std::to_string(v)) {}
  explicit String(unsigned v) : s_(std::to_string(v)) {}
  explicit String(long v) : s_(std::to_string(v)) {}

  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  char charAt(size_t i) const { return i < s_.size() ? s_[i] : '\0'; }
  char operator[](size_t i) const { return charAt(i); }

  void trim() {
    size_t b = s_.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) { s_.clear(); return; }
    size_t e = s_.find_last_not_of(" \t\r\n");
    s_ = s_.substr(b, e - b + 1);
  }
  bool startsWith(const char* p) const { return s_.rfind(p, 0) == 0; }
  bool endsWith(const char* p) const {
    size_t n = std::strlen(p);
    return s_.size() >= n && s_.compare(s_.size() - n, n, p) == 0;
  }
  String substring(size_t from) const {
    return from >= s_.size() ? String() : String(s_.substr(from));
  }
  String substring(size_t from, size_t to) const {
    if (from >= s_.size() || to <= from) return String();
    if (to > s_.size()) to = s_.size();
    return String(s_.substr(from, to - from));
  }
  int indexOf(char c) const {
    size_t p = s_.find(c); return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(const char* n) const {
    size_t p = s_.find(n); return p == std::string::npos ? -1 : (int)p;
  }
  int lastIndexOf(char c) const {
    size_t p = s_.rfind(c); return p == std::string::npos ? -1 : (int)p;
  }
  long toInt() const { return std::strtol(s_.c_str(), nullptr, 10); }
  double toFloat() const { return std::strtod(s_.c_str(), nullptr); }
  bool equalsIgnoreCase(const char* o) const {
    size_t n = std::strlen(o);
    if (n != s_.size()) return false;
    for (size_t i = 0; i < n; ++i)
      if (std::tolower((unsigned char)s_[i]) != std::tolower((unsigned char)o[i])) return false;
    return true;
  }
  void toUpperCase() { for (char& c : s_) c = (char)std::toupper((unsigned char)c); }
  void toLowerCase() { for (char& c : s_) c = (char)std::tolower((unsigned char)c); }

  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* o) { s_ += o; return *this; }
  friend String operator+(String a, const String& b) { a += b; return a; }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* o) const { return s_ == o; }
  bool operator!=(const String& o) const { return !(*this == o); }

  const std::string& std_str() const { return s_; }

 private:
  std::string s_;
};
```

Se o Step 1 mostrou um método que não está aqui, adicione-o agora com um assert correspondente no teste.

- [ ] **Step 4: Escrever `hal/Arduino.h`**

```cpp
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include "WString.h"
#include "HardwareSerial.h"

#define IRAM_ATTR
#define ICACHE_RAM_ATTR
#define PROGMEM
#define F(x) (x)
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define RISING 1
#define FALLING 2
#define CHANGE 3

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);

long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);

void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int digitalPinToInterrupt(int pin);
void attachInterrupt(int irq, void (*isr)(), int mode);
void detachInterrupt(int irq);

inline void* ps_malloc(size_t n) { return std::malloc(n); }
inline void* heap_caps_malloc(size_t n, int) { return std::malloc(n); }

struct EspClass {
  uint32_t getFreeHeap() const { return 200u * 1024u; }
  uint32_t getFreePsram() const { return 4u * 1024u * 1024u; }
  void restart();
};
extern EspClass ESP;

// O touch registra a ISR por attachInterrupt; hal/touch.cpp precisa dispara-la.
void halFireInterrupt(int pin);
```

- [ ] **Step 5: Escrever `hal/arduino.cpp`**

```cpp
#include "Arduino.h"
#include "hal_control.h"
#include <SDL3/SDL.h>
#include <random>
#include <unordered_map>
#include <string>

namespace {
uint32_t gTestOffsetMs = 0;
int64_t  gRtcOffsetSec = 0;
std::mt19937 gRng{std::random_device{}()};
std::unordered_map<int, void (*)()> gIsr;
bool gRestartRequested = false;
std::string gDataDir = ".";
std::string gSdRoot  = "./sdcard";
std::function<void(uint32_t)> gIdleWaiter;
}  // namespace

uint32_t halMillis() { return (uint32_t)SDL_GetTicks() + gTestOffsetMs; }
void halAdvanceForTest(uint32_t ms) { gTestOffsetMs = ms ? gTestOffsetMs + ms : 0; }
void halSetRtcOffsetForTest(int64_t s) { gRtcOffsetSec = s; }
int64_t halRtcOffsetSeconds() { return gRtcOffsetSec; }

void halSetDataDir(const char* d) { gDataDir = d ? d : "."; }
void halSetSdRoot(const char* d) { gSdRoot = d ? d : "./sdcard"; }
const char* halDataDir() { return gDataDir.c_str(); }
const char* halSdRoot() { return gSdRoot.c_str(); }

void halSetIdleWaiter(std::function<void(uint32_t)> w) { gIdleWaiter = std::move(w); }
void halIdleWait(uint32_t ms) {
  if (gIdleWaiter) gIdleWaiter(ms); else SDL_Delay(ms);
}

bool halRestartRequested() { return gRestartRequested; }
void halClearRestartRequest() { gRestartRequested = false; }

uint32_t millis() { return halMillis(); }
uint32_t micros() { return (uint32_t)(SDL_GetTicksNS() / 1000ull); }
void delay(uint32_t ms) { SDL_Delay(ms); }
void delayMicroseconds(uint32_t us) { SDL_DelayNS((uint64_t)us * 1000ull); }

long random(long max) {
  if (max <= 0) return 0;
  return (long)(std::uniform_int_distribution<long>(0, max - 1)(gRng));
}
long random(long min, long max) {
  if (max <= min) return min;
  return min + random(max - min);
}
void randomSeed(unsigned long seed) { gRng.seed((uint32_t)seed); }

void pinMode(int, int) {}
void digitalWrite(int, int) {}
int digitalRead(int) { return LOW; }
int digitalPinToInterrupt(int pin) { return pin; }
void attachInterrupt(int irq, void (*isr)(), int) { gIsr[irq] = isr; }
void detachInterrupt(int irq) { gIsr.erase(irq); }
void halFireInterrupt(int pin) {
  auto it = gIsr.find(pin);
  if (it != gIsr.end() && it->second) it->second();
}

EspClass ESP;
void EspClass::restart() { gRestartRequested = true; }
```

`halRtcOffsetSeconds()` é consumido por `hal/stubs.cpp` (Task 9); declare-o lá como `extern`.

- [ ] **Step 6: Rodar e ver passar**

Run:
```bash
cmake --build build --config Debug --target tamapoke_tests
ctest --test-dir build -C Debug -R tamapoke --output-on-failure
```
Expected: PASS nos 4 testes de `hal_arduino`.

- [ ] **Step 7: Commit**

```bash
git add hal/Arduino.h hal/WString.h hal/arduino.cpp tests/test_hal_arduino.cpp
git commit -m "feat(hal): Arduino.h, String e relogio"
```

### Task 6: `Serial` como console de debug

O upstream lê comandos com `Serial.available()` / `readStringUntil('\n')` e `Serial.readBytes`. Isso preserva `STATS`, `LVL`, `HATCH`, `TIME <epoch>`, `WIPE`.

**Files:**
- Create: `hal/HardwareSerial.h`, `hal/serial.cpp`
- Test: `tests/test_hal_serial.cpp`

- [ ] **Step 1: Levantar os métodos usados**

Run:
```bash
grep -ohE 'Serial\.[a-zA-Z_]+' upstream/*.ino upstream/*.cpp | sort -u
```
Expected: lista fechada. Cubra exatamente ela.

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_hal_serial.cpp
#include "framework.h"
#include "Arduino.h"
#include "hal_control.h"

TEST(hal_serial, linha_injetada_chega_por_readStringUntil) {
  halSerialInject("STATS\n");
  ASSERT_TRUE(Serial.available() > 0);
  String line = Serial.readStringUntil('\n');
  line.trim();
  // O String da HAL so tem o que o upstream usa: nao ha operator==. Compare
  // pelo c_str() de um String LOCAL (de um temporario, o ponteiro ficaria pendurado).
  CHECK_STREQ(line.c_str(), "STATS");
  ASSERT_EQ(Serial.available(), 0);
}

TEST(hal_serial, readBytes_le_o_que_foi_injetado) {
  halSerialInject("ABCD");
  char buf[4] = {0};
  size_t n = Serial.readBytes(buf, 4);
  ASSERT_EQ((int)n, 4);
  ASSERT_EQ(buf[0], 'A');
  ASSERT_EQ(buf[3], 'D');
}

TEST(hal_serial, printf_nao_quebra_sem_console) {
  Serial.printf("nivel=%d\n", 7);
  Serial.println("ok");
}
```

- [ ] **Step 2b: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `HardwareSerial.h` não existe.

- [ ] **Step 3: Escrever `hal/HardwareSerial.h`**

```cpp
#pragma once
#include <cstdarg>
#include <cstddef>
#include <cstdint>
class String;

class HardwareSerial {
 public:
  void begin(unsigned long = 115200) {}
  void setRxBufferSize(size_t) {}
  void setTxTimeoutMs(uint32_t) {}
  void flush() {}
  operator bool() const { return true; }

  int available();
  int read();
  size_t readBytes(char* buf, size_t len);
  String readStringUntil(char terminator);

  size_t write(uint8_t b);
  size_t write(const uint8_t* buf, size_t len);
  void print(const char* s);
  void print(int v);
  void println(const char* s = "");
  void println(int v);
  int printf(const char* fmt, ...);
};
extern HardwareSerial Serial;
```

Se o Step 1 mostrou um método fora desta lista, adicione-o com um assert no teste.

- [ ] **Step 4: Escrever `hal/serial.cpp`**

```cpp
#include "HardwareSerial.h"
#include "WString.h"
#include "hal_control.h"
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <filesystem>

namespace {
std::mutex gMu;
std::deque<char> gIn;
std::FILE* gLog = nullptr;

std::FILE* logFile() {
  if (gLog) return gLog;
  std::error_code ec;
  std::filesystem::create_directories(halDataDir(), ec);
  std::string path = std::string(halDataDir()) + "/log.txt";
  // rotacao simples em 1 MB
  if (std::filesystem::exists(path, ec) &&
      std::filesystem::file_size(path, ec) > 1024u * 1024u) {
    std::filesystem::rename(path, path + ".1", ec);
  }
  gLog = std::fopen(path.c_str(), "ab");
  return gLog;
}

void emit(const char* s, size_t n) {
  std::fwrite(s, 1, n, stdout);
  if (std::FILE* f = logFile()) { std::fwrite(s, 1, n, f); std::fflush(f); }
}
}  // namespace

HardwareSerial Serial;

void halSerialInject(const char* line) {
  if (!line) return;
  std::lock_guard<std::mutex> g(gMu);
  for (const char* p = line; *p; ++p) gIn.push_back(*p);
}

int HardwareSerial::available() {
  std::lock_guard<std::mutex> g(gMu);
  return (int)gIn.size();
}
int HardwareSerial::read() {
  std::lock_guard<std::mutex> g(gMu);
  if (gIn.empty()) return -1;
  char c = gIn.front(); gIn.pop_front(); return (unsigned char)c;
}
size_t HardwareSerial::readBytes(char* buf, size_t len) {
  std::lock_guard<std::mutex> g(gMu);
  size_t n = 0;
  while (n < len && !gIn.empty()) { buf[n++] = gIn.front(); gIn.pop_front(); }
  return n;
}
String HardwareSerial::readStringUntil(char term) {
  std::lock_guard<std::mutex> g(gMu);
  std::string out;
  while (!gIn.empty()) {
    char c = gIn.front(); gIn.pop_front();
    if (c == term) break;
    out.push_back(c);
  }
  return String(out);
}

size_t HardwareSerial::write(uint8_t b) { char c = (char)b; emit(&c, 1); return 1; }
size_t HardwareSerial::write(const uint8_t* buf, size_t len) {
  emit((const char*)buf, len); return len;
}
void HardwareSerial::print(const char* s) { emit(s, std::strlen(s)); }
void HardwareSerial::print(int v) { char b[32]; int n = std::snprintf(b, sizeof b, "%d", v); emit(b, n); }
void HardwareSerial::println(const char* s) { print(s); emit("\n", 1); }
void HardwareSerial::println(int v) { print(v); emit("\n", 1); }
int HardwareSerial::printf(const char* fmt, ...) {
  char b[1024];
  va_list ap; va_start(ap, fmt);
  int n = std::vsnprintf(b, sizeof b, fmt, ap);
  va_end(ap);
  if (n > 0) emit(b, (size_t)(n < (int)sizeof b ? n : (int)sizeof b - 1));
  return n;
}
```

Adicione `#include <cstring>` se o compilador reclamar de `strlen`.

- [ ] **Step 4b: Ligar o `Serial` ao `Arduino.h`**

A Task 5 deixou `hal/Arduino.h` sem o include de `HardwareSerial.h` (o arquivo ainda não existia). Agora que existe, acrescente em `hal/Arduino.h`, junto dos outros includes (troque o comentário `// HardwareSerial.h entra na Task 6`):

```cpp
#include "HardwareSerial.h"
```

O teste `tests/test_hal_serial.cpp` inclui `Arduino.h` e usa `Serial`, então só compila depois deste passo.

- [ ] **Step 5: Rodar e ver passar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS em `hal_serial`.

- [ ] **Step 6: Commit**

```bash
git add hal/HardwareSerial.h hal/serial.cpp tests/test_hal_serial.cpp
git commit -m "feat(hal): Serial com entrada injetavel e log em arquivo"
```

### Task 7: `Preferences` persistente em JSON

**Files:**
- Create: `hal/Preferences.h`, `hal/preferences.cpp`
- Modify: `CMakeLists.txt` (nlohmann/json)
- Test: `tests/test_hal_prefs.cpp`

- [ ] **Step 1: Copiar a interface exata do shim do upstream**

Run:
```bash
cat upstream/test/shim/Preferences.h
grep -ohE 'prefs\.[a-zA-Z]+|Preferences [a-zA-Z]+' upstream/*.cpp upstream/*.ino | sort -u
```
Expected: a interface que o upstream espera. A nossa precisa ser **assinatura por assinatura** igual — o upstream não será alterado.

- [ ] **Step 2: Adicionar nlohmann/json ao `CMakeLists.txt`**

```cmake
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)
```

E ligue nos dois alvos que usam HAL:
```cmake
target_link_libraries(tamapoke_tests PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json)
```

- [ ] **Step 3: Escrever o teste falhando**

```cpp
// tests/test_hal_prefs.cpp
#include "framework.h"
#include "Preferences.h"
#include "hal_control.h"
#include <filesystem>
#include <string>

static void usarDirTemporario(const char* nome) {
  auto dir = std::filesystem::temp_directory_path() / nome;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  halSetDataDir(dir.string().c_str());
}

TEST(hal_prefs, round_trip_sobrevive_a_recarga) {
  usarDirTemporario("tamapoke_prefs_a");
  {
    Preferences p;
    ASSERT_TRUE(p.begin("pet", false));
    p.putUInt("ep", 123u);
    p.putUChar("lvl", 7);
    p.putString("nick", "Pika");
    const uint8_t blob[3] = {1, 2, 3};
    p.putBytes("blob", blob, sizeof blob);
    p.end();
  }
  {
    Preferences p;
    ASSERT_TRUE(p.begin("pet", true));
    ASSERT_EQ((int)p.getUInt("ep", 0), 123);
    ASSERT_EQ((int)p.getUChar("lvl", 0), 7);
    char nick[16] = {0};
    p.getString("nick", nick, sizeof nick);
    ASSERT_TRUE(std::string(nick) == "Pika");
    uint8_t blob[3] = {0};
    ASSERT_EQ((int)p.getBytes("blob", blob, sizeof blob), 3);
    ASSERT_EQ((int)blob[2], 3);
    p.end();
  }
}

TEST(hal_prefs, tipo_errado_devolve_default_como_na_nvs) {
  usarDirTemporario("tamapoke_prefs_b");
  Preferences p;
  p.begin("pet", false);
  p.putUChar("lvl", 7);
  ASSERT_EQ((int)p.getUInt("lvl", 99u), 99);   // gravado como UChar, lido como UInt
  ASSERT_EQ((int)p.getUChar("lvl", 0), 7);
  p.end();
}

TEST(hal_prefs, clear_so_apaga_o_proprio_namespace) {
  usarDirTemporario("tamapoke_prefs_c");
  { Preferences p; p.begin("pet", false); p.putUInt("ep", 1u); p.end(); }
  { Preferences p; p.begin("i18n", false); p.putUChar("lang", 3); p.end(); }
  { Preferences p; p.begin("pet", false); p.clear(); p.end(); }
  { Preferences p; p.begin("pet", true);  ASSERT_TRUE(!p.isKey("ep")); p.end(); }
  { Preferences p; p.begin("i18n", true); ASSERT_EQ((int)p.getUChar("lang", 0), 3); p.end(); }
}

TEST(hal_prefs, isKey_e_remove) {
  usarDirTemporario("tamapoke_prefs_d");
  Preferences p;
  p.begin("pet", false);
  p.putUInt("ep", 5u);
  ASSERT_TRUE(p.isKey("ep"));
  p.remove("ep");
  ASSERT_TRUE(!p.isKey("ep"));
  p.end();
}

TEST(hal_prefs, getString_respeita_maxLen) {
  usarDirTemporario("tamapoke_prefs_e");
  Preferences p;
  p.begin("pet", false);
  p.putString("nick", "NomeMuitoLongo");
  char buf[5] = {0};
  p.getString("nick", buf, sizeof buf);
  ASSERT_EQ((int)std::string(buf).size(), 4);  // 4 chars + terminador
  p.end();
}

TEST(hal_prefs, arquivo_corrompido_nao_trava_e_vira_bad) {
  usarDirTemporario("tamapoke_prefs_f");
  std::string path = std::string(halDataDir()) + "/nvs.json";
  { std::FILE* f = std::fopen(path.c_str(), "wb"); std::fputs("{nao e json", f); std::fclose(f); }
  Preferences p;
  ASSERT_TRUE(p.begin("pet", true));            // nao trava
  ASSERT_EQ((int)p.getUInt("ep", 42u), 42);     // comeca limpo
  p.end();
  bool achouBad = false;
  for (auto& e : std::filesystem::directory_iterator(halDataDir()))
    if (e.path().filename().string().rfind("nvs.json.bad-", 0) == 0) achouBad = true;
  ASSERT_TRUE(achouBad);
}
```

- [ ] **Step 3b: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `Preferences.h` não existe.

- [ ] **Step 4: Escrever `hal/Preferences.h`**

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Mesma interface de upstream/test/shim/Preferences.h. Se o Step 1 mostrou
// assinatura diferente, a daqui se ajusta — o upstream nunca.
class Preferences {
 public:
  bool begin(const char* name, bool readOnly = false);
  void end();
  bool clear();
  bool remove(const char* key);
  bool isKey(const char* key);

  size_t putChar(const char* key, int8_t v);
  size_t putUChar(const char* key, uint8_t v);
  size_t putShort(const char* key, int16_t v);
  size_t putUShort(const char* key, uint16_t v);
  size_t putInt(const char* key, int32_t v);
  size_t putUInt(const char* key, uint32_t v);
  size_t putLong64(const char* key, int64_t v);
  size_t putULong64(const char* key, uint64_t v);
  size_t putBool(const char* key, bool v);
  size_t putString(const char* key, const char* v);
  size_t putBytes(const char* key, const void* v, size_t len);

  int8_t   getChar(const char* key, int8_t def = 0);
  uint8_t  getUChar(const char* key, uint8_t def = 0);
  int16_t  getShort(const char* key, int16_t def = 0);
  uint16_t getUShort(const char* key, uint16_t def = 0);
  int32_t  getInt(const char* key, int32_t def = 0);
  uint32_t getUInt(const char* key, uint32_t def = 0);
  int64_t  getLong64(const char* key, int64_t def = 0);
  uint64_t getULong64(const char* key, uint64_t def = 0);
  bool     getBool(const char* key, bool def = false);
  size_t   getString(const char* key, char* out, size_t maxLen);
  size_t   getBytesLength(const char* key);
  size_t   getBytes(const char* key, void* out, size_t maxLen);

 private:
  std::string ns_;
  bool readOnly_ = false;
  bool open_ = false;
};
```

- [ ] **Step 5: Escrever `hal/preferences.cpp`**

```cpp
#include "Preferences.h"
#include "hal_control.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <vector>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace {
json gDoc = json::object();
bool gLoaded = false;
std::string gLoadedFrom;

std::string nvsPath() { return std::string(halDataDir()) + "/nvs.json"; }

const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64encode(const uint8_t* d, size_t n) {
  std::string o;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = d[i] << 16;
    if (i + 1 < n) v |= d[i + 1] << 8;
    if (i + 2 < n) v |= d[i + 2];
    o += kB64[(v >> 18) & 63];
    o += kB64[(v >> 12) & 63];
    o += (i + 1 < n) ? kB64[(v >> 6) & 63] : '=';
    o += (i + 2 < n) ? kB64[v & 63] : '=';
  }
  return o;
}

std::vector<uint8_t> b64decode(const std::string& s) {
  auto idx = [](char c) -> int {
    const char* p = std::strchr(kB64, c);
    return p ? (int)(p - kB64) : -1;
  };
  std::vector<uint8_t> o;
  uint32_t acc = 0; int bits = 0;
  for (char c : s) {
    int v = idx(c);
    if (v < 0) continue;              // '=' e lixo
    acc = (acc << 6) | (uint32_t)v; bits += 6;
    if (bits >= 8) { bits -= 8; o.push_back((uint8_t)((acc >> bits) & 0xFF)); }
  }
  return o;
}

void quarantine(const std::string& path) {
  std::error_code ec;
  std::string bad = path + ".bad-" + std::to_string((long long)std::time(nullptr));
  std::filesystem::rename(path, bad, ec);
}

void loadOnce() {
  std::string path = nvsPath();
  if (gLoaded && gLoadedFrom == path) return;
  gDoc = json::object();
  gLoaded = true;
  gLoadedFrom = path;
  std::ifstream in(path, std::ios::binary);
  if (!in) return;
  try {
    json parsed = json::parse(in);
    if (!parsed.is_object()) throw std::runtime_error("raiz nao e objeto");
    gDoc = std::move(parsed);
  } catch (...) {
    in.close();
    quarantine(path);
    gDoc = json::object();
  }
}

void saveAtomic() {
  std::error_code ec;
  std::filesystem::create_directories(halDataDir(), ec);
  std::string path = nvsPath();
  std::string tmp = path + ".tmp";
  { std::ofstream out(tmp, std::ios::binary | std::ios::trunc); out << gDoc.dump(2); }
#ifdef _WIN32
  MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
#else
  std::filesystem::rename(tmp, path, ec);
#endif
}

// Cada chave e guardada como {"t":"<tipo>","v":<valor>} para reproduzir a
// tipagem da NVS: ler com o getter errado devolve o default.
json* findEntry(json& doc, const std::string& ns, const char* key) {
  auto n = doc.find(ns);
  if (n == doc.end() || !n->is_object()) return nullptr;
  auto k = n->find(key);
  if (k == n->end() || !k->is_object()) return nullptr;
  return &(*k);
}
}  // namespace

bool Preferences::begin(const char* name, bool readOnly) {
  ns_ = name ? name : "";
  readOnly_ = readOnly;
  open_ = true;
  loadOnce();
  if (!gDoc.contains(ns_)) gDoc[ns_] = json::object();
  return true;
}
void Preferences::end() { open_ = false; }

bool Preferences::clear() {
  if (!open_ || readOnly_) return false;
  gDoc[ns_] = json::object();
  saveAtomic();
  return true;
}
bool Preferences::remove(const char* key) {
  if (!open_ || readOnly_) return false;
  gDoc[ns_].erase(key);
  saveAtomic();
  return true;
}
bool Preferences::isKey(const char* key) {
  return open_ && findEntry(gDoc, ns_, key) != nullptr;
}

namespace {
template <typename T>
size_t putTyped(json& doc, const std::string& ns, bool readOnly, bool open,
                const char* key, const char* type, T v) {
  if (!open || readOnly) return 0;
  doc[ns][key] = json{{"t", type}, {"v", v}};
  saveAtomic();
  return sizeof(T);
}

template <typename T>
T getTyped(json& doc, const std::string& ns, bool open,
           const char* key, const char* type, T def) {
  if (!open) return def;
  json* e = findEntry(doc, ns, key);
  if (!e) return def;
  auto t = e->find("t");
  if (t == e->end() || !t->is_string() || t->get<std::string>() != type) return def;
  auto v = e->find("v");
  if (v == e->end()) return def;
  try { return v->get<T>(); } catch (...) { return def; }
}
}  // namespace

size_t Preferences::putChar(const char* k, int8_t v)      { return putTyped(gDoc, ns_, readOnly_, open_, k, "i8",  v); }
size_t Preferences::putUChar(const char* k, uint8_t v)    { return putTyped(gDoc, ns_, readOnly_, open_, k, "u8",  v); }
size_t Preferences::putShort(const char* k, int16_t v)    { return putTyped(gDoc, ns_, readOnly_, open_, k, "i16", v); }
size_t Preferences::putUShort(const char* k, uint16_t v)  { return putTyped(gDoc, ns_, readOnly_, open_, k, "u16", v); }
size_t Preferences::putInt(const char* k, int32_t v)      { return putTyped(gDoc, ns_, readOnly_, open_, k, "i32", v); }
size_t Preferences::putUInt(const char* k, uint32_t v)    { return putTyped(gDoc, ns_, readOnly_, open_, k, "u32", v); }
size_t Preferences::putLong64(const char* k, int64_t v)   { return putTyped(gDoc, ns_, readOnly_, open_, k, "i64", v); }
size_t Preferences::putULong64(const char* k, uint64_t v) { return putTyped(gDoc, ns_, readOnly_, open_, k, "u64", v); }
size_t Preferences::putBool(const char* k, bool v)        { return putTyped(gDoc, ns_, readOnly_, open_, k, "b",   v); }

int8_t   Preferences::getChar(const char* k, int8_t d)      { return getTyped(gDoc, ns_, open_, k, "i8",  d); }
uint8_t  Preferences::getUChar(const char* k, uint8_t d)    { return getTyped(gDoc, ns_, open_, k, "u8",  d); }
int16_t  Preferences::getShort(const char* k, int16_t d)    { return getTyped(gDoc, ns_, open_, k, "i16", d); }
uint16_t Preferences::getUShort(const char* k, uint16_t d)  { return getTyped(gDoc, ns_, open_, k, "u16", d); }
int32_t  Preferences::getInt(const char* k, int32_t d)      { return getTyped(gDoc, ns_, open_, k, "i32", d); }
uint32_t Preferences::getUInt(const char* k, uint32_t d)    { return getTyped(gDoc, ns_, open_, k, "u32", d); }
int64_t  Preferences::getLong64(const char* k, int64_t d)   { return getTyped(gDoc, ns_, open_, k, "i64", d); }
uint64_t Preferences::getULong64(const char* k, uint64_t d) { return getTyped(gDoc, ns_, open_, k, "u64", d); }
bool     Preferences::getBool(const char* k, bool d)        { return getTyped(gDoc, ns_, open_, k, "b",   d); }

size_t Preferences::putString(const char* k, const char* v) {
  if (!open_ || readOnly_) return 0;
  gDoc[ns_][k] = json{{"t", "s"}, {"v", v ? v : ""}};
  saveAtomic();
  return v ? std::strlen(v) : 0;
}
size_t Preferences::getString(const char* k, char* out, size_t maxLen) {
  if (!out || maxLen == 0) return 0;
  out[0] = '\0';
  std::string s = getTyped<std::string>(gDoc, ns_, open_, k, "s", std::string());
  size_t n = s.size() < maxLen - 1 ? s.size() : maxLen - 1;
  std::memcpy(out, s.data(), n);
  out[n] = '\0';
  return n;
}
size_t Preferences::putBytes(const char* k, const void* v, size_t len) {
  if (!open_ || readOnly_) return 0;
  gDoc[ns_][k] = json{{"t", "blob"}, {"v", b64encode((const uint8_t*)v, len)}};
  saveAtomic();
  return len;
}
size_t Preferences::getBytesLength(const char* k) {
  std::string s = getTyped<std::string>(gDoc, ns_, open_, k, "blob", std::string());
  return b64decode(s).size();
}
size_t Preferences::getBytes(const char* k, void* out, size_t maxLen) {
  std::string s = getTyped<std::string>(gDoc, ns_, open_, k, "blob", std::string());
  std::vector<uint8_t> d = b64decode(s);
  size_t n = d.size() < maxLen ? d.size() : maxLen;
  if (n && out) std::memcpy(out, d.data(), n);
  return n;
}
```

Nota: `loadOnce()` recarrega quando `halSetDataDir` muda, o que é o que deixa cada teste usar seu próprio diretório temporário.

- [ ] **Step 6: Rodar e ver passar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 6 testes de `hal_prefs`.

- [ ] **Step 7: Commit**

```bash
git add hal/Preferences.h hal/preferences.cpp tests/test_hal_prefs.cpp CMakeLists.txt
git commit -m "feat(hal): Preferences persistente em JSON com tipagem NVS"
```

### Task 8: `SD_MMC` e `File`

**Files:**
- Create: `hal/FS.h`, `hal/SD_MMC.h`, `hal/sd.cpp`
- Test: `tests/test_hal_sd.cpp`

- [ ] **Step 1: Levantar o que `sdmon.cpp` usa**

Run:
```bash
grep -ohE 'SD_MMC\.[a-zA-Z]+|\bf\.[a-zA-Z]+\(|\bfile\.[a-zA-Z]+\(' upstream/sdmon.cpp | sort -u
cat upstream/test/shim/FS.h 2>/dev/null | head -60
ls upstream/tools/sdcard/mons | head -3
```
Expected: lista fechada de métodos e a confirmação de que os `.bin` existem.

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_hal_sd.cpp
#include "framework.h"
#include "SD_MMC.h"
#include "hal_control.h"
#include <filesystem>
#include <fstream>

static std::filesystem::path prepararCartao() {
  auto root = std::filesystem::temp_directory_path() / "tamapoke_sd";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root / "mons", ec);
  std::ofstream f(root / "mons" / "p001.bin", std::ios::binary);
  const char data[8] = {'T','A','M','A',1,2,3,4};
  f.write(data, sizeof data);
  f.close();
  halSetSdRoot(root.string().c_str());
  return root;
}

TEST(hal_sd, abre_le_e_fecha_um_arquivo_existente) {
  prepararCartao();
  ASSERT_TRUE(SD_MMC.begin());
  File f = SD_MMC.open("/mons/p001.bin", FILE_READ);
  ASSERT_TRUE((bool)f);
  ASSERT_EQ((int)f.size(), 8);
  uint8_t buf[8] = {0};
  ASSERT_EQ((int)f.read(buf, 8), 8);
  ASSERT_EQ((int)buf[0], 'T');
  ASSERT_EQ((int)buf[7], 4);
  f.close();
}

TEST(hal_sd, arquivo_inexistente_da_File_falso) {
  prepararCartao();
  File f = SD_MMC.open("/mons/naoexiste.bin", FILE_READ);
  ASSERT_TRUE(!(bool)f);
}

TEST(hal_sd, seek_e_available) {
  prepararCartao();
  File f = SD_MMC.open("/mons/p001.bin", FILE_READ);
  ASSERT_TRUE(f.seek(4));
  ASSERT_EQ((int)f.available(), 4);
  uint8_t b = 0;
  f.read(&b, 1);
  ASSERT_EQ((int)b, 1);
  f.close();
}

TEST(hal_sd, openNextFile_lista_o_diretorio) {
  prepararCartao();
  File dir = SD_MMC.open("/mons", FILE_READ);
  ASSERT_TRUE((bool)dir);
  ASSERT_TRUE(dir.isDirectory());
  File child = dir.openNextFile();
  ASSERT_TRUE((bool)child);
  ASSERT_TRUE(std::string(child.name()).find("p001.bin") != std::string::npos);
  child.close();
  dir.close();
}

TEST(hal_sd, mkdir_e_escrita) {
  auto root = prepararCartao();
  ASSERT_TRUE(SD_MMC.mkdir("/novo"));
  File f = SD_MMC.open("/novo/x.bin", FILE_WRITE);
  ASSERT_TRUE((bool)f);
  const uint8_t b[2] = {9, 9};
  ASSERT_EQ((int)f.write(b, 2), 2);
  f.close();
  ASSERT_TRUE(std::filesystem::exists(root / "novo" / "x.bin"));
}

TEST(hal_sd, cardSize_nao_eh_zero_quando_a_raiz_existe) {
  prepararCartao();
  ASSERT_TRUE(SD_MMC.cardSize() > 0);
}
```

- [ ] **Step 2b: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `SD_MMC.h` não existe.

- [ ] **Step 3: Escrever `hal/FS.h`**

```cpp
#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <filesystem>

#define FILE_READ   "r"
#define FILE_WRITE  "w"
#define FILE_APPEND "a"

class File {
 public:
  File() = default;
  File(const std::string& fullPath, const char* mode);
  explicit File(const std::string& dirPath);   // diretorio

  operator bool() const { return ok_; }
  size_t size() const { return size_; }
  size_t read(uint8_t* buf, size_t len);
  int read();
  size_t write(const uint8_t* buf, size_t len);
  size_t write(uint8_t b);
  int available() const;
  bool seek(uint32_t pos);
  size_t position() const;
  void close();
  const char* name() const { return name_.c_str(); }
  bool isDirectory() const { return isDir_; }
  File openNextFile();
  void rewindDirectory();

 private:
  struct Handle { std::FILE* f = nullptr; ~Handle() { if (f) std::fclose(f); } };
  std::shared_ptr<Handle> h_;
  std::string name_;
  std::string path_;
  bool ok_ = false;
  bool isDir_ = false;
  size_t size_ = 0;
  std::shared_ptr<std::filesystem::directory_iterator> it_;
};
```

- [ ] **Step 4: Escrever `hal/SD_MMC.h`**

```cpp
#pragma once
#include "FS.h"

class SDMMCClass {
 public:
  bool begin(const char* mountpoint = "/sdcard", bool mode1bit = false,
             bool formatIfEmpty = false);
  void end() {}
  void setPins(int, int, int, int, int, int) {}
  void setPins(int, int, int) {}
  File open(const char* path, const char* mode = FILE_READ);
  bool exists(const char* path);
  bool mkdir(const char* path);
  bool remove(const char* path);
  uint64_t cardSize();
  uint64_t totalBytes();
  uint64_t usedBytes();
  int cardType();
};
extern SDMMCClass SD_MMC;
```

Se o Step 1 mostrou outra assinatura de `setPins` ou outro método, ajuste **aqui** para bater com a chamada do upstream.

- [ ] **Step 5: Escrever `hal/sd.cpp`**

```cpp
#include "SD_MMC.h"
#include "hal_control.h"
#include <cstring>

namespace fs = std::filesystem;

namespace {
std::string resolve(const char* path) {
  std::string p = path ? path : "";
  while (!p.empty() && (p.front() == '/' || p.front() == '\\')) p.erase(p.begin());
  return std::string(halSdRoot()) + "/" + p;
}
}  // namespace

File::File(const std::string& fullPath, const char* mode) {
  h_ = std::make_shared<Handle>();
  std::string m = std::string(mode ? mode : "r") + "b";
  h_->f = std::fopen(fullPath.c_str(), m.c_str());
  if (!h_->f) return;
  ok_ = true;
  path_ = fullPath;
  name_ = fullPath;
  std::error_code ec;
  size_t s = (size_t)fs::file_size(fullPath, ec);
  size_ = ec ? 0 : s;
}

File::File(const std::string& dirPath) {
  std::error_code ec;
  if (!fs::is_directory(dirPath, ec)) return;
  ok_ = true;
  isDir_ = true;
  path_ = dirPath;
  name_ = dirPath;
  it_ = std::make_shared<fs::directory_iterator>(dirPath, ec);
}

size_t File::read(uint8_t* buf, size_t len) {
  if (!ok_ || isDir_ || !h_ || !h_->f) return 0;
  return std::fread(buf, 1, len, h_->f);
}
int File::read() {
  uint8_t b = 0;
  return read(&b, 1) == 1 ? (int)b : -1;
}
size_t File::write(const uint8_t* buf, size_t len) {
  if (!ok_ || isDir_ || !h_ || !h_->f) return 0;
  return std::fwrite(buf, 1, len, h_->f);
}
size_t File::write(uint8_t b) { return write(&b, 1); }
int File::available() const {
  if (!ok_ || isDir_ || !h_ || !h_->f) return 0;
  long pos = std::ftell(h_->f);
  return pos < 0 ? 0 : (int)(size_ - (size_t)pos);
}
bool File::seek(uint32_t pos) {
  if (!ok_ || isDir_ || !h_ || !h_->f) return false;
  return std::fseek(h_->f, (long)pos, SEEK_SET) == 0;
}
size_t File::position() const {
  if (!ok_ || isDir_ || !h_ || !h_->f) return 0;
  long p = std::ftell(h_->f);
  return p < 0 ? 0 : (size_t)p;
}
void File::close() {
  if (h_ && h_->f) { std::fclose(h_->f); h_->f = nullptr; }
  ok_ = false;
}
File File::openNextFile() {
  if (!isDir_ || !it_) return File();
  std::error_code ec;
  auto& it = *it_;
  if (it == fs::directory_iterator()) return File();
  fs::path p = it->path();
  it.increment(ec);
  if (fs::is_directory(p, ec)) return File(p.string());
  return File(p.string(), FILE_READ);
}
void File::rewindDirectory() {
  if (!isDir_) return;
  std::error_code ec;
  it_ = std::make_shared<fs::directory_iterator>(path_, ec);
}

SDMMCClass SD_MMC;

bool SDMMCClass::begin(const char*, bool, bool) {
  std::error_code ec;
  return fs::is_directory(halSdRoot(), ec);
}
File SDMMCClass::open(const char* path, const char* mode) {
  std::string full = resolve(path);
  std::error_code ec;
  if (fs::is_directory(full, ec)) return File(full);
  if (std::strcmp(mode, FILE_READ) == 0 && !fs::exists(full, ec)) return File();
  return File(full, mode);
}
bool SDMMCClass::exists(const char* path) {
  std::error_code ec;
  return fs::exists(resolve(path), ec);
}
bool SDMMCClass::mkdir(const char* path) {
  std::error_code ec;
  fs::create_directories(resolve(path), ec);
  return !ec;
}
bool SDMMCClass::remove(const char* path) {
  std::error_code ec;
  return fs::remove(resolve(path), ec);
}
uint64_t SDMMCClass::cardSize() {
  std::error_code ec;
  auto si = fs::space(halSdRoot(), ec);
  return ec ? 0 : si.capacity;
}
uint64_t SDMMCClass::totalBytes() { return cardSize(); }
uint64_t SDMMCClass::usedBytes() {
  std::error_code ec;
  auto si = fs::space(halSdRoot(), ec);
  return ec ? 0 : (si.capacity - si.available);
}
int SDMMCClass::cardType() { return 3; }  // CARD_SDHC
```

- [ ] **Step 6: Rodar e ver passar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 6 testes de `hal_sd`.

- [ ] **Step 7: Teste de integração com um sprite real**

```cpp
// acrescentar em tests/test_hal_sd.cpp
#include "sdmon.h"   // header do upstream que declara PmdMon

TEST(hal_sd, carrega_um_pmd_real_do_upstream) {
  halSetSdRoot("upstream/tools/sdcard");
  ASSERT_TRUE(SD_MMC.begin());
  PmdMon pmd;
  ASSERT_TRUE(pmd.load(25, false));
  ASSERT_TRUE(pmd.loaded);
  ASSERT_TRUE(pmd.has(PMD_IDLE));
}
```

Confira os nomes reais com `grep -nE "class PmdMon|bool load|PMD_IDLE|loaded" upstream/sdmon.h` e ajuste o teste à API real. Adicione `upstream/sdmon.cpp` ao alvo `tamapoke_tests` no `CMakeLists.txt`:

```cmake
target_sources(tamapoke_tests PRIVATE upstream/sdmon.cpp)
```

O teste roda a partir da raiz do repo; configure o diretório de trabalho do ctest:
```cmake
set_tests_properties(tamapoke PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
```

- [ ] **Step 8: Rodar e ver passar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS, inclusive `carrega_um_pmd_real_do_upstream`.

- [ ] **Step 9: Commit**

```bash
git add hal/FS.h hal/SD_MMC.h hal/sd.cpp tests/test_hal_sd.cpp CMakeLists.txt
git commit -m "feat(hal): SD_MMC sobre o sistema de arquivos"
```

### Task 9: Stubs de Wire, GPIO, RTC, PMU e sleep

**Files:**
- Create: `hal/Wire.h`, `hal/esp_sleep.h`, `hal/driver/gpio.h`, `hal/SensorPCF85063.hpp`, `hal/XPowersLib.h`, `hal/stubs.cpp`
- Test: `tests/test_hal_rtc.cpp`
- Modify: `CMakeLists.txt` (adicionar `upstream/rtcbat.cpp` ao alvo de testes)

- [ ] **Step 1: Levantar exatamente o que `rtcbat.cpp` chama**

Run:
```bash
grep -ohE '(rtc|pmu|PMU|Wire)\.[a-zA-Z_0-9]+' upstream/rtcbat.cpp upstream/TamaPoke.ino upstream/audio.cpp | sort -u
grep -ohE 'esp_[a-z_]+\(|gpio_[a-z_]+\(' upstream/*.ino upstream/*.cpp | sort -u
grep -nE 'rtcEpoch|batPercent|usbPresent|batShouldShutdown|pwrShortPressed|rtcSetEpoch' upstream/rtcbat.h
```
Expected: a lista fechada de métodos a stubar e as assinaturas públicas de `rtcbat.h`. Implemente exatamente ela.

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_hal_rtc.cpp
#include "framework.h"
#include "hal_control.h"
#include "rtcbat.h"
#include <ctime>
#include <cstdlib>

TEST(hal_rtc, epoch_segue_o_relogio_do_windows) {
  halSetRtcOffsetForTest(0);
  rtcBegin();
  long long agora = (long long)std::time(nullptr);
  long long lido = (long long)rtcEpoch();
  ASSERT_TRUE(std::llabs(lido - agora) <= 2);
}

TEST(hal_rtc, offset_de_teste_avanca_o_epoch) {
  halSetRtcOffsetForTest(0);
  long long base = (long long)rtcEpoch();
  halSetRtcOffsetForTest(7200);          // +2 h
  long long depois = (long long)rtcEpoch();
  ASSERT_TRUE(depois - base >= 7190);
  halSetRtcOffsetForTest(0);
}

TEST(hal_rtc, bateria_e_energia_sao_estaveis) {
  ASSERT_EQ((int)batPercent(), -1);
  ASSERT_TRUE(usbPresent());
  ASSERT_TRUE(!batShouldShutdown());
}

TEST(hal_rtc, pwr_short_press_eh_consumido_uma_vez) {
  while (pwrShortPressed()) {}           // drena
  halPwrShortPress();
  ASSERT_TRUE(pwrShortPressed());
  ASSERT_TRUE(!pwrShortPressed());
}
```

Ajuste os nomes (`rtcBegin`, `rtcEpoch`, ...) aos reais de `rtcbat.h` descobertos no Step 1.

- [ ] **Step 2b: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `Wire.h`/`SensorPCF85063.hpp` não existem.

- [ ] **Step 3: Escrever `hal/Wire.h`**

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

class TwoWire {
 public:
  explicit TwoWire(int = 0) {}
  bool begin(int sda = -1, int scl = -1, uint32_t freq = 0) { (void)sda; (void)scl; (void)freq; return true; }
  void end() {}
  void setClock(uint32_t) {}
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool = true) { return 0; }   // 0 = sucesso
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t*, size_t n) { return n; }
  uint8_t requestFrom(uint8_t, size_t) { return 0; }
  int available() { return 0; }
  int read() { return -1; }
};
extern TwoWire Wire;
extern TwoWire Wire1;
```

- [ ] **Step 4: Escrever `hal/esp_sleep.h` e `hal/driver/gpio.h`**

```cpp
// hal/esp_sleep.h
#pragma once
#include <cstdint>
typedef int esp_err_t;
#define ESP_OK 0
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED = 0, ESP_SLEEP_WAKEUP_TIMER = 4,
               ESP_SLEEP_WAKEUP_GPIO = 7 } esp_sleep_wakeup_cause_t;

esp_err_t esp_light_sleep_start();
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us);
esp_err_t esp_sleep_enable_gpio_wakeup();
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause();
```

```cpp
// hal/driver/gpio.h
#pragma once
#include <cstdint>
typedef int gpio_num_t;
typedef enum { GPIO_INTR_LOW_LEVEL = 4, GPIO_INTR_HIGH_LEVEL = 5 } gpio_int_type_t;
inline int gpio_wakeup_enable(gpio_num_t, gpio_int_type_t) { return 0; }
inline int gpio_wakeup_disable(gpio_num_t) { return 0; }
inline int gpio_hold_en(gpio_num_t) { return 0; }
inline int gpio_hold_dis(gpio_num_t) { return 0; }
```

Se o Step 1 mostrou outra função `esp_*`/`gpio_*`, adicione-a aqui como no-op.

- [ ] **Step 5: Escrever `hal/SensorPCF85063.hpp` e `hal/XPowersLib.h`**

```cpp
// hal/SensorPCF85063.hpp
#pragma once
#include <cstdint>
#include <ctime>

struct RTC_DateTime {
  uint16_t year = 1970; uint8_t month = 1, day = 1;
  uint8_t hour = 0, minute = 0, second = 0;
};

class SensorPCF85063 {
 public:
  bool begin(class TwoWire& w, int sda = -1, int scl = -1) { (void)w; (void)sda; (void)scl; return true; }
  RTC_DateTime getDateTime();
  void setDateTime(const RTC_DateTime& dt);
  uint32_t getEpoch();          // segundos; usa time(nullptr) + offset
  void setEpoch(uint32_t e);    // grava offset em memoria; nao mexe no relogio do Windows
  void disableAlarm() {}
  void clearAlarmFlag() {}
};
```

```cpp
// hal/XPowersLib.h
#pragma once
#include <cstdint>

class XPowersAXP2101 {
 public:
  bool begin(class TwoWire& w, uint8_t addr = 0x34, int sda = -1, int scl = -1) {
    (void)w; (void)addr; (void)sda; (void)scl; return true;
  }
  int  getBatteryPercent() { return -1; }      // sem bateria
  bool isBatteryConnect()  { return false; }
  bool isVbusIn()          { return true; }    // sempre na tomada
  bool isCharging()        { return false; }
  uint16_t getBattVoltage(){ return 0; }
  void setSysPowerDownVoltage(uint16_t) {}
  void disableTSPinMeasure() {}
  void enableBattDetection() {}
  void enableVbusVoltageMeasure() {}
  void enableBattVoltageMeasure() {}
  void shutdown() {}
  void clearIrqStatus() {}
  uint32_t getIrqStatus() { return 0; }
  bool isPekeyShortPressIrq() { return false; }
};
```

Os nomes de método **devem** ser os que o Step 1 listou. Se o upstream chama `pmu.getBatteryPercent()` com outro nome, use o nome real — este arquivo existe só para satisfazer o upstream.

- [ ] **Step 6: Escrever `hal/stubs.cpp`**

```cpp
#include "Wire.h"
#include "esp_sleep.h"
#include "SensorPCF85063.hpp"
#include "hal_control.h"
#include <atomic>
#include <ctime>

TwoWire Wire(0);
TwoWire Wire1(1);

int64_t halRtcOffsetSeconds();   // definido em hal/arduino.cpp

namespace {
std::atomic<bool> gPwrShort{false};
int64_t gUserSetOffset = 0;      // de rtcSetEpoch
}  // namespace

void halPwrShortPress() { gPwrShort.store(true); }
bool halConsumePwrShortPress() { return gPwrShort.exchange(false); }

uint32_t SensorPCF85063::getEpoch() {
  return (uint32_t)((int64_t)std::time(nullptr) + halRtcOffsetSeconds() + gUserSetOffset);
}
void SensorPCF85063::setEpoch(uint32_t e) {
  gUserSetOffset = (int64_t)e - (int64_t)std::time(nullptr) - halRtcOffsetSeconds();
}
RTC_DateTime SensorPCF85063::getDateTime() {
  std::time_t t = (std::time_t)getEpoch();
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  RTC_DateTime d;
  d.year = (uint16_t)(tm.tm_year + 1900); d.month = (uint8_t)(tm.tm_mon + 1);
  d.day = (uint8_t)tm.tm_mday; d.hour = (uint8_t)tm.tm_hour;
  d.minute = (uint8_t)tm.tm_min; d.second = (uint8_t)tm.tm_sec;
  return d;
}
void SensorPCF85063::setDateTime(const RTC_DateTime& dt) {
  std::tm tm{};
  tm.tm_year = dt.year - 1900; tm.tm_mon = dt.month - 1; tm.tm_mday = dt.day;
  tm.tm_hour = dt.hour; tm.tm_min = dt.minute; tm.tm_sec = dt.second;
  setEpoch((uint32_t)std::mktime(&tm));
}

esp_err_t esp_light_sleep_start() { halIdleWait(50); return ESP_OK; }
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t) { return ESP_OK; }
esp_err_t esp_sleep_enable_gpio_wakeup() { return ESP_OK; }
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_GPIO; }
```

`halConsumePwrShortPress()` é o que o `pwrShortPressed()` do upstream acaba lendo — se o upstream lê a flag via `pmu.isPekeyShortPressIrq()`, faça esse método retornar `halConsumePwrShortPress()` em vez de `false`. Declare `halConsumePwrShortPress()` em `hal/hal_control.h` junto de `halPwrShortPress()`.

- [ ] **Step 7: Adicionar `upstream/rtcbat.cpp` ao alvo de testes**

```cmake
target_sources(tamapoke_tests PRIVATE upstream/rtcbat.cpp)
```

- [ ] **Step 8: Rodar e ver passar**

Run:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target tamapoke_tests
ctest --test-dir build -C Debug -R tamapoke --output-on-failure
```
Expected: PASS nos 4 testes de `hal_rtc`. Isto prova que `rtcbat.cpp` compila contra a HAL sem patch.

- [ ] **Step 9: Commit**

```bash
git add hal/Wire.h hal/esp_sleep.h hal/driver/gpio.h hal/SensorPCF85063.hpp \
        hal/XPowersLib.h hal/stubs.cpp hal/hal_control.h tests/test_hal_rtc.cpp CMakeLists.txt
git commit -m "feat(hal): stubs de RTC, PMU, I2C e sleep"
```

---

## Fase 2: tela

### Task 10 (spike, máximo 1 dia): compilar o núcleo real do Arduino_GFX

Usar o código original garante desenho e texto pixel a pixel idênticos, inclusive `getTextBounds` e as fontes U8g2. Esta tarefa decide entre a Task 11A e a Task 11B.

**Files:**
- Create: `third_party/Arduino_GFX` (submódulo), `hal/Print.h`, `docs/decisions/0001-canvas.md`

- [ ] **Step 1: Descobrir a versão que o CI do upstream usa**

Run:
```bash
grep -rniE 'arduino_gfx|GFX_Library|moononournation' upstream/.github upstream/*.md 2>/dev/null
```
Expected: a versão ou o nome da lib. Anote. Se não houver pista, use a tag mais recente e registre a escolha.

- [ ] **Step 2: Adicionar o submódulo**

```bash
git submodule add https://github.com/moononournation/Arduino_GFX third_party/Arduino_GFX
cd third_party/Arduino_GFX && git checkout <tag-anotada> && cd ../..
```

- [ ] **Step 3: Escrever `hal/Print.h`**

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdarg>

// Print do Arduino: o Arduino_GFX herda disso para text rendering.
class Print {
 public:
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* buf, size_t n) {
    size_t w = 0;
    for (size_t i = 0; i < n; ++i) w += write(buf[i]);
    return w;
  }
  size_t print(const char* s) {
    size_t w = 0;
    for (const char* p = s; p && *p; ++p) w += write((uint8_t)*p);
    return w;
  }
  size_t print(int v) { char b[32]; int n = std::snprintf(b, sizeof b, "%d", v); return write((const uint8_t*)b, (size_t)n); }
  size_t println(const char* s = "") { size_t w = print(s); w += write((uint8_t)'\n'); return w; }
  int printf(const char* fmt, ...) {
    char b[512]; va_list ap; va_start(ap, fmt);
    int n = std::vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    if (n > 0) write((const uint8_t*)b, (size_t)n);
    return n;
  }
  virtual ~Print() = default;
};
```

- [ ] **Step 4: Tentar compilar só o núcleo**

Crie um alvo de spike temporário no `CMakeLists.txt`:

```cmake
add_library(gfx_spike STATIC
  third_party/Arduino_GFX/src/Arduino_GFX.cpp
  third_party/Arduino_GFX/src/Arduino_G.cpp
  third_party/Arduino_GFX/src/canvas/Arduino_Canvas.cpp)
target_include_directories(gfx_spike PRIVATE hal third_party/Arduino_GFX/src)
```

Run: `cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Debug --target gfx_spike`

Conte quantos defines/stubs novos foram necessários para chegar ao fim.

- [ ] **Step 5: Aplicar o critério de decisão**

- **Compilou com até ~10 defines/stubs** → siga a **Task 11A**, mantenha o submódulo e o alvo (renomeie `gfx_spike` para `gfx_core`).
- **Depende de `esp_lcd`, DMA, ou tem `#ifdef ESP32` espalhado pelo núcleo** → siga a **Task 11B**, remova o submódulo (`git submodule deinit -f third_party/Arduino_GFX && git rm -f third_party/Arduino_GFX`) e o alvo.

- [ ] **Step 6: Registrar a decisão**

```markdown
<!-- docs/decisions/0001-canvas.md -->
# 0001 — Origem do canvas de desenho

**Data:** <data>
**Decisão:** <Arduino_GFX real | canvas próprio>

## Contexto
O upstream desenha com Arduino_GFX (`Arduino_ESP32QSPI` + `Arduino_CO5300` + `Arduino_Canvas`).
Reusar o núcleo original dá paridade pixel a pixel; reimplementar dá controle, com risco de divergência.

## O que o spike mostrou
<lista de erros encontrados, defines/stubs necessários, veredito>

## Consequência
Seguimos a Task <11A|11B>. Fontes CJK na Fase 5 <reusam o decodificador do Arduino_GFX | exigem portar u8g2_font_decode>.
```

- [ ] **Step 7: Commit**

```bash
git add docs/decisions/0001-canvas.md hal/Print.h CMakeLists.txt .gitmodules third_party 2>/dev/null
git commit -m "spike: decidir a origem do canvas (ver docs/decisions/0001-canvas.md)"
```

### Task 11A: Painel falso para o Arduino_Canvas real

**Só execute se a Task 10 decidiu "Arduino_GFX real". Caso contrário, pule para a Task 11B.**

**Files:**
- Create: `hal/Arduino_GFX_Library.h`, `hal/panel.cpp`
- Test: `tests/test_canvas.cpp`

- [ ] **Step 1: Escrever o teste falhando**

```cpp
// tests/test_canvas.cpp
#include "framework.h"
#include "Arduino_GFX_Library.h"
#include "hal_control.h"

TEST(canvas, fillRect_chega_no_framebuffer_depois_do_flush) {
  Arduino_DataBus* bus = new Arduino_ESP32QSPI(0, 0, 0, 0, 0, 0);
  Arduino_GFX* panel = new Arduino_CO5300(bus, -1, 0, false, 466, 466, 6, 0, 0, 0);
  Arduino_Canvas* gfx = new Arduino_Canvas(466, 466, panel);
  ASSERT_TRUE(gfx->begin());
  gfx->fillScreen(0x0000);
  gfx->fillRect(10, 10, 4, 4, 0xF800);
  gfx->flush();
  ASSERT_EQ((int)halFrame()[10 * 466 + 10], (int)0xF800);
  ASSERT_EQ((int)halFrame()[9 * 466 + 10], 0);
  ASSERT_TRUE(halFrameDirty());
  halClearFrameDirty();
  ASSERT_TRUE(!halFrameDirty());
}

TEST(canvas, brilho_do_painel_chega_na_hal) {
  Arduino_DataBus* bus = new Arduino_ESP32QSPI(0, 0, 0, 0, 0, 0);
  Arduino_CO5300* panel = new Arduino_CO5300(bus, -1, 0, false, 466, 466, 6, 0, 0, 0);
  panel->setBrightness(90);
  ASSERT_EQ((int)halBrightness(), 90);
}
```

As assinaturas de construtor acima vêm do `TamaPoke.ino`. Confirme as reais:
```bash
grep -nE 'Arduino_ESP32QSPI|Arduino_CO5300|Arduino_Canvas' upstream/TamaPoke.ino
```
e copie **exatamente** a chamada do upstream para o teste — é ela que o painel falso precisa aceitar.

- [ ] **Step 2: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `Arduino_GFX_Library.h` não existe.

- [ ] **Step 3: Escrever `hal/Arduino_GFX_Library.h`**

```cpp
#pragma once
// Inclui o nucleo real do Arduino_GFX e substitui apenas o barramento e o painel.
#include "Arduino_GFX.h"
#include "Arduino_G.h"
#include "Arduino_DataBus.h"
#include "canvas/Arduino_Canvas.h"

// Barramento falso: aceita a mesma chamada do upstream e nao faz nada.
class Arduino_ESP32QSPI : public Arduino_DataBus {
 public:
  Arduino_ESP32QSPI(int8_t cs, int8_t sck, int8_t d0, int8_t d1, int8_t d2, int8_t d3)
      { (void)cs; (void)sck; (void)d0; (void)d1; (void)d2; (void)d3; }
  bool begin(int32_t = 0, int8_t = -1) override { return true; }
  void beginWrite() override {}
  void endWrite() override {}
  void writeCommand(uint8_t) override {}
  void writeCommand16(uint16_t) override {}
  void write(uint8_t) override {}
  void write16(uint16_t) override {}
  void writeRepeat(uint16_t, uint32_t) override {}
  void writeBytes(uint8_t*, uint32_t) override {}
  void writePixels(uint16_t*, uint32_t) override {}
};

// Painel falso: recebe os blits do Arduino_Canvas e copia para o framebuffer da HAL.
class Arduino_CO5300 : public Arduino_GFX {
 public:
  Arduino_CO5300(Arduino_DataBus* bus, int8_t rst, uint8_t rotation, bool ips,
                 int16_t w, int16_t h, uint8_t colOff1, uint8_t rowOff1,
                 uint8_t colOff2, uint8_t rowOff2)
      : Arduino_GFX(w, h) {
    (void)bus; (void)rst; (void)rotation; (void)ips;
    (void)colOff1; (void)rowOff1; (void)colOff2; (void)rowOff2;
  }
  bool begin(int32_t = 0) override { return true; }
  void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override;
  void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h) override;
  void draw16bitBeRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h) override;
  void setBrightness(uint8_t b);
  void displayOn() {}
  void displayOff() {}
  void setRotation(uint8_t) {}
};
```

Se o `Arduino_GFX` real exigir sobrescrever outro método virtual puro, o compilador dirá qual — adicione-o com o corpo mínimo. Se o upstream chamar outro método do painel (`grep -oE 'panel->[a-zA-Z_0-9]+' upstream/TamaPoke.ino | sort -u`), adicione-o aqui.

- [ ] **Step 4: Escrever `hal/panel.cpp`**

```cpp
#include "Arduino_GFX_Library.h"
#include "hal_control.h"
#include <mutex>
#include <vector>
#include <atomic>
#include <algorithm>

namespace {
std::mutex gMu;
std::vector<uint16_t> gFb(HAL_SCREEN_W * HAL_SCREEN_H, 0);
std::atomic<bool> gDirty{false};
std::atomic<uint8_t> gBrightness{255};
}  // namespace

const uint16_t* halFrame() { return gFb.data(); }
bool halFrameDirty() { return gDirty.load(); }
void halClearFrameDirty() { gDirty.store(false); }
uint8_t halBrightness() { return gBrightness.load(); }

void Arduino_CO5300::setBrightness(uint8_t b) { gBrightness.store(b); }

void Arduino_CO5300::writePixelPreclipped(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= HAL_SCREEN_W || y >= HAL_SCREEN_H) return;
  std::lock_guard<std::mutex> g(gMu);
  gFb[(size_t)y * HAL_SCREEN_W + (size_t)x] = color;
  gDirty.store(true);
}

void Arduino_CO5300::draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap,
                                        int16_t w, int16_t h) {
  if (!bitmap) return;
  std::lock_guard<std::mutex> g(gMu);
  for (int16_t row = 0; row < h; ++row) {
    int16_t dy = y + row;
    if (dy < 0 || dy >= HAL_SCREEN_H) continue;
    int16_t x0 = std::max<int16_t>(0, x);
    int16_t x1 = std::min<int16_t>(HAL_SCREEN_W, (int16_t)(x + w));
    if (x1 <= x0) continue;
    std::copy(bitmap + (size_t)row * w + (x0 - x),
              bitmap + (size_t)row * w + (x1 - x),
              gFb.begin() + (size_t)dy * HAL_SCREEN_W + x0);
  }
  gDirty.store(true);
}

void Arduino_CO5300::draw16bitBeRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap,
                                          int16_t w, int16_t h) {
  // O canvas do upstream usa little-endian; se este caminho for usado, converte.
  if (!bitmap) return;
  std::lock_guard<std::mutex> g(gMu);
  for (int16_t row = 0; row < h; ++row) {
    int16_t dy = y + row;
    if (dy < 0 || dy >= HAL_SCREEN_H) continue;
    for (int16_t col = 0; col < w; ++col) {
      int16_t dx = x + col;
      if (dx < 0 || dx >= HAL_SCREEN_W) continue;
      uint16_t v = bitmap[(size_t)row * w + col];
      gFb[(size_t)dy * HAL_SCREEN_W + dx] = (uint16_t)((v >> 8) | (v << 8));
    }
  }
  gDirty.store(true);
}
```

- [ ] **Step 5: Ligar o núcleo do GFX ao alvo de testes**

```cmake
target_link_libraries(tamapoke_tests PRIVATE gfx_core)
target_include_directories(tamapoke_tests PRIVATE third_party/Arduino_GFX/src)
```

- [ ] **Step 6: Rodar e ver passar**

Run:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target tamapoke_tests
ctest --test-dir build -C Debug -R tamapoke --output-on-failure
```
Expected: PASS nos 2 testes de `canvas`.

- [ ] **Step 7: Commit**

```bash
git add hal/Arduino_GFX_Library.h hal/panel.cpp tests/test_canvas.cpp CMakeLists.txt
git commit -m "feat(hal): painel falso sobre o Arduino_Canvas real"
```

### Task 11B (alternativa): Canvas próprio compatível

> **NÃO EXECUTAR.** A Task 10 decidiu pelo Caminho A (Arduino_GFX real, v1.6.9) — veja `docs/decisions/0001-canvas.md`. Esta tarefa é mantida só como registro da alternativa descartada.

**Só execute se a Task 10 decidiu "canvas próprio". Caso contrário, pule.**

**Files:**
- Create: `hal/Arduino_GFX_Library.h`, `hal/canvas.cpp`, `hal/glcdfont.h`
- Test: `tests/test_canvas.cpp`

- [ ] **Step 1: Levantar todas as primitivas usadas**

Run:
```bash
grep -ohE 'gfx->[a-zA-Z_0-9]+' upstream/TamaPoke.ino upstream/*.cpp | sort -u
grep -ohE 'panel->[a-zA-Z_0-9]+' upstream/TamaPoke.ino | sort -u
```
Expected: a lista fechada. Cada primitiva da lista precisa de um teste e de uma implementação.

- [ ] **Step 2: Copiar a fonte 5×7 clássica**

Copie o array `glcdfont` de `third_party/Arduino_GFX/src/font/glcdfont.c` (ou do Adafruit_GFX — licença BSD) para `hal/glcdfont.h` como `static const uint8_t kGlcdFont[] PROGMEM = {...}`. Mantenha o cabeçalho de licença BSD no topo do arquivo.

- [ ] **Step 3: Escrever o teste falhando (uma referência por primitiva)**

```cpp
// tests/test_canvas.cpp
#include "framework.h"
#include "Arduino_GFX_Library.h"
#include "hal_control.h"

static Arduino_Canvas* novoCanvas() {
  static Arduino_Canvas canvas(466, 466, nullptr);
  canvas.begin();
  canvas.fillScreen(0x0000);
  return &canvas;
}
static uint16_t px(int x, int y) { return halFrame()[(size_t)y * 466 + x]; }

TEST(canvas, fillRect) {
  auto* g = novoCanvas();
  g->fillRect(10, 10, 4, 4, 0xF800);
  g->flush();
  ASSERT_EQ((int)px(10, 10), (int)0xF800);
  ASSERT_EQ((int)px(13, 13), (int)0xF800);
  ASSERT_EQ((int)px(14, 13), 0);
  ASSERT_EQ((int)px(9, 10), 0);
}

TEST(canvas, drawRect_so_a_borda) {
  auto* g = novoCanvas();
  g->drawRect(5, 5, 5, 5, 0x07E0);
  g->flush();
  ASSERT_EQ((int)px(5, 5), (int)0x07E0);
  ASSERT_EQ((int)px(9, 9), (int)0x07E0);
  ASSERT_EQ((int)px(7, 7), 0);            // meio vazio
}

TEST(canvas, drawFastHLine_e_VLine) {
  auto* g = novoCanvas();
  g->drawFastHLine(0, 20, 5, 0x001F);
  g->drawFastVLine(30, 0, 5, 0x001F);
  g->flush();
  ASSERT_EQ((int)px(4, 20), (int)0x001F);
  ASSERT_EQ((int)px(5, 20), 0);
  ASSERT_EQ((int)px(30, 4), (int)0x001F);
  ASSERT_EQ((int)px(30, 5), 0);
}

TEST(canvas, fillCircle_bresenham_do_adafruit) {
  auto* g = novoCanvas();
  g->fillCircle(5, 5, 3, 0xFFFF);
  g->flush();
  ASSERT_EQ((int)px(5, 5), (int)0xFFFF);   // centro
  ASSERT_EQ((int)px(2, 5), (int)0xFFFF);   // extremo esquerdo (r=3)
  ASSERT_EQ((int)px(8, 5), (int)0xFFFF);   // extremo direito
  ASSERT_EQ((int)px(5, 2), (int)0xFFFF);   // topo
  ASSERT_EQ((int)px(2, 2), 0);             // canto fora do circulo
}

TEST(canvas, drawCircle_so_a_borda) {
  auto* g = novoCanvas();
  g->drawCircle(10, 10, 3, 0xFFFF);
  g->flush();
  ASSERT_EQ((int)px(7, 10), (int)0xFFFF);
  ASSERT_EQ((int)px(10, 10), 0);           // centro vazio
}

TEST(canvas, texto_classico_avanca_6px_por_char) {
  auto* g = novoCanvas();
  g->setTextSize(1);
  g->setTextColor(0xFFFF);
  g->setCursor(0, 0);
  g->print("AB");
  g->flush();
  int16_t x1, y1; uint16_t w, h;
  g->getTextBounds("AB", 0, 0, &x1, &y1, &w, &h);
  ASSERT_EQ((int)w, 12);                   // 2 chars * 6 px
  ASSERT_EQ((int)h, 8);
}

TEST(canvas, draw16bitRGBBitmap_copia_o_sprite) {
  auto* g = novoCanvas();
  uint16_t sprite[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
  g->draw16bitRGBBitmap(100, 100, sprite, 2, 2);
  g->flush();
  ASSERT_EQ((int)px(100, 100), (int)0xF800);
  ASSERT_EQ((int)px(101, 101), (int)0xFFFF);
}
```

Acrescente um teste por primitiva que o Step 1 listou e que não esteja coberta acima — sem exceção. Uma primitiva sem teste é uma primitiva que vai divergir do upstream em silêncio.

- [ ] **Step 4: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `Arduino_GFX_Library.h` não existe.

- [ ] **Step 5: Escrever `hal/Arduino_GFX_Library.h`**

```cpp
#pragma once
#include <cstdint>
#include "Print.h"

// Canvas proprio, compativel com a superficie que o upstream usa.
// Algoritmos portados do Adafruit_GFX (BSD) para manter os mesmos arredondamentos.
class Arduino_DataBus { public: virtual ~Arduino_DataBus() = default; };

class Arduino_ESP32QSPI : public Arduino_DataBus {
 public:
  Arduino_ESP32QSPI(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) {}
  bool begin(int32_t = 0, int8_t = -1) { return true; }
};

class Arduino_GFX : public Print {
 public:
  Arduino_GFX(int16_t w, int16_t h) : w_(w), h_(h) {}
  int16_t width() const { return w_; }
  int16_t height() const { return h_; }

  virtual bool begin(int32_t = 0) { return true; }
  virtual void flush() {}
  virtual void writePixel(int16_t x, int16_t y, uint16_t color) = 0;

  void fillScreen(uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawPixel(int16_t x, int16_t y, uint16_t color) { writePixel(x, y, color); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void drawTriangle(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t);
  void fillTriangle(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t);
  virtual void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h);

  void setCursor(int16_t x, int16_t y) { cx_ = x; cy_ = y; }
  int16_t getCursorX() const { return cx_; }
  int16_t getCursorY() const { return cy_; }
  void setTextColor(uint16_t c) { fg_ = c; bg_ = c; opaqueBg_ = false; }
  void setTextColor(uint16_t c, uint16_t b) { fg_ = c; bg_ = b; opaqueBg_ = true; }
  void setTextSize(uint8_t s) { size_ = s ? s : 1; }
  void setTextWrap(bool w) { wrap_ = w; }
  void setFont(const void* f) { font_ = f; }       // U8g2: Fase 5
  void setUTF8Print(bool e) { utf8_ = e; }
  void getTextBounds(const char* s, int16_t x, int16_t y,
                     int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);
  size_t write(uint8_t c) override;
  void setBrightness(uint8_t b);
  void displayOn() {}
  void displayOff() {}

 protected:
  void drawChar5x7(int16_t x, int16_t y, uint8_t c);
  int16_t w_, h_;
  int16_t cx_ = 0, cy_ = 0;
  uint16_t fg_ = 0xFFFF, bg_ = 0x0000;
  bool opaqueBg_ = false, wrap_ = true, utf8_ = false;
  uint8_t size_ = 1;
  const void* font_ = nullptr;
};

// O "painel" e o "canvas" sao a mesma coisa aqui: tudo vai para o framebuffer da HAL.
class Arduino_CO5300 : public Arduino_GFX {
 public:
  Arduino_CO5300(Arduino_DataBus*, int8_t, uint8_t, bool, int16_t w, int16_t h,
                 uint8_t, uint8_t, uint8_t, uint8_t) : Arduino_GFX(w, h) {}
  void writePixel(int16_t x, int16_t y, uint16_t color) override;
};

class Arduino_Canvas : public Arduino_GFX {
 public:
  Arduino_Canvas(int16_t w, int16_t h, Arduino_GFX* output)
      : Arduino_GFX(w, h), out_(output) {}
  bool begin(int32_t = 0) override;
  void flush() override;
  void writePixel(int16_t x, int16_t y, uint16_t color) override;
  uint16_t* getFramebuffer();
 private:
  Arduino_GFX* out_;
};
```

Ajuste a lista de métodos para ser exatamente a do Step 1: nem a mais (YAGNI) nem a menos.

- [ ] **Step 6: Escrever `hal/canvas.cpp`**

Porte os algoritmos do Adafruit_GFX mantendo os mesmos arredondamentos. Núcleo do círculo e do texto, que são os que divergem com mais facilidade:

```cpp
#include "Arduino_GFX_Library.h"
#include "glcdfont.h"
#include "hal_control.h"
#include <mutex>
#include <vector>
#include <atomic>
#include <algorithm>
#include <cstring>

namespace {
std::mutex gMu;
std::vector<uint16_t> gBack(HAL_SCREEN_W * HAL_SCREEN_H, 0);  // canvas
std::vector<uint16_t> gFront(HAL_SCREEN_W * HAL_SCREEN_H, 0); // apresentado
std::atomic<bool> gDirty{false};
std::atomic<uint8_t> gBrightness{255};
}  // namespace

const uint16_t* halFrame() { return gFront.data(); }
bool halFrameDirty() { return gDirty.load(); }
void halClearFrameDirty() { gDirty.store(false); }
uint8_t halBrightness() { return gBrightness.load(); }
void Arduino_GFX::setBrightness(uint8_t b) { gBrightness.store(b); }

void Arduino_Canvas::writePixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= HAL_SCREEN_W || y >= HAL_SCREEN_H) return;
  gBack[(size_t)y * HAL_SCREEN_W + x] = color;
}
uint16_t* Arduino_Canvas::getFramebuffer() { return gBack.data(); }
bool Arduino_Canvas::begin(int32_t) { std::fill(gBack.begin(), gBack.end(), 0); return true; }
void Arduino_Canvas::flush() {
  std::lock_guard<std::mutex> g(gMu);
  gFront = gBack;
  gDirty.store(true);
}
void Arduino_CO5300::writePixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= HAL_SCREEN_W || y >= HAL_SCREEN_H) return;
  std::lock_guard<std::mutex> g(gMu);
  gFront[(size_t)y * HAL_SCREEN_W + x] = color;
  gDirty.store(true);
}

void Arduino_GFX::fillScreen(uint16_t c) { fillRect(0, 0, w_, h_, c); }

void Arduino_GFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  for (int16_t i = 0; i < h; ++i) drawFastHLine(x, (int16_t)(y + i), w, c);
}
void Arduino_GFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  drawFastHLine(x, y, w, c);
  drawFastHLine(x, (int16_t)(y + h - 1), w, c);
  drawFastVLine(x, y, h, c);
  drawFastVLine((int16_t)(x + w - 1), y, h, c);
}
void Arduino_GFX::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) {
  for (int16_t i = 0; i < w; ++i) writePixel((int16_t)(x + i), y, c);
}
void Arduino_GFX::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) {
  for (int16_t i = 0; i < h; ++i) writePixel(x, (int16_t)(y + i), c);
}

// Bresenham do Adafruit_GFX, igual ao original.
void Arduino_GFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) {
  bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
  if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
  if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
  int16_t dx = (int16_t)(x1 - x0), dy = (int16_t)std::abs(y1 - y0);
  int16_t err = (int16_t)(dx / 2), ystep = (y0 < y1) ? 1 : -1;
  for (; x0 <= x1; ++x0) {
    if (steep) writePixel(y0, x0, c); else writePixel(x0, y0, c);
    err -= dy;
    if (err < 0) { y0 = (int16_t)(y0 + ystep); err = (int16_t)(err + dx); }
  }
}

void Arduino_GFX::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t c) {
  int16_t f = (int16_t)(1 - r), ddFx = 1, ddFy = (int16_t)(-2 * r), x = 0, y = r;
  writePixel(x0, (int16_t)(y0 + r), c);
  writePixel(x0, (int16_t)(y0 - r), c);
  writePixel((int16_t)(x0 + r), y0, c);
  writePixel((int16_t)(x0 - r), y0, c);
  while (x < y) {
    if (f >= 0) { --y; ddFy = (int16_t)(ddFy + 2); f = (int16_t)(f + ddFy); }
    ++x; ddFx = (int16_t)(ddFx + 2); f = (int16_t)(f + ddFx);
    writePixel((int16_t)(x0 + x), (int16_t)(y0 + y), c);
    writePixel((int16_t)(x0 - x), (int16_t)(y0 + y), c);
    writePixel((int16_t)(x0 + x), (int16_t)(y0 - y), c);
    writePixel((int16_t)(x0 - x), (int16_t)(y0 - y), c);
    writePixel((int16_t)(x0 + y), (int16_t)(y0 + x), c);
    writePixel((int16_t)(x0 - y), (int16_t)(y0 + x), c);
    writePixel((int16_t)(x0 + y), (int16_t)(y0 - x), c);
    writePixel((int16_t)(x0 - y), (int16_t)(y0 - x), c);
  }
}

void Arduino_GFX::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t c) {
  drawFastVLine(x0, (int16_t)(y0 - r), (int16_t)(2 * r + 1), c);
  int16_t f = (int16_t)(1 - r), ddFx = 1, ddFy = (int16_t)(-2 * r), x = 0, y = r;
  while (x < y) {
    if (f >= 0) { --y; ddFy = (int16_t)(ddFy + 2); f = (int16_t)(f + ddFy); }
    ++x; ddFx = (int16_t)(ddFx + 2); f = (int16_t)(f + ddFx);
    drawFastVLine((int16_t)(x0 + x), (int16_t)(y0 - y), (int16_t)(2 * y + 1), c);
    drawFastVLine((int16_t)(x0 - x), (int16_t)(y0 - y), (int16_t)(2 * y + 1), c);
    drawFastVLine((int16_t)(x0 + y), (int16_t)(y0 - x), (int16_t)(2 * x + 1), c);
    drawFastVLine((int16_t)(x0 - y), (int16_t)(y0 - x), (int16_t)(2 * x + 1), c);
  }
}

void Arduino_GFX::draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t* bmp,
                                     int16_t w, int16_t h) {
  if (!bmp) return;
  for (int16_t row = 0; row < h; ++row)
    for (int16_t col = 0; col < w; ++col)
      writePixel((int16_t)(x + col), (int16_t)(y + row), bmp[(size_t)row * w + col]);
}

void Arduino_GFX::drawChar5x7(int16_t x, int16_t y, uint8_t ch) {
  for (int8_t col = 0; col < 5; ++col) {
    uint8_t bits = kGlcdFont[(size_t)ch * 5 + col];
    for (int8_t row = 0; row < 8; ++row, bits >>= 1) {
      uint16_t color = (bits & 1) ? fg_ : bg_;
      if (!(bits & 1) && !opaqueBg_) continue;
      if (size_ == 1) writePixel((int16_t)(x + col), (int16_t)(y + row), color);
      else fillRect((int16_t)(x + col * size_), (int16_t)(y + row * size_),
                    size_, size_, color);
    }
  }
}

size_t Arduino_GFX::write(uint8_t c) {
  if (c == '\n') { cx_ = 0; cy_ = (int16_t)(cy_ + 8 * size_); return 1; }
  if (c == '\r') return 1;
  if (wrap_ && cx_ + 6 * size_ > w_) { cx_ = 0; cy_ = (int16_t)(cy_ + 8 * size_); }
  drawChar5x7(cx_, cy_, c);
  cx_ = (int16_t)(cx_ + 6 * size_);
  return 1;
}

void Arduino_GFX::getTextBounds(const char* s, int16_t x, int16_t y,
                                int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
  int16_t lineW = 0, maxW = 0, lines = 1;
  for (const char* p = s; p && *p; ++p) {
    if (*p == '\n') { maxW = std::max(maxW, lineW); lineW = 0; ++lines; continue; }
    if (*p == '\r') continue;
    lineW = (int16_t)(lineW + 6 * size_);
  }
  maxW = std::max(maxW, lineW);
  if (x1) *x1 = x;
  if (y1) *y1 = y;
  if (w) *w = (uint16_t)maxW;
  if (h) *h = (uint16_t)(8 * size_ * lines);
}
```

Implemente também `drawRoundRect`, `fillRoundRect`, `drawTriangle` e `fillTriangle` portando o Adafruit_GFX, **cada um com seu teste** no Step 3. As fontes CJK (`setFont` com fonte U8g2) ficam para a Fase 5; até lá, `setFont(nullptr)` é o único caminho e `setFont` de uma fonte U8g2 cai no 5×7.

- [ ] **Step 7: Rodar e ver passar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS em todos os testes de `canvas`.

- [ ] **Step 8: Commit**

```bash
git add hal/Arduino_GFX_Library.h hal/canvas.cpp hal/glcdfont.h tests/test_canvas.cpp
git commit -m "feat(hal): canvas proprio compativel com o Arduino_GFX"
```

### Task 12: Apresentar o framebuffer no SDL

**Files:**
- Create: `src/present.h`, `src/present.cpp`
- Test: `tests/test_present.cpp`

- [ ] **Step 1: Escrever o teste falhando**

A máscara circular é lógica pura e testável sem janela; o resto é SDL e vai para o checklist manual.

```cpp
// tests/test_present.cpp
#include "framework.h"
#include "present.h"

TEST(present, mascara_circular_marca_dentro_e_fora) {
  ASSERT_TRUE(presentInsideCircle(233, 233));   // centro
  ASSERT_TRUE(presentInsideCircle(1, 233));     // borda esquerda
  ASSERT_TRUE(!presentInsideCircle(0, 0));      // canto
  ASSERT_TRUE(!presentInsideCircle(465, 0));
  ASSERT_TRUE(!presentInsideCircle(465, 465));
}

TEST(present, escala_converte_pixel_de_janela_em_coord_logica) {
  ASSERT_EQ(presentToLogical(100, 2.0f), 50);
  ASSERT_EQ(presentToLogical(100, 1.0f), 100);
  ASSERT_EQ(presentToLogical(100, 0.5f), 200);
  ASSERT_EQ(presentToLogical(-5, 1.0f), 0);     // clamp
  ASSERT_EQ(presentToLogical(1000, 1.0f), 465); // clamp
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `present.h` não existe.

- [ ] **Step 3: Escrever `src/present.h`**

```cpp
#pragma once
#include <cstdint>
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

// Funcoes puras, testaveis sem janela.
bool presentInsideCircle(int x, int y);
int  presentToLogical(int windowPixel, float scale);

class Presenter {
 public:
  bool init(SDL_Window* window);
  void shutdown();
  // Desenha o framebuffer da HAL. brightness 0..255; circular aplica alfa 0 fora.
  void draw(const uint16_t* frame, uint8_t brightness, bool circular);
  void setScale(float scale);
  float scale() const { return scale_; }

 private:
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* tex_ = nullptr;
  float scale_ = 1.0f;
};
```

- [ ] **Step 4: Escrever `src/present.cpp`**

```cpp
#include "present.h"
#include "hal_control.h"
#include <SDL3/SDL.h>
#include <vector>

bool presentInsideCircle(int x, int y) {
  const int r = HAL_SCREEN_W / 2;            // 233
  int dx = x - r, dy = y - r;
  return dx * dx + dy * dy <= r * r;
}

int presentToLogical(int windowPixel, float scale) {
  if (scale <= 0.0f) scale = 1.0f;
  int v = (int)((float)windowPixel / scale);
  if (v < 0) v = 0;
  if (v > HAL_SCREEN_W - 1) v = HAL_SCREEN_W - 1;
  return v;
}

bool Presenter::init(SDL_Window* window) {
  ren_ = SDL_CreateRenderer(window, nullptr);
  if (!ren_) return false;
  SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_NONE);
  tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ARGB8888,
                           SDL_TEXTUREACCESS_STREAMING,
                           HAL_SCREEN_W, HAL_SCREEN_H);
  if (!tex_) return false;
  SDL_SetTextureScaleMode(tex_, SDL_SCALEMODE_NEAREST);
  SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
  return true;
}

void Presenter::shutdown() {
  if (tex_) { SDL_DestroyTexture(tex_); tex_ = nullptr; }
  if (ren_) { SDL_DestroyRenderer(ren_); ren_ = nullptr; }
}

void Presenter::setScale(float s) { scale_ = s > 0.0f ? s : 1.0f; }

void Presenter::draw(const uint16_t* frame, uint8_t brightness, bool circular) {
  if (!ren_ || !tex_ || !frame) return;

  // RGB565 -> ARGB8888, aplicando a mascara circular no canal alfa.
  // ARGB8888 em vez de RGB565 porque a silhueta redonda precisa de alfa por pixel.
  static std::vector<uint32_t> buf((size_t)HAL_SCREEN_W * HAL_SCREEN_H);
  for (int y = 0; y < HAL_SCREEN_H; ++y) {
    for (int x = 0; x < HAL_SCREEN_W; ++x) {
      size_t i = (size_t)y * HAL_SCREEN_W + x;
      if (circular && !presentInsideCircle(x, y)) { buf[i] = 0x00000000u; continue; }
      uint16_t p = frame[i];
      uint32_t r = (uint32_t)((p >> 11) & 0x1F) * 255u / 31u;
      uint32_t g = (uint32_t)((p >> 5) & 0x3F) * 255u / 63u;
      uint32_t b = (uint32_t)(p & 0x1F) * 255u / 31u;
      buf[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
  }
  SDL_UpdateTexture(tex_, nullptr, buf.data(), HAL_SCREEN_W * (int)sizeof(uint32_t));
  SDL_SetTextureColorMod(tex_, brightness, brightness, brightness);

  SDL_SetRenderDrawColor(ren_, 0, 0, 0, 0);
  SDL_RenderClear(ren_);
  SDL_RenderTexture(ren_, tex_, nullptr, nullptr);
  SDL_RenderPresent(ren_);
}
```

Nota de design: a textura é ARGB8888, não RGB565, porque a silhueta redonda exige alfa por pixel — RGB565 não tem canal alfa. O custo é uma conversão de 217 KB por frame, irrelevante a 10 fps, e só acontece quando há frame novo.

- [ ] **Step 5: Adicionar `src/present.cpp` ao alvo de testes**

```cmake
target_sources(tamapoke_tests PRIVATE src/present.cpp)
```

- [ ] **Step 6: Rodar e ver passar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 2 testes de `present`.

- [ ] **Step 7: Commit**

```bash
git add src/present.h src/present.cpp tests/test_present.cpp CMakeLists.txt
git commit -m "feat: apresentar o framebuffer com escala, brilho e mascara circular"
```

---

## Fase 3: o jogo rodando

### Task 13: Toque vindo do mouse

O upstream só consulta o chip de toque se `gTouchIrq` foi setado pela ISR registrada em `attachInterrupt`, ou se o dedo ainda está pressionado; depois chama `touch.getPoint(&x, &y, 1)`.

**Files:**
- Create: `hal/TouchDrvCSTXXX.hpp`, `hal/touch.cpp`
- Test: `tests/test_touch.cpp`

- [ ] **Step 1: Ler o uso real no upstream**

Run:
```bash
grep -nE 'touch\.|gTouchIrq|attachInterrupt|getPoint|TouchDrv' upstream/TamaPoke.ino
```
Expected: o pino de interrupção, o modo (`FALLING`/`RISING`) e a assinatura de `getPoint`. Os passos abaixo assumem `getPoint(int16_t* x, int16_t* y, uint8_t n)` retornando a contagem de pontos.

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_touch.cpp
#include "framework.h"
#include "Arduino.h"
#include "TouchDrvCSTXXX.hpp"
#include "hal_control.h"

static volatile bool gIsrChamada = false;
static void IRAM_ATTR isrDeTeste() { gIsrChamada = true; }

TEST(touch, down_dispara_a_isr_e_getPoint_devolve_o_ponto) {
  TouchDrvCST92xx touch;
  ASSERT_TRUE(touch.begin(Wire, 0x5A, -1, -1));
  const int pinoIrq = halTouchIrqPinForTest();
  attachInterrupt(digitalPinToInterrupt(pinoIrq), isrDeTeste, FALLING);

  gIsrChamada = false;
  halTouchDown(100, 200);
  ASSERT_TRUE(gIsrChamada);

  int16_t x = 0, y = 0;
  ASSERT_EQ((int)touch.getPoint(&x, &y, 1), 1);
  ASSERT_EQ((int)x, 100);
  ASSERT_EQ((int)y, 200);
}

TEST(touch, move_atualiza_sem_soltar) {
  TouchDrvCST92xx touch;
  touch.begin(Wire, 0x5A, -1, -1);
  halTouchDown(10, 10);
  halTouchMove(50, 60);
  int16_t x = 0, y = 0;
  ASSERT_EQ((int)touch.getPoint(&x, &y, 1), 1);
  ASSERT_EQ((int)x, 50);
  ASSERT_EQ((int)y, 60);
}

TEST(touch, up_faz_getPoint_devolver_zero) {
  TouchDrvCST92xx touch;
  touch.begin(Wire, 0x5A, -1, -1);
  halTouchDown(10, 10);
  halTouchUp();
  int16_t x = 0, y = 0;
  ASSERT_EQ((int)touch.getPoint(&x, &y, 1), 0);
}

TEST(touch, isPressed_acompanha_o_estado) {
  TouchDrvCST92xx touch;
  touch.begin(Wire, 0x5A, -1, -1);
  halTouchUp();
  ASSERT_TRUE(!touch.isPressed());
  halTouchDown(1, 1);
  ASSERT_TRUE(touch.isPressed());
}
```

Acrescente a `hal/hal_control.h`:
```cpp
int halTouchIrqPinForTest();
void halSetTouchIrqPin(int pin);   // chamado por TouchDrvCST92xx::setPins
```

- [ ] **Step 3: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `TouchDrvCSTXXX.hpp` não existe.

- [ ] **Step 4: Escrever `hal/TouchDrvCSTXXX.hpp`**

```cpp
#pragma once
#include <cstdint>
#include "Wire.h"

class TouchDrvCST92xx {
 public:
  bool begin(TwoWire& w, uint8_t addr = 0x5A, int sda = -1, int scl = -1) {
    (void)w; (void)addr; (void)sda; (void)scl; return true;
  }
  void setPins(int rst, int irq);
  void reset() {}
  void sleep() {}
  void wakeup() {}
  void setMirrorXY(bool, bool) {}
  void setSwapXY(bool) {}
  void setMaxCoordinates(uint16_t, uint16_t) {}
  uint8_t getPoint(int16_t* x, int16_t* y, uint8_t n = 1);
  bool isPressed();
  uint8_t getSupportTouchPoint() { return 1; }
};

// Alias que o upstream pode usar.
using TouchDrvCSTXXX = TouchDrvCST92xx;
```

Se o Step 1 mostrou outro nome de classe ou outro método, use o nome real.

- [ ] **Step 5: Escrever `hal/touch.cpp`**

```cpp
#include "TouchDrvCSTXXX.hpp"
#include "Arduino.h"
#include "hal_control.h"
#include <atomic>
#include <mutex>

namespace {
std::mutex gMu;
int gX = 0, gY = 0;
bool gDown = false;
int gIrqPin = 12;   // sobrescrito por setPins; default inofensivo
}  // namespace

int halTouchIrqPinForTest() { return gIrqPin; }
void halSetTouchIrqPin(int pin) { if (pin >= 0) gIrqPin = pin; }
void TouchDrvCST92xx::setPins(int, int irq) { halSetTouchIrqPin(irq); }

void halTouchDown(int x, int y) {
  { std::lock_guard<std::mutex> g(gMu); gX = x; gY = y; gDown = true; }
  halFireInterrupt(digitalPinToInterrupt(gIrqPin));
}
void halTouchMove(int x, int y) {
  bool estavaPressionado;
  { std::lock_guard<std::mutex> g(gMu); gX = x; gY = y; estavaPressionado = gDown; }
  if (estavaPressionado) halFireInterrupt(digitalPinToInterrupt(gIrqPin));
}
void halTouchUp() {
  { std::lock_guard<std::mutex> g(gMu); gDown = false; }
  halFireInterrupt(digitalPinToInterrupt(gIrqPin));
}

uint8_t TouchDrvCST92xx::getPoint(int16_t* x, int16_t* y, uint8_t n) {
  if (n == 0) return 0;
  std::lock_guard<std::mutex> g(gMu);
  if (!gDown) return 0;
  if (x) *x = (int16_t)gX;
  if (y) *y = (int16_t)gY;
  return 1;
}
bool TouchDrvCST92xx::isPressed() {
  std::lock_guard<std::mutex> g(gMu);
  return gDown;
}
```

- [ ] **Step 6: Rodar e ver passar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 4 testes de `touch`.

- [ ] **Step 7: Commit**

```bash
git add hal/TouchDrvCSTXXX.hpp hal/touch.cpp hal/hal_control.h tests/test_touch.cpp
git commit -m "feat(hal): toque a partir do mouse, disparando a ISR do upstream"
```

### Task 14: Gerador de protótipos do `.ino`

> **EXECUTADA COM REDESENHO (commit 710890b).** O desenho abaixo (header de protótipos via `/FI`) **não funciona**: `idleWait(PowerCtx c)` usa um tipo de `power.h`, e um protótipo force-incluído apareceria *antes* desse `#include`. O que foi implementado: `tools/gen_protos.py ENTRADA.ino -o SAIDA.cpp` gera uma **cópia .cpp completa** do sketch com `#include <Arduino.h>` (se faltar), os protótipos inseridos logo antes da primeira função (como o Arduino) e `#line` apontando para o `.ino` original. O CMake compila essa cópia gerada (`build/gen/TamaPoke.gen.cpp`), **sem `/TP` e sem `/FI`**. Testes em `unittest` (sem pytest), registrados no ctest como `gen_protos`. Verificado contra o sketch real: 90 protótipos e o `.ino` inteiro passa em `cl /Zs`, restando só a constante `PI`. O texto original da tarefa fica abaixo apenas como registro.

O Arduino gera protótipos de função automaticamente; o C++ não. Isto resolve sem patch no upstream.

**Files:**
- Create: `tools/gen_protos.py`, `tests/fixtures/exemplo.ino`, `tests/test_gen_protos.py`

- [ ] **Step 1: Escrever a fixture e o teste falhando**

```cpp
// tests/fixtures/exemplo.ino
#include <Arduino.h>

static const int kAlgo = 3;

void setup() {
  desenhar(1, 2);
}

void loop() {}

static void desenhar(int x, int y) {
  // corpo
}

uint16_t corDe(uint8_t i) { return i; }

void IRAM_ATTR naInterrupcao() {}

struct Bicho { int nivel; };          // nao e funcao
int tabela[4] = {1, 2, 3, 4};         // nao e funcao
```

```python
# tests/test_gen_protos.py
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[1]

def gerar(ino: Path) -> str:
    out = subprocess.run(
        [sys.executable, "-I", str(RAIZ / "tools" / "gen_protos.py"), str(ino)],
        capture_output=True, text=True, check=True)
    return out.stdout

def test_declara_as_funcoes_de_nivel_superior():
    proto = gerar(RAIZ / "tests" / "fixtures" / "exemplo.ino")
    assert "void setup();" in proto
    assert "void loop();" in proto
    assert "static void desenhar(int x, int y);" in proto
    assert "uint16_t corDe(uint8_t i);" in proto

def test_preserva_atributos_de_interrupcao():
    proto = gerar(RAIZ / "tests" / "fixtures" / "exemplo.ino")
    assert "void IRAM_ATTR naInterrupcao();" in proto

def test_ignora_o_que_nao_e_funcao():
    proto = gerar(RAIZ / "tests" / "fixtures" / "exemplo.ino")
    assert "Bicho" not in proto
    assert "tabela" not in proto
    assert "kAlgo" not in proto

def test_tem_include_guard():
    proto = gerar(RAIZ / "tests" / "fixtures" / "exemplo.ino")
    assert proto.lstrip().startswith("#pragma once")
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `python -m pytest tests/test_gen_protos.py -v`
Expected: FALHA — `tools/gen_protos.py` não existe.

- [ ] **Step 3: Escrever `tools/gen_protos.py`**

```python
#!/usr/bin/env python3
"""Gera protótipos das funções de nível superior de um .ino do Arduino.

Uso: python gen_protos.py ENTRADA.ino [-o SAIDA.h]
Sem -o, escreve em stdout. O header resultante entra no build com /FI.
"""
import argparse
import re
import sys

# Uma definicao de funcao de nivel superior: inicio de linha (coluna 0),
# tipo + nome + parenteses + "{" na mesma linha ou na seguinte.
PADRAO = re.compile(
    r"^(?P<assinatura>"
    r"(?:static\s+|inline\s+|extern\s+\"C\"\s+)*"      # qualificadores
    r"(?:const\s+)?"
    r"[A-Za-z_][A-Za-z0-9_:<>,\s\*&]*?"                # tipo de retorno
    r"\s+(?:IRAM_ATTR\s+|ICACHE_RAM_ATTR\s+)?"         # atributos de interrupcao
    r"(?P<nome>[A-Za-z_][A-Za-z0-9_]*)"                # nome
    r"\s*\((?P<args>[^;{)]*)\)"                        # argumentos
    r")\s*\{",                                          # abre corpo
    re.MULTILINE)

PALAVRAS_PROIBIDAS = ("if", "for", "while", "switch", "else", "do", "return",
                      "struct", "class", "enum", "union", "namespace")

def remover_comentarios_e_strings(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.DOTALL)
    src = re.sub(r"//[^\n]*", "", src)
    # Strings viram vazias para nao confundir o parser com chaves ou parenteses.
    src = re.sub(r'"(?:\\.|[^"\\])*"', '""', src)
    src = re.sub(r"'(?:\\.|[^'\\])*'", "'x'", src)
    return src

def prototipos(src: str) -> list:
    limpo = remover_comentarios_e_strings(src)
    # Junta uma eventual "{" que ficou na linha seguinte.
    limpo = re.sub(r"\)\s*\n\s*\{", ") {", limpo)
    vistos, saida = set(), []
    for m in PADRAO.finditer(limpo):
        nome = m.group("nome")
        if nome in PALAVRAS_PROIBIDAS:
            continue
        assinatura = " ".join(m.group("assinatura").split())
        if assinatura.split()[0] in PALAVRAS_PROIBIDAS:
            continue
        if nome in vistos:
            continue
        vistos.add(nome)
        saida.append(assinatura + ";")
    return saida

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("entrada")
    ap.add_argument("-o", "--saida")
    args = ap.parse_args()

    with open(args.entrada, "r", encoding="utf-8", errors="replace") as f:
        src = f.read()

    linhas = ["#pragma once",
              "// Gerado por tools/gen_protos.py a partir de " + args.entrada,
              "// Nao edite: regenerado em cada build.",
              ""]
    linhas += prototipos(src)
    texto = "\n".join(linhas) + "\n"

    if args.saida:
        with open(args.saida, "w", encoding="utf-8") as f:
            f.write(texto)
    else:
        sys.stdout.write(texto)
    return 0

if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Rodar e ver passar**

Run: `python -m pytest tests/test_gen_protos.py -v`
Expected: PASS nos 4 testes.

- [ ] **Step 5: Rodar contra o `.ino` real e conferir à mão**

Run:
```bash
python tools/gen_protos.py upstream/TamaPoke.ino | head -60
grep -cE '^[a-zA-Z_].*\(.*\) *\{' upstream/TamaPoke.ino
```
Expected: a contagem de funções do `.ino` e a contagem de protótipos gerados batem aproximadamente. Se alguma função de nível superior ficou de fora, acrescente uma fixture com esse formato e um teste antes de ajustar o regex — nunca ajuste o regex sem teste.

- [ ] **Step 6: Commit**

```bash
git add tools/gen_protos.py tests/test_gen_protos.py tests/fixtures/exemplo.ino
git commit -m "feat: gerador de prototipos do .ino com testes"
```

### Task 15: `GameHost` — o jogo rodando headless

Esta é a tarefa que faz o upstream inteiro compilar. Ao fim dela o jogo roda sem janela, verificado por teste.

**Files:**
- Create: `src/game_host.h`, `src/game_host.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/test_host.cpp`

- [ ] **Step 1: Escrever `src/game_host.h`**

```cpp
#pragma once
#include <cstdint>

// Interface estreita entre o shell (SDL/Win32) e o jogo (upstream + HAL).
// Nada aqui menciona SDL, HWND ou bandeja.
struct IGameHost {
  virtual ~IGameHost() = default;
  virtual void boot() = 0;
  virtual void tick() = 0;
  virtual bool frameDirty() const = 0;
  virtual const uint16_t* frame() const = 0;
  virtual uint8_t brightness() const = 0;
  virtual void touchDown(int x, int y) = 0;
  virtual void touchMove(int x, int y) = 0;
  virtual void touchUp() = 0;
  virtual void powerShortPress() = 0;
  virtual void injectConsole(const char* line) = 0;
  virtual void save() = 0;
  virtual bool wantsRestart() const = 0;
  virtual void clearFrameDirty() = 0;
};

// Implementacao sobre o upstream. dataDir e sdRoot sao absolutos.
IGameHost* createGameHost(const char* dataDir, const char* sdRoot);
void destroyGameHost(IGameHost* host);
```

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_host.cpp
#include "framework.h"
#include "game_host.h"
#include "hal_control.h"
#include <filesystem>
#include <memory>

static std::string dirLimpo(const char* nome) {
  auto d = std::filesystem::temp_directory_path() / nome;
  std::error_code ec;
  std::filesystem::remove_all(d, ec);
  std::filesystem::create_directories(d, ec);
  return d.string();
}

TEST(host, boot_e_ticks_produzem_frame) {
  std::string data = dirLimpo("tamapoke_host_a");
  IGameHost* h = createGameHost(data.c_str(), "upstream/tools/sdcard");
  h->boot();
  h->clearFrameDirty();
  for (int i = 0; i < 20; ++i) h->tick();
  ASSERT_TRUE(h->frameDirty());
  ASSERT_TRUE(h->frame() != nullptr);
  destroyGameHost(h);
}

TEST(host, toque_eh_entregue_ao_upstream) {
  std::string data = dirLimpo("tamapoke_host_b");
  IGameHost* h = createGameHost(data.c_str(), "upstream/tools/sdcard");
  h->boot();
  for (int i = 0; i < 5; ++i) h->tick();
  h->clearFrameDirty();
  h->touchDown(233, 300);
  for (int i = 0; i < 5; ++i) h->tick();
  h->touchUp();
  for (int i = 0; i < 5; ++i) h->tick();
  ASSERT_TRUE(h->frameDirty());          // a tela reagiu ao toque
  destroyGameHost(h);
}

TEST(host, comando_de_console_eh_consumido) {
  std::string data = dirLimpo("tamapoke_host_c");
  IGameHost* h = createGameHost(data.c_str(), "upstream/tools/sdcard");
  h->boot();
  h->injectConsole("STATS");
  for (int i = 0; i < 5; ++i) h->tick();   // o upstream le a linha no loop
  destroyGameHost(h);                      // nao travou, nao vazou
}

TEST(host, save_e_recarga_preservam_o_bicho) {
  std::string data = dirLimpo("tamapoke_host_d");
  {
    IGameHost* h = createGameHost(data.c_str(), "upstream/tools/sdcard");
    h->boot();
    for (int i = 0; i < 30; ++i) h->tick();
    h->save();
    destroyGameHost(h);
  }
  ASSERT_TRUE(std::filesystem::exists(data + "/nvs.json"));
  {
    IGameHost* h = createGameHost(data.c_str(), "upstream/tools/sdcard");
    h->boot();                             // nao recomeca do zero nem trava
    for (int i = 0; i < 5; ++i) h->tick();
    destroyGameHost(h);
  }
}
```

- [ ] **Step 3: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `game_host.h` sem implementação.

- [ ] **Step 4: Descobrir os pontos de entrada do upstream**

Run:
```bash
grep -nE '^void setup|^void loop' upstream/TamaPoke.ino
grep -nE 'flushSave|void begin|syncClock' upstream/pet.h
```
Expected: `setup()`/`loop()` e o nome real do método de gravação do `pet` (o plano assume `pet.flushSave()`). Use o nome real no Step 5.

- [ ] **Step 5: Escrever `src/game_host.cpp`**

```cpp
#include "game_host.h"
#include "hal_control.h"
#include "pet.h"            // do upstream: expõe o objeto `pet`
#include <string>

// Pontos de entrada do .ino, declarados pelo header gerado por gen_protos.py.
void setup();
void loop();

namespace {
class GameHost : public IGameHost {
 public:
  GameHost(std::string dataDir, std::string sdRoot)
      : dataDir_(std::move(dataDir)), sdRoot_(std::move(sdRoot)) {}

  void boot() override {
    halSetDataDir(dataDir_.c_str());
    halSetSdRoot(sdRoot_.c_str());
    setup();
    booted_ = true;
  }
  void tick() override { if (booted_) loop(); }

  bool frameDirty() const override { return halFrameDirty(); }
  void clearFrameDirty() override { halClearFrameDirty(); }
  const uint16_t* frame() const override { return halFrame(); }
  uint8_t brightness() const override { return halBrightness(); }

  void touchDown(int x, int y) override { halTouchDown(x, y); }
  void touchMove(int x, int y) override { halTouchMove(x, y); }
  void touchUp() override { halTouchUp(); }
  void powerShortPress() override { halPwrShortPress(); }

  void injectConsole(const char* line) override {
    if (!line) return;
    std::string s(line);
    if (s.empty() || s.back() != '\n') s.push_back('\n');
    halSerialInject(s.c_str());
  }

  void save() override { if (booted_) pet.flushSave(); }
  bool wantsRestart() const override { return halRestartRequested(); }

 private:
  std::string dataDir_, sdRoot_;
  bool booted_ = false;
};
}  // namespace

IGameHost* createGameHost(const char* dataDir, const char* sdRoot) {
  return new GameHost(dataDir ? dataDir : ".", sdRoot ? sdRoot : "./sdcard");
}
void destroyGameHost(IGameHost* h) { delete h; }
```

Se `pet.flushSave()` não existir com esse nome, use o que o Step 4 revelou. Se o upstream não expõe gravação explícita (salva sozinho dentro do `loop()`), troque o corpo de `save()` por um comentário dizendo isso e remova o `ASSERT_TRUE(exists(nvs.json))` do teste — o save passa a acontecer no `tick()`.

- [ ] **Step 6: Compilar o `.ino` com os protótipos gerados**

> **Atualizado:** use o desenho da Task 14 redesenhada. No CMake, um `add_custom_command` roda `python -I tools/gen_protos.py upstream/TamaPoke.ino -o ${CMAKE_BINARY_DIR}/gen/TamaPoke.gen.cpp` (DEPENDS no `.ino` e no script) e o alvo compila **esse .cpp gerado** como C++ normal. Ignore o `set_source_files_properties(... /TP;/FI...)` do bloco abaixo. **Pré-requisitos:** as Tasks 23 (áudio/FreeRTOS) e 24 (fontes CJK) devem estar feitas antes desta, porque `audio.cpp` e o `.ino` dependem delas. Falta ainda no `Arduino.h` da HAL a constante `PI` (usada em `TamaPoke.ino` linhas 2320 e 2332).

No `CMakeLists.txt`:

```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(INO_PROTOS ${CMAKE_BINARY_DIR}/gen/TamaPoke_protos.h)
add_custom_command(
  OUTPUT ${INO_PROTOS}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gen
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_SOURCE_DIR}/tools/gen_protos.py
          ${CMAKE_SOURCE_DIR}/upstream/TamaPoke.ino -o ${INO_PROTOS}
  DEPENDS ${CMAKE_SOURCE_DIR}/upstream/TamaPoke.ino
          ${CMAKE_SOURCE_DIR}/tools/gen_protos.py
  COMMENT "Gerando prototipos do TamaPoke.ino")
add_custom_target(ino_protos DEPENDS ${INO_PROTOS})

set(UPSTREAM_GAME_SRC
  upstream/TamaPoke.ino
  upstream/pet.cpp
  upstream/i18n.cpp
  upstream/sdmon.cpp
  upstream/rtcbat.cpp
  upstream/audio.cpp)

set_source_files_properties(upstream/TamaPoke.ino PROPERTIES
  LANGUAGE CXX
  COMPILE_OPTIONS "/TP;/FI${INO_PROTOS}")

target_sources(tamapoke_tests PRIVATE ${UPSTREAM_GAME_SRC} src/game_host.cpp)
target_include_directories(tamapoke_tests PRIVATE ${CMAKE_BINARY_DIR}/gen)
add_dependencies(tamapoke_tests ino_protos)
```

`target_sources` substitui os `target_sources` parciais de `upstream/sdmon.cpp` e `upstream/rtcbat.cpp` adicionados nas Tasks 8 e 9 — remova-os para não duplicar.

- [ ] **Step 7: Resolver os erros de compilação só na HAL**

Compile e trate cada erro **sem tocar em `upstream/`**:

Run: `cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Debug --target tamapoke_tests`

| Erro típico | Onde corrigir |
|---|---|
| símbolo de hardware não declarado | adicione à HAL o header/método que falta |
| `U8g2lib.h` não encontrado | crie `hal/U8g2lib.h` com os dois símbolos de fonte declarados como `extern const uint8_t[]` (os arrays entram na Fase 5; até lá, defina-os vazios em `hal/u8g2_fonts.c`) |
| `freertos/...h` não encontrado | crie `hal/freertos/FreeRTOS.h` e amigos reexportando `hal/freertos_shim.h` (Fase 4) |
| função do `.ino` não declarada | o gerador falhou nela: adicione fixture + teste na Task 14 e ajuste o regex |

Nenhum erro justifica editar `upstream/`. Se algum parecer justificar, pare e reporte.

- [ ] **Step 8: Rodar e ver passar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 4 testes de `host`. Isto é o jogo do upstream rodando de verdade, sem janela.

- [ ] **Step 9: Commit**

```bash
git add src/game_host.h src/game_host.cpp tests/test_host.cpp CMakeLists.txt
git commit -m "feat: GameHost rodando o upstream headless, com testes"
```

### Task 16: Tempo offline e progressão pelo RTC

**Files:**
- Test: `tests/test_host_offline.cpp`

- [ ] **Step 1: Descobrir a regra real do upstream**

Run:
```bash
grep -nE 'syncClock|3600|offline|lastSeen|level|LVL_MAX|14 *\* *24' upstream/pet.cpp | head -40
```
Expected: a regra de progressão offline (o plano assume +1 nível por hora, teto de 2 semanas) e o nome do método. Use os valores reais no teste.

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_host_offline.cpp
#include "framework.h"
#include "game_host.h"
#include "hal_control.h"
#include "pet.h"
#include <filesystem>
#include <string>

static std::string dirLimpo(const char* nome) {
  auto d = std::filesystem::temp_directory_path() / nome;
  std::error_code ec;
  std::filesystem::remove_all(d, ec);
  std::filesystem::create_directories(d, ec);
  return d.string();
}

TEST(host_offline, duas_horas_paradas_sobem_dois_niveis) {
  std::string data = dirLimpo("tamapoke_offline_a");
  halSetRtcOffsetForTest(0);

  IGameHost* h = createGameHost(data.c_str(), "upstream/tools/sdcard");
  h->boot();
  for (int i = 0; i < 20; ++i) h->tick();
  int nivelAntes = (int)pet.level();
  h->save();
  destroyGameHost(h);

  halSetRtcOffsetForTest(2 * 3600);        // o relogio "andou" 2 h com o app fechado
  IGameHost* h2 = createGameHost(data.c_str(), "upstream/tools/sdcard");
  h2->boot();
  for (int i = 0; i < 20; ++i) h2->tick();
  int nivelDepois = (int)pet.level();
  destroyGameHost(h2);

  halSetRtcOffsetForTest(0);
  ASSERT_EQ(nivelDepois - nivelAntes, 2);
}

TEST(host_offline, teto_de_duas_semanas) {
  std::string data = dirLimpo("tamapoke_offline_b");
  halSetRtcOffsetForTest(0);

  IGameHost* h = createGameHost(data.c_str(), "upstream/tools/sdcard");
  h->boot();
  for (int i = 0; i < 20; ++i) h->tick();
  int nivelAntes = (int)pet.level();
  h->save();
  destroyGameHost(h);

  halSetRtcOffsetForTest(60LL * 24 * 3600); // 60 dias
  IGameHost* h2 = createGameHost(data.c_str(), "upstream/tools/sdcard");
  h2->boot();
  for (int i = 0; i < 20; ++i) h2->tick();
  int nivelDepois = (int)pet.level();
  destroyGameHost(h2);

  halSetRtcOffsetForTest(0);
  ASSERT_TRUE(nivelDepois - nivelAntes <= 14 * 24);   // teto, nao 60 dias
}
```

Ajuste `pet.level()` ao acessor real (`grep -nE 'level|getLevel' upstream/pet.h`) e os números ao que o Step 1 revelou.

- [ ] **Step 3: Rodar**

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS. Se falhar, o problema está no caminho `rtcEpoch()` → `syncClock` — depure com `superpowers:systematic-debugging`, **sem** alterar o upstream.

- [ ] **Step 4: Commit**

```bash
git add tests/test_host_offline.cpp
git commit -m "test: progressao offline pelo RTC"
```

---

## Fase 4: shell de Windows (desktop pet)

### Task 17: `settings.json`

**Files:**
- Create: `src/settings.h`, `src/settings.cpp`
- Test: `tests/test_settings.cpp`

- [ ] **Step 1: Escrever o teste falhando**

```cpp
// tests/test_settings.cpp
#include "framework.h"
#include "settings.h"
#include "hal_control.h"
#include <filesystem>

static void dirLimpo(const char* nome) {
  auto d = std::filesystem::temp_directory_path() / nome;
  std::error_code ec;
  std::filesystem::remove_all(d, ec);
  std::filesystem::create_directories(d, ec);
  halSetDataDir(d.string().c_str());
}

TEST(settings, defaults_do_spec) {
  dirLimpo("tamapoke_set_a");
  Settings s = settingsLoad();
  ASSERT_EQ((int)(s.scale * 100), 100);     // 1x
  ASSERT_TRUE(s.alwaysOnTop);
  ASSERT_TRUE(s.autostart);
  ASSERT_TRUE(s.soundEnabled);
  ASSERT_EQ(s.windowX, -1);                 // -1 = centralizar
  ASSERT_EQ(s.windowY, -1);
}

TEST(settings, round_trip) {
  dirLimpo("tamapoke_set_b");
  Settings s = settingsLoad();
  s.scale = 2.0f; s.alwaysOnTop = false; s.autostart = false;
  s.soundEnabled = false; s.windowX = 300; s.windowY = 400;
  settingsSave(s);

  Settings r = settingsLoad();
  ASSERT_EQ((int)(r.scale * 100), 200);
  ASSERT_TRUE(!r.alwaysOnTop);
  ASSERT_TRUE(!r.autostart);
  ASSERT_TRUE(!r.soundEnabled);
  ASSERT_EQ(r.windowX, 300);
  ASSERT_EQ(r.windowY, 400);
}

TEST(settings, arquivo_corrompido_volta_aos_defaults) {
  dirLimpo("tamapoke_set_c");
  std::string path = std::string(halDataDir()) + "/settings.json";
  { std::FILE* f = std::fopen(path.c_str(), "wb"); std::fputs("lixo", f); std::fclose(f); }
  Settings s = settingsLoad();
  ASSERT_EQ((int)(s.scale * 100), 100);
}

TEST(settings, escala_fora_do_permitido_eh_corrigida) {
  dirLimpo("tamapoke_set_d");
  Settings s = settingsLoad();
  s.scale = 7.0f;
  settingsSave(s);
  Settings r = settingsLoad();
  ASSERT_EQ((int)(r.scale * 100), 100);     // so 0.5, 1, 2 sao validos
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `settings.h` não existe.

- [ ] **Step 3: Escrever `src/settings.h`**

```cpp
#pragma once

struct Settings {
  float scale = 1.0f;         // 0.5, 1.0 ou 2.0
  bool alwaysOnTop = true;
  bool autostart = true;
  bool soundEnabled = true;
  int windowX = -1;           // -1 = centralizar
  int windowY = -1;
  bool startHidden = false;   // definido por --hidden, nao persistido
};

Settings settingsLoad();
void settingsSave(const Settings& s);
```

- [ ] **Step 4: Escrever `src/settings.cpp`**

```cpp
#include "settings.h"
#include "hal_control.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace {
std::string path() { return std::string(halDataDir()) + "/settings.json"; }

float escalaValida(float s) {
  if (s > 0.4f && s < 0.6f) return 0.5f;
  if (s > 1.9f && s < 2.1f) return 2.0f;
  return 1.0f;
}
}  // namespace

Settings settingsLoad() {
  Settings s;
  std::ifstream in(path(), std::ios::binary);
  if (!in) return s;
  try {
    json j = json::parse(in);
    s.scale = escalaValida(j.value("scale", 1.0f));
    s.alwaysOnTop = j.value("alwaysOnTop", true);
    s.autostart = j.value("autostart", true);
    s.soundEnabled = j.value("soundEnabled", true);
    s.windowX = j.value("windowX", -1);
    s.windowY = j.value("windowY", -1);
  } catch (...) {
    return Settings{};
  }
  return s;
}

void settingsSave(const Settings& s) {
  std::error_code ec;
  std::filesystem::create_directories(halDataDir(), ec);
  json j{{"scale", escalaValida(s.scale)},
         {"alwaysOnTop", s.alwaysOnTop},
         {"autostart", s.autostart},
         {"soundEnabled", s.soundEnabled},
         {"windowX", s.windowX},
         {"windowY", s.windowY}};
  std::string p = path(), tmp = p + ".tmp";
  { std::ofstream out(tmp, std::ios::binary | std::ios::trunc); out << j.dump(2); }
#ifdef _WIN32
  MoveFileExA(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING);
#else
  std::filesystem::rename(tmp, p, ec);
#endif
}
```

- [ ] **Step 5: Rodar e ver passar**

Run:
```bash
cmake --build build --config Debug --target tamapoke_tests
ctest --test-dir build -C Debug -R tamapoke --output-on-failure
```
Expected: PASS nos 4 testes de `settings`. Adicione `src/settings.cpp` ao alvo com `target_sources(tamapoke_tests PRIVATE src/settings.cpp)`.

- [ ] **Step 6: Commit**

```bash
git add src/settings.h src/settings.cpp tests/test_settings.cpp CMakeLists.txt
git commit -m "feat: settings.json com defaults do spec"
```

### Task 18: Instância única e autostart

**Files:**
- Create: `src/win/single_instance.cpp`, `src/win/single_instance.h`, `src/win/autostart.cpp`, `src/win/autostart.h`
- Test: `tests/test_win_autostart.cpp`

- [ ] **Step 1: Escrever o teste falhando**

A chave do registro é testável: escreve numa subchave de teste e lê de volta.

```cpp
// tests/test_win_autostart.cpp
#include "framework.h"
#include "win/autostart.h"

TEST(autostart, habilita_le_e_desabilita) {
  const char* nome = "TamaPokeTestEntry";
  autostartDisable(nome);
  ASSERT_TRUE(!autostartEnabled(nome));

  ASSERT_TRUE(autostartEnable(nome, "C:\\nao\\existe\\tamapoke.exe --hidden"));
  ASSERT_TRUE(autostartEnabled(nome));

  ASSERT_TRUE(autostartDisable(nome));
  ASSERT_TRUE(!autostartEnabled(nome));
}

TEST(autostart, desabilitar_o_que_nao_existe_nao_eh_erro) {
  ASSERT_TRUE(autostartDisable("TamaPokeNuncaExistiu"));
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `win/autostart.h` não existe.

- [ ] **Step 3: Escrever `src/win/autostart.h` e `src/win/autostart.cpp`**

```cpp
// src/win/autostart.h
#pragma once
// Chave HKCU\Software\Microsoft\Windows\CurrentVersion\Run
bool autostartEnable(const char* valueName, const char* commandLine);
bool autostartDisable(const char* valueName);
bool autostartEnabled(const char* valueName);
```

```cpp
// src/win/autostart.cpp
#include "autostart.h"
#include <windows.h>

namespace {
const char* kRunKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
}

bool autostartEnable(const char* valueName, const char* commandLine) {
  HKEY key{};
  if (RegCreateKeyExA(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                      KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
    return false;
  LSTATUS st = RegSetValueExA(key, valueName, 0, REG_SZ,
                              (const BYTE*)commandLine,
                              (DWORD)(strlen(commandLine) + 1));
  RegCloseKey(key);
  return st == ERROR_SUCCESS;
}

bool autostartDisable(const char* valueName) {
  HKEY key{};
  if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
    return true;                       // chave ausente = ja desabilitado
  LSTATUS st = RegDeleteValueA(key, valueName);
  RegCloseKey(key);
  return st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND;
}

bool autostartEnabled(const char* valueName) {
  HKEY key{};
  if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    return false;
  DWORD type = 0, size = 0;
  LSTATUS st = RegQueryValueExA(key, valueName, nullptr, &type, nullptr, &size);
  RegCloseKey(key);
  return st == ERROR_SUCCESS && type == REG_SZ;
}
```

- [ ] **Step 4: Escrever `src/win/single_instance.h` e `.cpp`**

```cpp
// src/win/single_instance.h
#pragma once
// true = esta e a unica instancia. false = outra ja roda e foi avisada para
// aparecer; o chamador deve sair imediatamente.
bool singleInstanceAcquire();
void singleInstanceRelease();
// Mensagem que a instancia viva recebe quando outra tenta subir.
unsigned int singleInstanceShowMessage();
```

```cpp
// src/win/single_instance.cpp
#include "single_instance.h"
#include <windows.h>

namespace {
HANDLE gMutex = nullptr;
const char* kMutexName = "Local\\TamaPoke";
const char* kMsgName = "TamaPokeShowWindow";
}

unsigned int singleInstanceShowMessage() {
  static UINT msg = RegisterWindowMessageA(kMsgName);
  return msg;
}

bool singleInstanceAcquire() {
  gMutex = CreateMutexA(nullptr, TRUE, kMutexName);
  if (gMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    // Outra instancia esta viva: pede para ela aparecer e desiste.
    PostMessageA(HWND_BROADCAST, singleInstanceShowMessage(), 0, 0);
    CloseHandle(gMutex);
    gMutex = nullptr;
    return false;
  }
  return gMutex != nullptr;
}

void singleInstanceRelease() {
  if (gMutex) {
    ReleaseMutex(gMutex);
    CloseHandle(gMutex);
    gMutex = nullptr;
  }
}
```

- [ ] **Step 5: Rodar e ver passar**

Adicione ao alvo de testes:
```cmake
target_sources(tamapoke_tests PRIVATE src/win/autostart.cpp src/win/single_instance.cpp)
target_include_directories(tamapoke_tests PRIVATE src)
```

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 2 testes de `autostart`. (Instância única não tem teste automatizado — é checklist manual.)

- [ ] **Step 6: Commit**

```bash
git add src/win/autostart.h src/win/autostart.cpp src/win/single_instance.h \
        src/win/single_instance.cpp tests/test_win_autostart.cpp CMakeLists.txt
git commit -m "feat(win): autostart no registro e instancia unica"
```

### Task 19: Modo de janela — transparência com fallback e hit-test

**Files:**
- Create: `src/win/window_mode.h`, `src/win/window_mode.cpp`
- Test: `tests/test_window_mode.cpp`

- [ ] **Step 1: Escrever o teste falhando**

A decisão de hit-test é lógica pura; a aplicação em si é SDL/Win32 e vai para o checklist manual.

```cpp
// tests/test_window_mode.cpp
#include "framework.h"
#include "win/window_mode.h"

TEST(window_mode, hit_test_segue_o_circulo_na_escala) {
  ASSERT_TRUE(windowModeHit(233, 233, 1.0f));     // centro, 1x
  ASSERT_TRUE(!windowModeHit(0, 0, 1.0f));        // canto, 1x
  ASSERT_TRUE(windowModeHit(466, 466, 2.0f));     // centro em 2x
  ASSERT_TRUE(!windowModeHit(0, 0, 2.0f));
  ASSERT_TRUE(windowModeHit(116, 116, 0.5f));     // centro em 0.5x
}

TEST(window_mode, descricao_do_caminho_escolhido) {
  ASSERT_TRUE(std::string(windowModePathName(WindowModePath::Alpha)) == "alpha");
  ASSERT_TRUE(std::string(windowModePathName(WindowModePath::Region)) == "region");
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `win/window_mode.h` não existe.

- [ ] **Step 3: Escrever `src/win/window_mode.h`**

```cpp
#pragma once
#include <string>
struct SDL_Window;

enum class WindowModePath { Alpha, Region };

// Hit-test em coordenadas da janela (ja escaladas).
bool windowModeHit(int windowX, int windowY, float scale);
const char* windowModePathName(WindowModePath p);

// Aplica a silhueta redonda. Tenta alfa por pixel; se a janela nao suportar,
// cai para SetWindowRgn. Devolve o caminho usado, para o log.
WindowModePath windowModeApplyRound(SDL_Window* window, float scale);
void windowModeSetAlwaysOnTop(SDL_Window* window, bool onTop);
// Arrasta a janela com o botao direito; dx/dy em pixels de tela.
void windowModeMove(SDL_Window* window, int dx, int dy);
```

- [ ] **Step 4: Escrever `src/win/window_mode.cpp`**

```cpp
#include "window_mode.h"
#include "present.h"
#include "hal_control.h"
#include <SDL3/SDL.h>
#include <windows.h>

bool windowModeHit(int windowX, int windowY, float scale) {
  return presentInsideCircle(presentToLogical(windowX, scale),
                             presentToLogical(windowY, scale));
}

const char* windowModePathName(WindowModePath p) {
  return p == WindowModePath::Alpha ? "alpha" : "region";
}

WindowModePath windowModeApplyRound(SDL_Window* window, float scale) {
  if (!window) return WindowModePath::Region;

  // Caminho A: a janela foi criada com SDL_WINDOW_TRANSPARENT e o compositor
  // respeita alfa por pixel. O alfa vem do Presenter (fora do circulo = 0).
  SDL_WindowFlags flags = SDL_GetWindowFlags(window);
  if (flags & SDL_WINDOW_TRANSPARENT) return WindowModePath::Alpha;

  // Caminho B: recorte duro pela regiao. Sem antialias, mesma silhueta.
  HWND hwnd = (HWND)SDL_GetPointerProperty(
      SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
  if (!hwnd) return WindowModePath::Region;
  int side = (int)(HAL_SCREEN_W * scale);
  HRGN rgn = CreateEllipticRgn(0, 0, side, side);
  SetWindowRgn(hwnd, rgn, TRUE);   // o sistema passa a ser dono da regiao
  return WindowModePath::Region;
}

void windowModeSetAlwaysOnTop(SDL_Window* window, bool onTop) {
  if (window) SDL_SetWindowAlwaysOnTop(window, onTop);
}

void windowModeMove(SDL_Window* window, int dx, int dy) {
  if (!window || (dx == 0 && dy == 0)) return;
  int x = 0, y = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_SetWindowPosition(window, x + dx, y + dy);
}
```

- [ ] **Step 5: Rodar e ver passar**

Adicione `src/win/window_mode.cpp` ao alvo de testes.

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 2 testes de `window_mode`.

- [ ] **Step 6: Commit**

```bash
git add src/win/window_mode.h src/win/window_mode.cpp tests/test_window_mode.cpp CMakeLists.txt
git commit -m "feat(win): silhueta redonda com fallback de regiao e hit-test"
```

### Task 20: `AppShell` — máquina de estados e laço principal

**Files:**
- Create: `src/app_shell.h`, `src/app_shell.cpp`
- Test: `tests/test_app_shell.cpp`

- [ ] **Step 1: Escrever o teste falhando**

A política de tick é lógica pura: dado estado e tempo, quantos `tick()` e quando apresentar. Testada com um `IGameHost` falso, sem janela.

```cpp
// tests/test_app_shell.cpp
#include "framework.h"
#include "app_shell.h"
#include "game_host.h"

namespace {
class HostFalso : public IGameHost {
 public:
  int ticks = 0, boots = 0, saves = 0, pwr = 0;
  void boot() override { ++boots; }
  void tick() override { ++ticks; }
  bool frameDirty() const override { return dirty; }
  void clearFrameDirty() override { dirty = false; }
  const uint16_t* frame() const override { return fb; }
  uint8_t brightness() const override { return 255; }
  void touchDown(int x, int y) override { lastX = x; lastY = y; down = true; }
  void touchMove(int x, int y) override { lastX = x; lastY = y; }
  void touchUp() override { down = false; }
  void powerShortPress() override { ++pwr; }
  void injectConsole(const char*) override {}
  void save() override { ++saves; }
  bool wantsRestart() const override { return false; }

  bool dirty = false, down = false;
  int lastX = -1, lastY = -1;
  uint16_t fb[4] = {0, 0, 0, 0};
};
}  // namespace

TEST(app_shell, visible_pede_tick_a_10hz) {
  HostFalso h;
  TickPolicy p;
  ASSERT_EQ(p.tickIntervalMs(ShellState::Visible), 100);
  ASSERT_EQ(p.tickIntervalMs(ShellState::Hidden), 1000);
  ASSERT_EQ(p.tickIntervalMs(ShellState::ScreenOff), 1000);
  (void)h;
}

TEST(app_shell, apresenta_so_em_visible) {
  TickPolicy p;
  ASSERT_TRUE(p.shouldPresent(ShellState::Visible));
  ASSERT_TRUE(!p.shouldPresent(ShellState::Hidden));
  ASSERT_TRUE(p.shouldPresent(ShellState::ScreenOff));  // desenha preto
}

TEST(app_shell, brilho_zero_em_screen_off) {
  TickPolicy p;
  ASSERT_EQ((int)p.effectiveBrightness(ShellState::ScreenOff, 200), 0);
  ASSERT_EQ((int)p.effectiveBrightness(ShellState::Visible, 200), 200);
}

TEST(app_shell, esconder_salva_uma_vez) {
  HostFalso h;
  ShellStateMachine sm(&h);
  ASSERT_EQ(sm.state(), ShellState::Visible);
  sm.hide();
  ASSERT_EQ(sm.state(), ShellState::Hidden);
  ASSERT_EQ(h.saves, 1);
  sm.hide();                       // idempotente
  ASSERT_EQ(h.saves, 1);
}

TEST(app_shell, mostrar_volta_para_visible_e_faz_um_tick_de_sincronia) {
  HostFalso h;
  ShellStateMachine sm(&h);
  sm.hide();
  int antes = h.ticks;
  sm.show();
  ASSERT_EQ(sm.state(), ShellState::Visible);
  ASSERT_TRUE(h.ticks > antes);    // tick extra para o upstream ver o epoch novo
}

TEST(app_shell, f2_alterna_screen_off_e_avisa_o_host) {
  HostFalso h;
  ShellStateMachine sm(&h);
  sm.togglePower();
  ASSERT_EQ(sm.state(), ShellState::ScreenOff);
  ASSERT_EQ(h.pwr, 1);
  sm.togglePower();
  ASSERT_EQ(sm.state(), ShellState::Visible);
  ASSERT_EQ(h.pwr, 2);
}

TEST(app_shell, save_periodico_em_hidden_a_cada_5_min) {
  HostFalso h;
  ShellStateMachine sm(&h);
  sm.hide();
  int depoisDeEsconder = h.saves;
  sm.onElapsed(4 * 60 * 1000);
  ASSERT_EQ(h.saves, depoisDeEsconder);
  sm.onElapsed(1 * 60 * 1000 + 1);
  ASSERT_EQ(h.saves, depoisDeEsconder + 1);
}

TEST(app_shell, escape_nao_fecha) {
  HostFalso h;
  ShellStateMachine sm(&h);
  ASSERT_TRUE(!sm.quitRequested());
  sm.onEscape();
  ASSERT_TRUE(!sm.quitRequested());
  sm.requestQuit();
  ASSERT_TRUE(sm.quitRequested());
  ASSERT_TRUE(h.saves >= 1);       // sair salva
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `app_shell.h` não existe.

- [ ] **Step 3: Escrever `src/app_shell.h`**

```cpp
#pragma once
#include <cstdint>
#include "settings.h"
struct IGameHost;
struct SDL_Window;

enum class ShellState { Visible, Hidden, ScreenOff };

// Politica de ritmo: pura, sem estado.
struct TickPolicy {
  uint32_t tickIntervalMs(ShellState s) const {
    return s == ShellState::Visible ? 100u : 1000u;
  }
  bool shouldPresent(ShellState s) const { return s != ShellState::Hidden; }
  uint8_t effectiveBrightness(ShellState s, uint8_t b) const {
    return s == ShellState::ScreenOff ? 0u : b;
  }
};

// Transicoes e save. Nao conhece SDL.
class ShellStateMachine {
 public:
  explicit ShellStateMachine(IGameHost* host) : host_(host) {}
  ShellState state() const { return state_; }
  void hide();
  void show();
  void togglePower();
  void onElapsed(uint32_t ms);       // tempo decorrido, para o save periodico
  void onEscape() {}                 // Esc nao faz nada, de proposito
  void requestQuit();
  bool quitRequested() const { return quit_; }

 private:
  IGameHost* host_;
  ShellState state_ = ShellState::Visible;
  uint32_t msSinceSave_ = 0;
  bool quit_ = false;
};

// Laco principal com SDL. Testado a mao (checklist).
class AppShell {
 public:
  bool init(IGameHost* host, const Settings& s);
  void run();
  void shutdown();

 private:
  void pumpEvents();
  void present();

  IGameHost* host_ = nullptr;
  SDL_Window* window_ = nullptr;
  Settings settings_{};
  TickPolicy policy_{};
  ShellStateMachine* sm_ = nullptr;
  bool draggingRight_ = false;
};
```

- [ ] **Step 4: Escrever `src/app_shell.cpp` — parte 1: a máquina de estados**

```cpp
#include "app_shell.h"
#include "game_host.h"
#include "present.h"
#include "hal_control.h"
#include "win/window_mode.h"
#include "win/tray.h"
#include <SDL3/SDL.h>

static const uint32_t kSavePeriodHiddenMs = 5u * 60u * 1000u;

void ShellStateMachine::hide() {
  if (state_ == ShellState::Hidden) return;
  state_ = ShellState::Hidden;
  msSinceSave_ = 0;
  host_->save();
}
void ShellStateMachine::show() {
  state_ = ShellState::Visible;
  msSinceSave_ = 0;
  host_->tick();        // deixa o upstream ver o epoch novo (progressao offline)
}
void ShellStateMachine::togglePower() {
  host_->powerShortPress();
  state_ = (state_ == ShellState::ScreenOff) ? ShellState::Visible
                                             : ShellState::ScreenOff;
}
void ShellStateMachine::onElapsed(uint32_t ms) {
  if (state_ != ShellState::Hidden) return;
  msSinceSave_ += ms;
  if (msSinceSave_ > kSavePeriodHiddenMs) { msSinceSave_ = 0; host_->save(); }
}
void ShellStateMachine::requestQuit() {
  host_->save();
  quit_ = true;
}
```

- [ ] **Step 5: Rodar os testes da máquina de estados**

Adicione `src/app_shell.cpp` ao alvo de testes. Como ele inclui `win/tray.h` (Task 21), crie primeiro um `src/win/tray.h` com as declarações e um `src/win/tray.cpp` de corpos vazios; a Task 21 os implementa.

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 8 testes de `app_shell`.

- [ ] **Step 6: Escrever `src/app_shell.cpp` — parte 2: o laço SDL**

```cpp
bool AppShell::init(IGameHost* host, const Settings& s) {
  host_ = host;
  settings_ = s;
  sm_ = new ShellStateMachine(host);

  SDL_WindowFlags flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT |
                          SDL_WINDOW_UTILITY;
  if (settings_.alwaysOnTop) flags |= SDL_WINDOW_ALWAYS_ON_TOP;

  int side = (int)(HAL_SCREEN_W * settings_.scale);
  window_ = SDL_CreateWindow("TamaPoke", side, side, flags);
  if (!window_) {
    flags &= ~SDL_WINDOW_TRANSPARENT;      // sem alfa: cai para regiao
    window_ = SDL_CreateWindow("TamaPoke", side, side, flags);
  }
  if (!window_) return false;

  if (settings_.windowX >= 0 && settings_.windowY >= 0)
    SDL_SetWindowPosition(window_, settings_.windowX, settings_.windowY);

  WindowModePath caminho = windowModeApplyRound(window_, settings_.scale);
  SDL_Log("silhueta: %s", windowModePathName(caminho));

  if (!presenterInit(window_, settings_.scale)) return false;

  // Espera ociosa do upstream (esp_light_sleep_start) vira espera de evento.
  halSetIdleWaiter([](uint32_t ms) {
    SDL_Event ev;
    SDL_WaitEventTimeout(&ev, (int)ms);
    SDL_PushEvent(&ev);                    // devolve para o laco tratar
  });

  trayInit(window_, &settings_);
  if (settings_.startHidden) { SDL_HideWindow(window_); sm_->hide(); }
  return true;
}

void AppShell::run() {
  uint64_t ultimoTick = SDL_GetTicks();
  while (!sm_->quitRequested()) {
    ShellState st = sm_->state();
    uint32_t intervalo = policy_.tickIntervalMs(st);

    pumpEvents();

    uint64_t agora = SDL_GetTicks();
    uint32_t decorrido = (uint32_t)(agora - ultimoTick);
    if (decorrido >= intervalo) {
      ultimoTick = agora;
      sm_->onElapsed(decorrido);
      host_->tick();

      if (host_->wantsRestart()) {
        host_->save();
        destroyGameHost(host_);
        host_ = createGameHost(halDataDir(), halSdRoot());
        host_->boot();
        halClearRestartRequest();
        continue;
      }
      if (policy_.shouldPresent(sm_->state()) && host_->frameDirty()) {
        present();
        host_->clearFrameDirty();
      }
    } else {
      // Dorme ate o proximo evento ou o proximo tick: CPU ocioso ~0%.
      SDL_Event ev;
      if (SDL_WaitEventTimeout(&ev, (int)(intervalo - decorrido))) SDL_PushEvent(&ev);
    }
  }
}

void AppShell::present() {
  presenterDraw(host_->frame(),
                policy_.effectiveBrightness(sm_->state(), host_->brightness()),
                true);
}

void AppShell::shutdown() {
  trayShutdown();
  presenterShutdown();
  if (window_) { 
    int x = 0, y = 0;
    SDL_GetWindowPosition(window_, &x, &y);
    settings_.windowX = x; settings_.windowY = y;
    settingsSave(settings_);
    SDL_DestroyWindow(window_); window_ = nullptr;
  }
  delete sm_; sm_ = nullptr;
}
```

Nota: `presenterInit`/`presenterDraw`/`presenterShutdown` são wrappers livres sobre a classe `Presenter` da Task 12 (um `Presenter` global em `present.cpp`) — adicione-os a `src/present.h` e `src/present.cpp`:

```cpp
// src/present.h (acrescentar)
bool presenterInit(SDL_Window* window, float scale);
void presenterDraw(const uint16_t* frame, uint8_t brightness, bool circular);
void presenterShutdown();
```

```cpp
// src/present.cpp (acrescentar)
namespace { Presenter gPresenter; }
bool presenterInit(SDL_Window* w, float scale) {
  gPresenter.setScale(scale);
  return gPresenter.init(w);
}
void presenterDraw(const uint16_t* f, uint8_t b, bool c) { gPresenter.draw(f, b, c); }
void presenterShutdown() { gPresenter.shutdown(); }
```

- [ ] **Step 7: Escrever `src/app_shell.cpp` — parte 3: eventos e input**

```cpp
void AppShell::pumpEvents() {
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_EVENT_QUIT:
        // O X da janela nao fecha o app: esconde para a bandeja.
        SDL_HideWindow(window_);
        sm_->hide();
        break;

      case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        SDL_HideWindow(window_);
        sm_->hide();
        break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev.button.button == SDL_BUTTON_LEFT) {
          if (windowModeHit((int)ev.button.x, (int)ev.button.y, settings_.scale))
            host_->touchDown(presentToLogical((int)ev.button.x, settings_.scale),
                             presentToLogical((int)ev.button.y, settings_.scale));
        } else if (ev.button.button == SDL_BUTTON_RIGHT) {
          draggingRight_ = true;
        }
        break;

      case SDL_EVENT_MOUSE_MOTION:
        if (draggingRight_) {
          windowModeMove(window_, (int)ev.motion.xrel, (int)ev.motion.yrel);
        } else if (ev.motion.state & SDL_BUTTON_LMASK) {
          host_->touchMove(presentToLogical((int)ev.motion.x, settings_.scale),
                           presentToLogical((int)ev.motion.y, settings_.scale));
        }
        break;

      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (ev.button.button == SDL_BUTTON_LEFT) {
          host_->touchUp();
        } else if (ev.button.button == SDL_BUTTON_RIGHT) {
          bool arrastou = draggingRight_ && trayWasDragged();
          draggingRight_ = false;
          if (!arrastou) trayShowContextMenu();   // clique seco = menu
        }
        break;

      case SDL_EVENT_KEY_DOWN:
        switch (ev.key.key) {
          case SDLK_F2:     sm_->togglePower(); break;
          case SDLK_ESCAPE: sm_->onEscape(); break;   // de proposito: nada
          case SDLK_F12:    trayToggleConsole(); break;
          case SDLK_LEFT:   host_->touchDown(400, 233); host_->touchMove(60, 233); host_->touchUp(); break;
          case SDLK_RIGHT:  host_->touchDown(60, 233);  host_->touchMove(400, 233); host_->touchUp(); break;
          case SDLK_UP:     host_->touchDown(233, 400); host_->touchMove(233, 60);  host_->touchUp(); break;
          case SDLK_DOWN:   host_->touchDown(233, 60);  host_->touchMove(233, 400); host_->touchUp(); break;
          default: break;
        }
        break;

      default:
        if (trayHandleEvent(ev, sm_)) break;
        break;
    }
  }
}
```

Nota sobre as setas: um swipe por teclado precisa de três `tick()` entre `touchDown`, `touchMove` e `touchUp` para o upstream perceber o gesto. Se o teste manual mostrar que o swipe não registra, converta as setas num gesto com estado (uma struct `SwipeSimulado` que entrega um passo por `tick()`) — e escreva o teste dela antes.

- [ ] **Step 8: Rodar os testes e compilar o alvo `tamapoke`**

**Substitua** o `add_executable(tamapoke WIN32 src/main.cpp)` e o `target_link_libraries(tamapoke ...)` da Task 1 pelo bloco abaixo (nao crie um segundo alvo `tamapoke`). Mantenha o `add_custom_command(TARGET tamapoke POST_BUILD ... copy_if_different $<TARGET_FILE:SDL3::SDL3> ...)` que a Task 1 adicionou: ele copia `SDL3.dll` para junto do exe.

```cmake
add_executable(tamapoke WIN32
  src/main.cpp src/app_shell.cpp src/game_host.cpp src/present.cpp src/settings.cpp
  src/win/tray.cpp src/win/autostart.cpp src/win/single_instance.cpp src/win/window_mode.cpp
  ${HAL_SRC} ${UPSTREAM_GAME_SRC})
target_include_directories(tamapoke PRIVATE hal src upstream ${CMAKE_BINARY_DIR}/gen)
target_link_libraries(tamapoke PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json)
add_dependencies(tamapoke ino_protos)
```

Se a Task 10 escolheu o GFX real, acrescente `target_link_libraries(tamapoke PRIVATE gfx_core)` e o include de `third_party/Arduino_GFX/src`.

Run:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```
Expected: ambos os alvos compilam; todos os testes passam.

- [ ] **Step 9: Commit**

```bash
git add src/app_shell.h src/app_shell.cpp src/present.h src/present.cpp \
        tests/test_app_shell.cpp CMakeLists.txt
git commit -m "feat: AppShell com maquina de estados, laco de eventos e input"
```

### Task 21: Bandeja

**Files:**
- Create: `src/win/tray.h`, `src/win/tray.cpp` (substituindo os stubs da Task 20)

- [ ] **Step 1: Escrever `src/win/tray.h`**

```cpp
#pragma once
union SDL_Event;
struct SDL_Window;
struct Settings;
class ShellStateMachine;

bool trayInit(SDL_Window* window, Settings* settings);
void trayShutdown();
// true = o evento era da bandeja e foi tratado.
bool trayHandleEvent(const SDL_Event& ev, ShellStateMachine* sm);
void trayShowContextMenu();
void trayToggleConsole();
bool trayWasDragged();          // o botao direito moveu a janela?
void traySetTooltip(const char* text);
```

- [ ] **Step 2: Escrever `src/win/tray.cpp`**

```cpp
#include "tray.h"
#include "app_shell.h"
#include "settings.h"
#include "autostart.h"
#include "window_mode.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_tray.h>
#include <string>

namespace {
SDL_Tray* gTray = nullptr;
SDL_TrayMenu* gMenu = nullptr;
SDL_Window* gWindow = nullptr;
Settings* gSettings = nullptr;
ShellStateMachine* gSm = nullptr;
bool gConsoleAberto = false;
bool gArrastou = false;

void onMostrarEsconder(void*, SDL_TrayEntry*) {
  if (!gWindow || !gSm) return;
  if (gSm->state() == ShellState::Hidden) { SDL_ShowWindow(gWindow); gSm->show(); }
  else { SDL_HideWindow(gWindow); gSm->hide(); }
}
void onSom(void*, SDL_TrayEntry* e) {
  if (!gSettings) return;
  gSettings->soundEnabled = SDL_GetTrayEntryChecked(e);
  settingsSave(*gSettings);
  // audioSetEnabled do upstream e aplicado pelo host no proximo tick (Task 23).
}
void onSempreNoTopo(void*, SDL_TrayEntry* e) {
  if (!gSettings) return;
  gSettings->alwaysOnTop = SDL_GetTrayEntryChecked(e);
  windowModeSetAlwaysOnTop(gWindow, gSettings->alwaysOnTop);
  settingsSave(*gSettings);
}
void onAutostart(void*, SDL_TrayEntry* e) {
  if (!gSettings) return;
  gSettings->autostart = SDL_GetTrayEntryChecked(e);
  if (gSettings->autostart) {
    std::string cmd = std::string("\"") + SDL_GetBasePath() + "tamapoke.exe\" --hidden";
    autostartEnable("TamaPoke", cmd.c_str());
  } else {
    autostartDisable("TamaPoke");
  }
  settingsSave(*gSettings);
}
void onEscala(void* userdata, SDL_TrayEntry*) {
  if (!gSettings || !gWindow) return;
  float escala = *(float*)userdata;
  gSettings->scale = escala;
  int lado = (int)(466 * escala);
  SDL_SetWindowSize(gWindow, lado, lado);
  windowModeApplyRound(gWindow, escala);
  settingsSave(*gSettings);
}
void onSair(void*, SDL_TrayEntry*) { if (gSm) gSm->requestQuit(); }

float gEscala05 = 0.5f, gEscala10 = 1.0f, gEscala20 = 2.0f;
}  // namespace

bool trayInit(SDL_Window* window, Settings* settings) {
  gWindow = window;
  gSettings = settings;
  gTray = SDL_CreateTray(nullptr, "TamaPoke");
  if (!gTray) return false;
  gMenu = SDL_CreateTrayMenu(gTray);

  SDL_TrayEntry* e;
  e = SDL_InsertTrayEntryAt(gMenu, -1, "Mostrar/Esconder", SDL_TRAYENTRY_BUTTON);
  SDL_SetTrayEntryCallback(e, onMostrarEsconder, nullptr);

  e = SDL_InsertTrayEntryAt(gMenu, -1, "Som", SDL_TRAYENTRY_CHECKBOX);
  SDL_SetTrayEntryChecked(e, settings->soundEnabled);
  SDL_SetTrayEntryCallback(e, onSom, nullptr);

  e = SDL_InsertTrayEntryAt(gMenu, -1, "Sempre no topo", SDL_TRAYENTRY_CHECKBOX);
  SDL_SetTrayEntryChecked(e, settings->alwaysOnTop);
  SDL_SetTrayEntryCallback(e, onSempreNoTopo, nullptr);

  SDL_TrayEntry* submenuPai =
      SDL_InsertTrayEntryAt(gMenu, -1, "Escala", SDL_TRAYENTRY_SUBMENU);
  SDL_TrayMenu* sub = SDL_CreateTraySubmenu(submenuPai);
  e = SDL_InsertTrayEntryAt(sub, -1, "0,5x", SDL_TRAYENTRY_BUTTON);
  SDL_SetTrayEntryCallback(e, onEscala, &gEscala05);
  e = SDL_InsertTrayEntryAt(sub, -1, "1x", SDL_TRAYENTRY_BUTTON);
  SDL_SetTrayEntryCallback(e, onEscala, &gEscala10);
  e = SDL_InsertTrayEntryAt(sub, -1, "2x", SDL_TRAYENTRY_BUTTON);
  SDL_SetTrayEntryCallback(e, onEscala, &gEscala20);

  e = SDL_InsertTrayEntryAt(gMenu, -1, "Iniciar com o Windows", SDL_TRAYENTRY_CHECKBOX);
  SDL_SetTrayEntryChecked(e, settings->autostart);
  SDL_SetTrayEntryCallback(e, onAutostart, nullptr);

  e = SDL_InsertTrayEntryAt(gMenu, -1, "Sair", SDL_TRAYENTRY_BUTTON);
  SDL_SetTrayEntryCallback(e, onSair, nullptr);
  return true;
}

void trayShutdown() {
  if (gTray) { SDL_DestroyTray(gTray); gTray = nullptr; gMenu = nullptr; }
}
bool trayHandleEvent(const SDL_Event&, ShellStateMachine* sm) { gSm = sm; return false; }
void trayShowContextMenu() { /* SDL nao expoe abrir o menu da bandeja por codigo */ }
void trayToggleConsole() { gConsoleAberto = !gConsoleAberto; }
bool trayWasDragged() { return gArrastou; }
void traySetTooltip(const char* text) { if (gTray) SDL_SetTrayTooltip(gTray, text); }
```

- [ ] **Step 3: Conferir a API real do SDL3 de bandeja**

Run:
```bash
grep -nE 'SDL_CreateTray|SDL_InsertTrayEntryAt|SDL_SetTrayEntryCallback|SDL_SetTrayTooltip|SDL_CreateTraySubmenu' build/_deps/sdl3-src/include/SDL3/SDL_tray.h
```
Expected: as assinaturas reais. Ajuste as chamadas acima a elas. Se `SDL_tray.h` não existir nessa versão do SDL3, implemente a bandeja com `Shell_NotifyIconW` direto neste mesmo arquivo — é o plano B do spec, e nenhum outro arquivo muda, porque todos só falam com `tray.h`.

- [ ] **Step 4: Compilar e testar à mão**

Run:
```bash
cmake --build build --config Debug
./build/Debug/tamapoke.exe
```
Expected: janela redonda sem borda, sempre no topo, ícone na bandeja. Fechar esconde; "Mostrar/Esconder" traz de volta; "Sair" encerra.

- [ ] **Step 5: Commit**

```bash
git add src/win/tray.h src/win/tray.cpp
git commit -m "feat(win): bandeja com mostrar/esconder, som, topo, escala, autostart e sair"
```

### Task 22: `main.cpp` — amarrar tudo

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Escrever `src/main.cpp`**

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <shlobj.h>
#include <string>
#include <thread>
#include "app_shell.h"
#include "game_host.h"
#include "settings.h"
#include "hal_control.h"
#include "win/single_instance.h"
#include "win/autostart.h"

namespace {
std::string appDataDir() {
  PWSTR raw = nullptr;
  std::string out = ".";
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw))) {
    char utf8[MAX_PATH * 4] = {0};
    WideCharToMultiByte(CP_UTF8, 0, raw, -1, utf8, sizeof utf8, nullptr, nullptr);
    out = std::string(utf8) + "\\TamaPoke";
    CoTaskMemFree(raw);
  }
  return out;
}

void consoleReader(IGameHost* host) {
  std::string linha;
  while (std::getline(std::cin, linha)) host->injectConsole(linha.c_str());
}
}  // namespace

int main(int argc, char** argv) {
  bool escondido = false, console = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--hidden") escondido = true;
    if (std::string(argv[i]) == "--console") console = true;
  }

  if (!singleInstanceAcquire()) return 0;   // outra instancia foi avisada

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TamaPoke",
                             SDL_GetError(), nullptr);
    singleInstanceRelease();
    return 1;
  }

  std::string data = appDataDir();
  std::string sd = std::string(SDL_GetBasePath()) + "sdcard";
  halSetDataDir(data.c_str());

  Settings settings = settingsLoad();
  settings.startHidden = escondido;

  // Primeiro run: registra o autostart se a configuracao pede e ele nao existe.
  if (settings.autostart && !autostartEnabled("TamaPoke")) {
    std::string cmd = std::string("\"") + SDL_GetBasePath() + "tamapoke.exe\" --hidden";
    autostartEnable("TamaPoke", cmd.c_str());
  }

  IGameHost* host = createGameHost(data.c_str(), sd.c_str());
  host->boot();

  if (console) std::thread(consoleReader, host).detach();

  AppShell shell;
  if (!shell.init(host, settings)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TamaPoke",
                             "Nao foi possivel criar a janela.", nullptr);
    destroyGameHost(host);
    SDL_Quit();
    singleInstanceRelease();
    return 1;
  }
  shell.run();
  host->save();
  shell.shutdown();
  destroyGameHost(host);
  SDL_Quit();
  singleInstanceRelease();
  return 0;
}
```

Adicione `#include <iostream>` para o `std::getline`. Ligue `Shell32.lib` e `Ole32.lib`:
```cmake
target_link_libraries(tamapoke PRIVATE Shell32 Ole32)
```

- [ ] **Step 2: Salvar no fim da sessão do Windows**

O `WM_QUERYENDSESSION` chega como `SDL_EVENT_TERMINATING`. Acrescente em `AppShell::pumpEvents`:

```cpp
      case SDL_EVENT_TERMINATING:
        host_->save();
        sm_->requestQuit();
        break;
```

- [ ] **Step 3: Compilar e rodar**

Run:
```bash
cmake --build build --config Debug
./build/Debug/tamapoke.exe --console
```
Expected: tela de escolha do inicial (Bulbasaur/Charmander/Squirtle). Clique num deles com o botão esquerdo e veja a reação.

- [ ] **Step 4: Copiar os sprites para a saída**

```cmake
add_custom_command(TARGET tamapoke POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
          ${CMAKE_SOURCE_DIR}/upstream/tools/sdcard/mons
          $<TARGET_FILE_DIR:tamapoke>/sdcard/mons
  COMMENT "Copiando sprites para a pasta de saida")
```

Run: `cmake --build build --config Debug && ls build/Debug/sdcard/mons | wc -l`
Expected: 303 arquivos.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp src/app_shell.cpp CMakeLists.txt
git commit -m "feat: TamaPoke rodando como desktop pet no Windows"
```

### Task 22b: Console de debug pelo F12 e tooltip da bandeja

Fecha duas coisas que o spec pede e que as Tasks 21 e 22 deixaram como stub: o F12 abrindo um console de verdade e a tooltip mostrando nome e nível do bicho.

**Files:**
- Create: `src/win/console.h`, `src/win/console.cpp`
- Modify: `src/win/tray.cpp`, `src/app_shell.cpp`, `src/game_host.h`, `src/game_host.cpp`

- [ ] **Step 1: Escrever `src/win/console.h` e `src/win/console.cpp`**

O exe é `WIN32` (sem console). O F12 precisa alocar um de verdade e redirecionar `stdin`/`stdout`.

```cpp
// src/win/console.h
#pragma once
// Abre ou fecha um console para os comandos de debug (STATS, LVL, TIME <epoch>...).
// Devolve true se o console esta aberto depois da chamada.
bool consoleToggle();
bool consoleIsOpen();
// Le uma linha sem bloquear. Devolve false se nao havia linha pronta.
bool consoleReadLine(char* out, int maxLen);
void consoleClose();
```

```cpp
// src/win/console.cpp
#include "console.h"
#include <windows.h>
#include <cstdio>
#include <string>

namespace {
bool gAberto = false;
std::string gParcial;
}  // namespace

bool consoleIsOpen() { return gAberto; }

bool consoleToggle() {
  if (gAberto) { consoleClose(); return false; }
  if (!AllocConsole()) return false;
  FILE* f = nullptr;
  freopen_s(&f, "CONOUT$", "w", stdout);
  freopen_s(&f, "CONOUT$", "w", stderr);
  freopen_s(&f, "CONIN$", "r", stdin);
  SetConsoleTitleA("TamaPoke — console de debug");
  std::printf("Comandos: STATS, LVL <n>, HATCH, TIME <epoch>, WIPE, HEALTH\n");
  gAberto = true;
  return true;
}

void consoleClose() {
  if (!gAberto) return;
  FreeConsole();
  gAberto = false;
  gParcial.clear();
}

// Leitura nao bloqueante: so consome o que ja esta no buffer do console.
bool consoleReadLine(char* out, int maxLen) {
  if (!gAberto || !out || maxLen <= 1) return false;
  HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
  if (in == INVALID_HANDLE_VALUE) return false;

  DWORD pendentes = 0;
  if (!GetNumberOfConsoleInputEvents(in, &pendentes) || pendentes == 0) return false;

  INPUT_RECORD rec[64];
  DWORD lidos = 0;
  if (!ReadConsoleInputA(in, rec, 64, &lidos)) return false;
  for (DWORD i = 0; i < lidos; ++i) {
    if (rec[i].EventType != KEY_EVENT || !rec[i].Event.KeyEvent.bKeyDown) continue;
    char c = rec[i].Event.KeyEvent.uChar.AsciiChar;
    if (c == '\r' || c == '\n') {
      if (gParcial.empty()) continue;
      std::snprintf(out, maxLen, "%s", gParcial.c_str());
      std::printf("> %s\n", gParcial.c_str());
      gParcial.clear();
      return true;
    }
    if (c == '\b') { if (!gParcial.empty()) gParcial.pop_back(); continue; }
    if (c >= 32 && c < 127) gParcial.push_back(c);
  }
  return false;
}
```

- [ ] **Step 2: Ligar o F12 e a leitura ao laço**

Em `src/win/tray.cpp`, troque o corpo de `trayToggleConsole`:

```cpp
#include "console.h"
...
void trayToggleConsole() { consoleToggle(); }
```

Em `src/app_shell.cpp`, dentro de `AppShell::pumpEvents`, logo no início:

```cpp
  // Comandos de debug digitados no console (F12).
  if (consoleIsOpen()) {
    char linha[256];
    while (consoleReadLine(linha, sizeof linha)) host_->injectConsole(linha);
  }
```

com `#include "win/console.h"` no topo.

- [ ] **Step 3: Expor nome e nível no host**

Em `src/game_host.h`, acrescente a `IGameHost`:

```cpp
  // Para a tooltip da bandeja. Nunca devolve nullptr.
  virtual const char* petName() const = 0;
  virtual int petLevel() const = 0;
```

Em `src/game_host.cpp`, dentro de `GameHost`:

```cpp
  const char* petName() const override {
    if (!booted_) return "TamaPoke";
    nameCache_ = pet.nickname();      // ajuste ao acessor real de pet.h
    return nameCache_.empty() ? "TamaPoke" : nameCache_.c_str();
  }
  int petLevel() const override { return booted_ ? (int)pet.level() : 0; }
```

com `mutable std::string nameCache_;` entre os membros. Descubra os acessores reais:

```bash
grep -nE 'nickname|name\(|level\(|getLevel' upstream/pet.h
```

Acrescente ao `HostFalso` de `tests/test_app_shell.cpp`:

```cpp
  const char* petName() const override { return "Falso"; }
  int petLevel() const override { return 3; }
```

- [ ] **Step 4: Atualizar a tooltip a cada minuto**

Em `src/app_shell.cpp`, dentro de `AppShell::run`, no bloco que roda quando `decorrido >= intervalo`:

```cpp
      tooltipAcc_ += decorrido;
      if (tooltipAcc_ >= 60000u) {
        tooltipAcc_ = 0;
        char buf[128];
        SDL_snprintf(buf, sizeof buf, "TamaPoke — %s, nivel %d",
                     host_->petName(), host_->petLevel());
        traySetTooltip(buf);
      }
```

com `uint32_t tooltipAcc_ = 60000;` entre os membros de `AppShell` (começa no limite, então a primeira tooltip aparece no primeiro tick).

- [ ] **Step 5: Compilar e testar à mão**

Adicione `src/win/console.cpp` aos dois alvos.

Run:
```bash
cmake --build build --config Debug
./build/Debug/tamapoke.exe
```
Expected: F12 abre um console; digitar `STATS` e Enter imprime as estatísticas do bicho; F12 de novo fecha. A tooltip da bandeja mostra nome e nível.

- [ ] **Step 6: Rodar a suíte**

Run: `ctest --test-dir build -C Debug --output-on-failure`
Expected: tudo verde (o `HostFalso` precisa dos dois métodos novos, senão não compila).

- [ ] **Step 7: Commit**

```bash
git add src/win/console.h src/win/console.cpp src/win/tray.cpp src/app_shell.cpp \
        src/game_host.h src/game_host.cpp tests/test_app_shell.cpp CMakeLists.txt
git commit -m "feat(win): console de debug no F12 e tooltip com nome e nivel"
```

---

## Fase 5: áudio

### Task 23: `ESP_I2S` e FreeRTOS sobre SDL

**Files:**
- Create: `hal/ESP_I2S.h`, `hal/i2s.cpp`, `hal/freertos_shim.h`, `hal/freertos/FreeRTOS.h`, `hal/freertos/task.h`, `hal/freertos/queue.h`
- Test: `tests/test_audio.cpp`

- [ ] **Step 1: Ler o uso real no upstream**

Run:
```bash
grep -nE 'I2S|i2s\.|xQueue|xTask|vTaskDelay|pdMS_TO_TICKS|portMAX_DELAY|audioSetEnabled|sfxPlay|playTone' upstream/audio.cpp | head -60
grep -nE 'void |bool |audioSetEnabled|sfxPlay' upstream/audio.h
```
Expected: as assinaturas de `I2SClass`, as chamadas de FreeRTOS e a API pública de `audio.h`. Use-as nos passos seguintes.

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_audio.cpp
#include "framework.h"
#include "hal_control.h"
#include "audio.h"
#include <SDL3/SDL.h>

TEST(audio, som_habilitado_escreve_bytes) {
  SDL_InitSubSystem(SDL_INIT_AUDIO);
  audioBegin();
  audioSetEnabled(true);
  uint64_t antes = halAudioBytesWritten();
  sfxPlay(SFX_TAP);
  for (int i = 0; i < 20 && halAudioBytesWritten() == antes; ++i) SDL_Delay(10);
  ASSERT_TRUE(halAudioBytesWritten() > antes);
}

TEST(audio, som_desabilitado_nao_escreve) {
  audioSetEnabled(false);
  SDL_Delay(50);
  uint64_t antes = halAudioBytesWritten();
  sfxPlay(SFX_TAP);
  SDL_Delay(200);
  ASSERT_EQ((int)(halAudioBytesWritten() - antes), 0);
}

TEST(audio, fila_do_shim_entrega_na_ordem) {
  #include "freertos_shim.h"
  QueueHandle_t q = xQueueCreate(4, sizeof(int));
  int a = 1, b = 2, lido = 0;
  ASSERT_EQ((int)xQueueSend(q, &a, 0), 1);
  ASSERT_EQ((int)xQueueSend(q, &b, 0), 1);
  ASSERT_EQ((int)xQueueReceive(q, &lido, 0), 1);
  ASSERT_EQ(lido, 1);
  ASSERT_EQ((int)xQueueReceive(q, &lido, 0), 1);
  ASSERT_EQ(lido, 2);
  ASSERT_EQ((int)xQueueReceive(q, &lido, 0), 0);   // vazia
  vQueueDelete(q);
}
```

Mova o `#include "freertos_shim.h"` para o topo do arquivo — um include dentro de função não compila. Ajuste `SFX_TAP`, `audioBegin` e `audioSetEnabled` aos nomes reais vistos no Step 1.

- [ ] **Step 3: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `ESP_I2S.h`/`freertos_shim.h` não existem.

- [ ] **Step 4: Escrever `hal/freertos_shim.h`**

```cpp
#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

// Shim de FreeRTOS sobre std::thread. Cobre so o que audio.cpp usa.
typedef void* TaskHandle_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configMAX_PRIORITIES 25

struct QueueDef {
  std::mutex mu;
  std::condition_variable cv;
  std::deque<std::vector<uint8_t>> items;
  size_t itemSize = 0;
  size_t capacity = 0;
};
typedef QueueDef* QueueHandle_t;

inline QueueHandle_t xQueueCreate(size_t len, size_t itemSize) {
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
    auto cheia = [q] { return q->items.size() < q->capacity; };
    if (waitTicks == portMAX_DELAY) q->cv.wait(lk, cheia);
    else if (!q->cv.wait_for(lk, std::chrono::milliseconds(waitTicks), cheia)) return pdFAIL;
  }
  std::vector<uint8_t> buf(q->itemSize);
  std::memcpy(buf.data(), item, q->itemSize);
  q->items.push_back(std::move(buf));
  lk.unlock();
  q->cv.notify_one();
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
  q->cv.notify_one();
  return pdPASS;
}

inline BaseType_t xQueueReset(QueueHandle_t q) {
  if (!q) return pdFAIL;
  std::lock_guard<std::mutex> g(q->mu);
  q->items.clear();
  return pdPASS;
}

inline BaseType_t xTaskCreatePinnedToCore(void (*fn)(void*), const char*, uint32_t,
                                          void* arg, uint32_t, TaskHandle_t* handle,
                                          int /*core*/) {
  std::thread(fn, arg).detach();
  if (handle) *handle = nullptr;
  return pdPASS;
}
inline BaseType_t xTaskCreate(void (*fn)(void*), const char* n, uint32_t s,
                              void* arg, uint32_t p, TaskHandle_t* h) {
  return xTaskCreatePinnedToCore(fn, n, s, arg, p, h, 0);
}
inline void vTaskDelay(TickType_t ticks) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}
inline void vTaskDelete(TaskHandle_t) {}
inline TickType_t xTaskGetTickCount() {
  using namespace std::chrono;
  return (TickType_t)duration_cast<milliseconds>(
      steady_clock::now().time_since_epoch()).count();
}
```

E três reexportadores, para o caso de o upstream incluir pelos caminhos do FreeRTOS:

```cpp
// hal/freertos/FreeRTOS.h
#pragma once
#include "../freertos_shim.h"
```
```cpp
// hal/freertos/task.h
#pragma once
#include "../freertos_shim.h"
```
```cpp
// hal/freertos/queue.h
#pragma once
#include "../freertos_shim.h"
```

- [ ] **Step 5: Escrever `hal/ESP_I2S.h`**

```cpp
#pragma once
#include <cstddef>
#include <cstdint>

enum i2s_mode_t { I2S_MODE_STD = 0 };
enum i2s_data_bit_width_t { I2S_DATA_BIT_WIDTH_16BIT = 16 };
enum i2s_slot_mode_t { I2S_SLOT_MODE_MONO = 1, I2S_SLOT_MODE_STEREO = 2 };

class I2SClass {
 public:
  void setPins(int bclk, int ws, int dout, int din = -1, int mclk = -1) {
    (void)bclk; (void)ws; (void)dout; (void)din; (void)mclk;
  }
  bool begin(i2s_mode_t mode, uint32_t rate, i2s_data_bit_width_t bits,
             i2s_slot_mode_t slots);
  void end();
  size_t write(const uint8_t* data, size_t len);
  size_t write(uint8_t b) { return write(&b, 1); }
};
```

Ajuste os nomes de enum aos reais usados em `upstream/audio.cpp` (Step 1).

- [ ] **Step 6: Escrever `hal/i2s.cpp`**

```cpp
#include "ESP_I2S.h"
#include "hal_control.h"
#include <SDL3/SDL.h>
#include <atomic>

namespace {
SDL_AudioStream* gStream = nullptr;
std::atomic<uint64_t> gBytes{0};
int gRate = 16000, gChannels = 2;
}  // namespace

uint64_t halAudioBytesWritten() { return gBytes.load(); }

bool I2SClass::begin(i2s_mode_t, uint32_t rate, i2s_data_bit_width_t bits,
                     i2s_slot_mode_t slots) {
  gRate = (int)rate;
  gChannels = (slots == I2S_SLOT_MODE_MONO) ? 1 : 2;
  SDL_AudioSpec spec{};
  spec.format = (bits == I2S_DATA_BIT_WIDTH_16BIT) ? SDL_AUDIO_S16 : SDL_AUDIO_S16;
  spec.channels = gChannels;
  spec.freq = gRate;
  gStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                      &spec, nullptr, nullptr);
  if (!gStream) return false;                 // sem audio: o jogo segue mudo
  SDL_ResumeAudioStreamDevice(gStream);
  return true;
}

void I2SClass::end() {
  if (gStream) { SDL_DestroyAudioStream(gStream); gStream = nullptr; }
}

size_t I2SClass::write(const uint8_t* data, size_t len) {
  if (!gStream || !data || len == 0) return 0;

  // Segura o produtor quando a fila passa de ~100 ms, para o playTone do
  // upstream manter o ritmo em vez de encher a memoria.
  const int bytesPorSegundo = gRate * gChannels * 2;
  const int limite = bytesPorSegundo / 10;
  for (int espera = 0; espera < 50; ++espera) {
    if (SDL_GetAudioStreamQueued(gStream) <= limite) break;
    SDL_Delay(5);
  }

  if (!SDL_PutAudioStreamData(gStream, data, (int)len)) return 0;
  gBytes.fetch_add(len);
  return len;
}
```

- [ ] **Step 7: Aplicar a configuração de som da bandeja**

O item "Som" da bandeja grava `settings.soundEnabled`. O host aplica no upstream. Em `src/game_host.h` e `.cpp`:

```cpp
// game_host.h, dentro de IGameHost:
  virtual void setSoundEnabled(bool on) = 0;
```
```cpp
// game_host.cpp, dentro de GameHost:
  void setSoundEnabled(bool on) override { if (booted_) audioSetEnabled(on); }
```
Com `#include "audio.h"` no topo e, no `HostFalso` dos testes de `app_shell`, um `void setSoundEnabled(bool) override {}`. Em `trayInit`, aplique o valor inicial e, em `onSom`, chame `gHost->setSoundEnabled(...)` — passe o host para `trayInit` junto do `Settings`.

- [ ] **Step 8: Rodar e ver passar**

Run:
```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```
Expected: PASS nos 3 testes de `audio`. Se a máquina de CI não tiver dispositivo de áudio, `begin()` devolve false e os dois primeiros testes falham — nesse caso marque-os para pular quando `SDL_OpenAudioDeviceStream` retornar nullptr, com uma mensagem explícita (`SKIP: sem dispositivo de audio`), nunca com um assert frouxo.

- [ ] **Step 9: Teste manual**

Run: `./build/Debug/tamapoke.exe`
Expected: jingle de arranque toca; alimentar e evoluir têm som; desmarcar "Som" na bandeja silencia na hora.

- [ ] **Step 10: Commit**

```bash
git add hal/ESP_I2S.h hal/i2s.cpp hal/freertos_shim.h hal/freertos \
        src/game_host.h src/game_host.cpp src/win/tray.cpp tests/test_audio.cpp
git commit -m "feat(hal): audio por SDL_AudioStream e shim de FreeRTOS"
```

---

## Fase 6: idiomas CJK

### Task 24: Fontes U8g2 para japonês e coreano

**Files:**
- Create: `hal/U8g2lib.h`, `hal/u8g2_fonts.c`
- Test: `tests/test_fonts.cpp`

- [ ] **Step 1: Ver quais fontes o upstream usa**

Run:
```bash
grep -nE 'u8g2_font|setFont|unifont' upstream/TamaPoke.ino | sort -u
```
Expected: os nomes exatos (o plano assume `u8g2_font_unifont_t_japanese3` e `u8g2_font_unifont_t_korean2`).

- [ ] **Step 2: Escrever o teste falhando**

```cpp
// tests/test_fonts.cpp
#include "framework.h"
#include "Arduino_GFX_Library.h"
#include "U8g2lib.h"
#include "hal_control.h"

TEST(fonts, arrays_de_fonte_nao_estao_vazios) {
  ASSERT_TRUE(u8g2_font_unifont_t_japanese3[0] != 0);
  ASSERT_TRUE(u8g2_font_unifont_t_korean2[0] != 0);
}

TEST(fonts, texto_latino_com_fonte_u8g2_tem_largura_plausivel) {
  extern Arduino_Canvas* testCanvas();     // helper do test_canvas.cpp
  Arduino_Canvas* g = testCanvas();
  g->setFont(u8g2_font_unifont_t_japanese3);
  g->setUTF8Print(true);
  int16_t x1, y1; uint16_t w, h;
  g->getTextBounds("AB", 0, 0, &x1, &y1, &w, &h);
  ASSERT_TRUE(w >= 12 && w <= 20);          // unifont: ~8 px por char latino
  ASSERT_TRUE(h >= 10);
}

TEST(fonts, kana_tem_largura_dupla) {
  extern Arduino_Canvas* testCanvas();
  Arduino_Canvas* g = testCanvas();
  g->setFont(u8g2_font_unifont_t_japanese3);
  g->setUTF8Print(true);
  int16_t x1, y1; uint16_t wLatino, wKana, h;
  g->getTextBounds("A", 0, 0, &x1, &y1, &wLatino, &h);
  g->getTextBounds(u8"あ", 0, 0, &x1, &y1, &wKana, &h);  // あ
  ASSERT_TRUE(wKana > wLatino);
}

TEST(fonts, desenhar_kana_acende_pixels) {
  extern Arduino_Canvas* testCanvas();
  Arduino_Canvas* g = testCanvas();
  g->fillScreen(0x0000);
  g->setFont(u8g2_font_unifont_t_japanese3);
  g->setUTF8Print(true);
  g->setTextColor(0xFFFF);
  g->setCursor(10, 30);
  g->print(u8"あ");
  g->flush();
  int acesos = 0;
  for (int y = 10; y < 40; ++y)
    for (int x = 5; x < 35; ++x)
      if (halFrame()[(size_t)y * 466 + x] != 0) ++acesos;
  ASSERT_TRUE(acesos > 10);
}
```

Acrescente o helper `Arduino_Canvas* testCanvas()` a `tests/test_canvas.cpp` (não `static`), devolvendo o mesmo canvas usado lá.

- [ ] **Step 3: Rodar e ver falhar**

Run: `cmake --build build --config Debug --target tamapoke_tests`
Expected: FALHA — `U8g2lib.h` não existe.

- [ ] **Step 4: Escrever `hal/U8g2lib.h`**

```cpp
#pragma once
#include <cstdint>

// Arrays de fonte copiados de olikraus/u8g2 (csrc/u8g2_fonts.c, licenca BSD).
#ifdef __cplusplus
extern "C" {
#endif
extern const uint8_t u8g2_font_unifont_t_japanese3[];
extern const uint8_t u8g2_font_unifont_t_korean2[];
#ifdef __cplusplus
}
#endif
```

- [ ] **Step 5: Copiar os arrays de fonte**

Baixe `https://raw.githubusercontent.com/olikraus/u8g2/master/csrc/u8g2_fonts.c` para uma pasta vazia própria e extraia **apenas** os dois arrays nomeados no Step 1 para `hal/u8g2_fonts.c`, mantendo o cabeçalho de licença BSD do u8g2 no topo do arquivo. Os dois arrays juntos passam de 1 MB de fonte — é esperado.

Run:
```bash
grep -c . hal/u8g2_fonts.c
```
Expected: milhares de linhas. Acrescente ao CMake:
```cmake
set_source_files_properties(hal/u8g2_fonts.c PROPERTIES LANGUAGE C)
```

- [ ] **Step 6A: Se a Task 10 escolheu o Arduino_GFX real**

Nada mais a fazer: o decodificador de fontes U8g2 já vem no núcleo do Arduino_GFX, e `setFont` aceita o array direto.

Run: `ctest --test-dir build -C Debug -R tamapoke --output-on-failure`
Expected: PASS nos 4 testes de `fonts`.

- [ ] **Step 6B: Se a Task 10 escolheu o canvas próprio**

Porte `u8g2_font_decode` para `hal/canvas.cpp`: leia o cabeçalho de 23 bytes da fonte, decodifique os glifos com o esquema RLE do u8g2, e use-o em `drawChar`/`write`/`getTextBounds` quando `font_ != nullptr`. Decodifique UTF-8 em `write()` quando `utf8_` estiver ligado, acumulando os bytes de continuação até formar o code point.

Os 4 testes do Step 2 são o critério de pronto. Se o decodificador ficar maior que ~150 linhas, mova-o para `hal/u8g2_decode.cpp` com seu próprio header.

- [ ] **Step 7: Teste manual**

Run: `./build/Debug/tamapoke.exe`
Expected: deslizar para baixo → trocar idioma para Japonês e Coreano; nomes aparecem corretos (フシギダネ, 이상해씨) e os 8 idiomas renderizam sem caixinhas vazias.

- [ ] **Step 8: Commit**

```bash
git add hal/U8g2lib.h hal/u8g2_fonts.c tests/test_fonts.cpp tests/test_canvas.cpp CMakeLists.txt
git commit -m "feat(hal): fontes U8g2 para japones e coreano"
```

---

## Fase 7: empacotamento e release

### Task 25: Ícone, recursos e build Release

**Files:**
- Create: `assets/tamapoke.ico`, `assets/app.rc`, `CREDITS.md`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Criar o ícone**

Gere `assets/tamapoke.ico` com tamanhos 16, 32, 48 e 256 px a partir de um sprite do upstream (por exemplo o Pikachu de `tools/sdcard/mons/p025.bin`) ou de um desenho próprio. O arquivo precisa existir antes do Step 2.

- [ ] **Step 2: Escrever `assets/app.rc`**

```rc
#include <windows.h>

1 ICON "tamapoke.ico"

VS_VERSION_INFO VERSIONINFO
FILEVERSION 0,1,0,0
PRODUCTVERSION 0,1,0,0
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "FileDescription", "TamaPoke"
      VALUE "FileVersion", "0.1.0.0"
      VALUE "ProductName", "TamaPoke para Windows"
      VALUE "LegalCopyright", "Codigo MIT. Sprites CC BY-NC."
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
```

- [ ] **Step 3: Escrever `CREDITS.md`**

```markdown
# Créditos e licenças

## TamaPoke (firmware)
https://github.com/socquique/TamaPoke — licença MIT. Compilado aqui sem alterações,
como submódulo git em `upstream/`.

## Sprites dos Pokémon
Licença **CC BY-NC**: uso não comercial, com atribuição. Distribuídos em `sdcard/mons`.
Pokémon é marca registrada da Nintendo / Creatures Inc. / GAME FREAK inc.
Este projeto não é afiliado a eles.

## Bibliotecas
- SDL3 — https://github.com/libsdl-org/SDL — Zlib
- nlohmann/json — https://github.com/nlohmann/json — MIT
- Arduino_GFX — https://github.com/moononournation/Arduino_GFX — BSD
  (ou os algoritmos do Adafruit_GFX, BSD, conforme `docs/decisions/0001-canvas.md`)
- u8g2 (arrays de fonte) — https://github.com/olikraus/u8g2 — BSD

## Este port
Código de `src/`, `hal/` e `tools/` — MIT.
```

- [ ] **Step 4: Adicionar os recursos e a configuração Release ao CMake**

```cmake
if (MSVC)
  target_sources(tamapoke PRIVATE assets/app.rc)
  set_property(TARGET tamapoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_link_options(tamapoke PRIVATE "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>")
endif()
```

`SDL_SHARED ON` significa `SDL3.dll` ao lado do exe; o instalador leva as duas coisas.

- [ ] **Step 5: Compilar Release e rodar**

Run:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/Release/tamapoke.exe
```
Expected: testes verdes; o exe roda com ícone próprio; sem janela de console.

- [ ] **Step 6: Commit**

```bash
git add assets/tamapoke.ico assets/app.rc CREDITS.md CMakeLists.txt
git commit -m "chore: icone, recursos de versao e creditos"
```

### Task 26: Instalador Inno Setup e release no CI

**Files:**
- Create: `installer/tamapoke.iss`
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Escrever `installer/tamapoke.iss`**

```iss
#define MyAppName "TamaPoke"
#define MyAppVersion "0.1.0"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={localappdata}\Programs\TamaPoke
DefaultGroupName=TamaPoke
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=TamaPokeSetup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
SetupIconFile=..\assets\tamapoke.ico

[Files]
Source: "..\build\Release\tamapoke.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\SDL3.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\sdcard\mons\*"; DestDir: "{app}\sdcard\mons"; Flags: ignoreversion recursesubdirs
Source: "..\LICENSE";    DestDir: "{app}"; Flags: ignoreversion
Source: "..\CREDITS.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\TamaPoke"; Filename: "{app}\tamapoke.exe"
Name: "{userdesktop}\TamaPoke"; Filename: "{app}\tamapoke.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na area de trabalho"
Name: "autostart"; Description: "Iniciar o TamaPoke com o Windows"; GroupDescription: "Opcoes:"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
  ValueType: string; ValueName: "TamaPoke"; \
  ValueData: """{app}\tamapoke.exe"" --hidden"; \
  Flags: uninsdeletevalue; Tasks: autostart

[Messages]
WelcomeLabel2=Este instalador inclui sprites sob licenca CC BY-NC: uso nao comercial, com atribuicao. Veja CREDITS.md.

[Run]
Filename: "{app}\tamapoke.exe"; Description: "Abrir o TamaPoke"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\sdcard"
```

O save em `%APPDATA%\TamaPoke` **não** é removido na desinstalação de propósito: o bicho sobrevive a uma reinstalação. Diga isso no `README.md`.

- [ ] **Step 2: Criar o `LICENSE`**

Escreva o texto da licença MIT com o seu nome e o ano. O `.iss` referencia este arquivo e falha sem ele.

- [ ] **Step 3: Adicionar o job de release ao CI**

```yaml
  release:
    needs: windows
    if: startsWith(github.ref, 'refs/tags/v')
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Configure
        run: cmake -B build -G "Visual Studio 17 2022" -A x64
      - name: Build Release
        run: cmake --build build --config Release
      - name: Build installer
        run: iscc installer\tamapoke.iss
      - uses: actions/upload-artifact@v4
        with:
          name: TamaPokeSetup
          path: dist/*.exe
```

`iscc` já vem na imagem `windows-latest`. Se não vier, acrescente um passo com `choco install innosetup -y`.

- [ ] **Step 4: Gerar o instalador localmente e testar**

Run:
```bash
cmake --build build --config Release
iscc installer/tamapoke.iss
ls dist/
```
Expected: `dist/TamaPokeSetup-0.1.0.exe`. Instale numa máquina (ou usuário) limpa, confira que o app abre, que os sprites aparecem e que marcar a opção de autostart cria a chave no registro.

- [ ] **Step 5: Commit**

```bash
git add installer/tamapoke.iss LICENSE .github/workflows/ci.yml README.md
git commit -m "chore: instalador Inno Setup e job de release no CI"
```

### Task 27: Verificação final

**Use `superpowers:verification-before-completion`. Nenhuma afirmação de "pronto" sem a saída do comando colada.**

- [ ] **Step 1: Suíte completa**

Run:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python -m pytest tests/test_gen_protos.py -v
```
Expected: `upstream` e `tamapoke` verdes; pytest verde. Cole a saída.

- [ ] **Step 2: Checklist manual do jogo**

Marque cada um ao verificar no exe Release:

- [ ] escolher o inicial
- [ ] chocar o ovo com 3 toques
- [ ] alimentar
- [ ] minijogo
- [ ] saco
- [ ] banho
- [ ] dormir
- [ ] Pokédex
- [ ] ficha nas 4 páginas
- [ ] renomear
- [ ] os 8 idiomas, incluindo japonês e coreano
- [ ] som on/off pela bandeja

- [ ] **Step 3: Checklist manual do shell**

- [ ] janela redonda, sem borda, sempre no topo
- [ ] fora do Alt-Tab e da barra de tarefas
- [ ] botão esquerdo = toque; direito arrastando = move; direito seco = menu
- [ ] clicar fora do círculo acerta a janela de trás
- [ ] F2 liga/desliga a tela
- [ ] Esc não fecha nada
- [ ] fechar pelo X esconde para a bandeja; o bicho continua vivo
- [ ] "Mostrar/Esconder" e "Sair" funcionam
- [ ] escala 0,5× / 1× / 2×, com a silhueta acompanhando
- [ ] 100% e 150% de DPI
- [ ] posição e escala restauradas ao reabrir
- [ ] abrir uma segunda instância só faz a primeira aparecer
- [ ] desmarcar "Iniciar com o Windows" remove a chave do registro (`reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v TamaPoke`)
- [ ] reiniciar o Windows e ver o app subir **escondido**

- [ ] **Step 4: Checklist de persistência e tempo**

- [ ] fechar e reabrir: save mantido
- [ ] logoff com o app aberto: save intacto
- [ ] `TIME <epoch>` no console (`--console`) mostra a progressão offline
- [ ] matar o processo durante a gravação não corrompe o `nvs.json` (repetir 5 vezes)
- [ ] apagar o `nvs.json` à mão: o jogo recomeça limpo, sem travar
- [ ] corromper o `nvs.json` à mão: vira `nvs.json.bad-<epoch>` e o jogo abre

- [ ] **Step 5: Estabilidade**

- [ ] 24 h rodando, boa parte em `Hidden`, com `HEALTH` no console de hora em hora
- [ ] memória estável (Gerenciador de Tarefas: sem crescimento contínuo)
- [ ] CPU ocioso < 1% em `Hidden` (Gerenciador de Tarefas)
- [ ] `log.txt` rotacionou em 1 MB e não encheu o disco

- [ ] **Step 6: Commit final e tag**

```bash
git add -A
git commit -m "chore: verificacao final da v0.1.0"
git tag v0.1.0
git push --follow-tags
```
Expected: CI verde e o instalador publicado como artefato da tag.

---

## Notas de execução

**Ordem real de execução (diverge da numeração).** Dependências descobertas na execução: a Task 15 compila o jogo inteiro, que precisa de `audio.cpp` (FreeRTOS + I2S, Task 23) e das fontes CJK (Task 24). A ordem executada é: 1–14, **23, 24**, 15, 16, depois 17–22b, 25–27. A Task 11B nunca é executada (Task 10 decidiu pelo Caminho A). Os Steps 7 e 9 da Task 23 que tocam `src/game_host.*` e a bandeja migram para as Tasks 15 e 21, quando esses arquivos existirem.

**O que fazer quando o upstream não bate com o plano.** Este plano foi escrito sem o submódulo clonado, então toda assinatura de função do upstream é uma hipótese declarada: cada tarefa que depende de uma começa com um passo de `grep` que revela a assinatura real. A regra é sempre a mesma: **a HAL se ajusta ao upstream, nunca o contrário.** Se um `grep` revelar algo que invalida o desenho de uma tarefa (por exemplo, o upstream não ter um `pet.flushSave()`), ajuste a tarefa e diga isso no commit — não force o plano.

**Ordem das fases.** Fases 0 a 4 são sequenciais: cada uma depende da anterior compilar. Fases 5 (áudio) e 6 (CJK) são independentes entre si e podem ser trocadas de ordem ou feitas em paralelo. Fase 7 depende de todas.

**Onde o plano admite não ter cobertura.** O `AppShell` com SDL, a bandeja, a transparência e o registro não têm teste automatizado — só o checklist da Task 27. A lógica que **dá** para testar foi separada em funções puras (`TickPolicy`, `ShellStateMachine`, `windowModeHit`, `presentToLogical`, `presentInsideCircle`) justamente para que a parte não testada seja fina.
