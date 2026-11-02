> **SUBSTITUIDO.** Este plano foi reescrito a partir do spec aprovado.
> Use `docs/superpowers/plans/2026-11-01-tamapoke-windows-desktop-pet.md`.
> Spec: `docs/superpowers/specs/2026-11-01-tamapoke-windows-port-design.md`.
> Mantido so como registro do rascunho inicial.

# TamaPoke para Windows: plano de implementação

> **Para o Claude:** SUB-SKILL OBRIGATÓRIA: use `superpowers:executing-plans` (ou `superpowers:subagent-driven-development`) para executar este plano tarefa por tarefa, com `superpowers:test-driven-development` em cada passo de código.

**Objetivo:** rodar o firmware do TamaPoke (https://github.com/socquique/TamaPoke) como um app nativo de Windows, com o mesmo visual, as mesmas regras e o mesmo save, **sem alterar o código do upstream**.

**Arquitetura:** o upstream entra como submódulo git em `upstream/` e seus `.cpp`/`.ino` são compilados **como estão**. Uma pasta `hal/` fornece versões de desktop de todos os headers de hardware que eles incluem (`Arduino.h`, `Wire.h`, `Preferences.h`, `SD_MMC.h`, `Arduino_GFX_Library.h`, `TouchDrvCSTXXX.hpp`, `ESP_I2S.h`, `SensorPCF85063.hpp`, `XPowersLib.h`, `esp_sleep.h`, `driver/gpio.h`, `U8g2lib.h`). Um `main.cpp` com SDL3 cria a janela, chama `setup()` uma vez e `loop()` a cada iteração, traduz mouse em toque e apresenta o framebuffer RGB565 de 466×466. Atualizar o upstream passa a ser `git submodule update` + recompilar.

**Stack:** C++17, CMake ≥ 3.24, MSVC 2022 (ou clang-cl), SDL3 via `FetchContent`, nlohmann/json (save), GitHub Actions `windows-latest`, Inno Setup para o instalador.

**Fora de escopo nesta versão:** batalhas/encontros (não existem no upstream), envio de sprites pela serial (o app lê do disco), Wasm/Tauri (fase posterior).

---

## Decisões já tomadas e premissas

- Stack escolhida: C++ + SDL3 com HAL (ver o doc de análise do projeto).
- **Premissa a confirmar:** a janela começa como janela normal 466×466 com escala 1×/2×. O modo "desktop pet" (redonda, sem borda, sempre no topo) entra na Fase 6 atrás de uma opção; se o Robson preferir esse modo como padrão, só muda o valor default da Tarefa 6.2.
- Save em `%APPDATA%\TamaPoke\nvs.json`. Sprites em `<pasta do exe>\sdcard\mons\` (copiados de `upstream/tools/sdcard/mons`, 303 arquivos, ~41 MB, incluindo `thumbs.bin`).
- Hardware sem equivalente vira stub previsível: bateria = -1 (sem bateria), `usbPresent()` = true (nunca desliga por bateria), brilho aplicado como escurecimento na hora de apresentar o frame.
- Licença: código MIT; sprites CC BY-NC. O repositório e o instalador precisam levar `CREDITS.md` e o aviso de uso não comercial.

## Mapa do que o upstream usa (alvo da HAL)

| Upstream chama | Onde | Implementação desktop |
|---|---|---|
| `millis()`, `delay()`, `random()`, `randomSeed()`, `String`, `Serial`, `ESP.getFreeHeap()`, `ESP.restart()`, `ps_malloc`, `IRAM_ATTR`, `pinMode`, `attachInterrupt`, `digitalPinToInterrupt`, `digitalWrite` | todos | `hal/Arduino.h` + `hal/arduino.cpp` |
| FreeRTOS: `xQueueCreate/Send/Receive`, `xTaskCreatePinnedToCore`, `vTaskDelay`, `pdMS_TO_TICKS`, `portMAX_DELAY` | `audio.cpp` | `hal/freertos_shim.h` com `std::thread` + fila com `std::condition_variable` |
| `Preferences` | `pet.cpp`, `i18n.cpp`, `audio.cpp` | `hal/Preferences.h` gravando JSON |
| `SD_MMC`, `File`, `FS.h` | `sdmon.cpp` | `hal/SD_MMC.h` sobre `std::FILE*` com raiz configurável |
| `Wire` | `TamaPoke.ino`, `rtcbat.cpp`, `audio.cpp` | no-op; `endTransmission()` retorna 0 |
| `Arduino_ESP32QSPI`, `Arduino_CO5300`, `Arduino_Canvas` (+ `setFont`, `setUTF8Print`, `getTextBounds`, ~20 primitivas) | `TamaPoke.ino` | ver Fase 2 (spike decide entre compilar o núcleo real do Arduino_GFX ou implementar um canvas próprio) |
| `TouchDrvCST92xx` | `TamaPoke.ino` | estado do mouse vindo do SDL |
| `SensorPCF85063`, `XPowersAXP2101` | `rtcbat.cpp` | RTC = `time(nullptr)`; PMU = stubs |
| `ESP_I2S` (`I2SClass::write`) | `audio.cpp` | `SDL_AudioStream` a 16 kHz estéreo 16 bits |
| `esp_light_sleep_start`, `esp_sleep_enable_*`, `gpio_wakeup_*` | `TamaPoke.ino` | dormir até o próximo evento do SDL ou o timeout |
| U8g2 (`unifont_t_japanese3`, `unifont_t_korean2`) | `TamaPoke.ino` | arrays de fonte copiados do U8g2 (Fase 5) |

---

## Fase 0: esqueleto do repositório

### Tarefa 0.1: repo, submódulo e CMake mínimo

**Arquivos:**
- Criar: `CMakeLists.txt`, `.gitignore`, `README.md`, `.gitmodules`
- Criar: `src/main.cpp` (só abre e fecha uma janela SDL)

**Passos:**
1. `git init`, `git submodule add https://github.com/socquique/TamaPoke upstream` e fixar num commit (anotar o hash no README).
2. `CMakeLists.txt` com `FetchContent` do SDL3 (tag de release fixa), alvo `tamapoke` (WIN32 executable) e alvo `tamapoke_tests` (console).
3. `src/main.cpp`: `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)`, janela 466×466 "TamaPoke", laço de eventos até `SDL_EVENT_QUIT`.
4. Rodar: `cmake -B build -G "Visual Studio 17 2022" && cmake --build build --config Debug` e abrir `build\Debug\tamapoke.exe`. Esperado: janela preta que fecha no X.
5. Commit: `chore: esqueleto CMake + SDL3 + submódulo upstream`.

### Tarefa 0.2: rodar a suíte de testes do upstream no Windows

Isto prova que a lógica compila com MSVC antes de mexer em qualquer coisa.

**Arquivos:**
- Modificar: `CMakeLists.txt` (alvo `upstream_tests` com `upstream/test/*.cpp`, `upstream/test/shim/shim.cpp`, `upstream/pet.cpp`, `upstream/i18n.cpp`, include `upstream/test/shim`)

**Passos:**
1. Adicionar o alvo e `add_test(NAME upstream COMMAND upstream_tests)`.
2. Rodar `ctest --test-dir build -C Debug --output-on-failure`. Esperado: tudo verde (os testes marcados `TEST_KNOWN_ISSUE` são informados à parte e não quebram).
3. Se o MSVC reclamar de algo (ex.: VLA, `__attribute__`, `min/max` de `windows.h`), corrigir **só com flags ou defines no CMake** (`NOMINMAX`, `/Zc:__cplusplus`, `/utf-8`). Nada de patch no upstream.
4. Commit: `test: suíte do upstream rodando com MSVC`.

### Tarefa 0.3: CI

**Arquivos:** Criar `.github/workflows/ci.yml`

**Passos:**
1. Job `windows-latest`: checkout com `submodules: recursive`, configure, build Release, `ctest`.
2. Commit e push; conferir o job verde.

---

## Fase 1: HAL base (sem tela)

Cada tarefa desta fase segue TDD com o micro-framework do upstream (`upstream/test/framework.h`, macros `TEST(suite, name)`). Os testes novos ficam em `tests/` e entram no alvo `tamapoke_tests`, que compila contra `hal/` (e não contra `upstream/test/shim`).

### Tarefa 1.1: `Arduino.h` mínimo + relógio

**Arquivos:**
- Criar: `hal/Arduino.h`, `hal/arduino.cpp`, `hal/WString.h` (classe `String` com o que o upstream usa: construtor de `const char*`, `c_str()`, `length()`, `trim()`, `startsWith()`, `substring()`, `toInt()`, `indexOf()`, `equalsIgnoreCase()`, `toUpperCase()`)
- Teste: `tests/test_hal_arduino.cpp`

**Passos:**
1. Teste falhando: `millis()` é monotônico e avança com `halAdvanceForTest(ms)`; `random(10)` fica em `[0,10)`; `String("  PUT x ").trim()` dá `"PUT x"`.
2. Rodar e ver falhar (não compila).
3. Implementar: `millis()` = `SDL_GetTicks()` truncado para `uint32_t` (com gancho de teste), `delay()` = `SDL_Delay`, `random` com `std::mt19937`, `ps_malloc` = `malloc`, `IRAM_ATTR` vazio, `ESP` com `getFreeHeap()` fixo e `restart()` que pede reinício ao `main`.
4. Antes de implementar `String`, rodar `grep -ohE '\bString\b[^;]*|\.(trim|startsWith|substring|toInt|indexOf|equalsIgnoreCase|toUpperCase|readStringUntil)\(' upstream/*.ino upstream/*.cpp | sort -u` e cobrir exatamente o que aparecer.
5. Rodar e ver passar. Commit: `feat(hal): Arduino.h, String e relógio`.

### Tarefa 1.2: `Serial` como console de debug

O upstream lê comandos com `Serial.available()` / `readStringUntil('\n')` (`TamaPoke.ino:436`) e `Serial.readBytes` (`sdmon.cpp:268`).

**Arquivos:** Criar `hal/HardwareSerial.h`, `hal/serial.cpp`. Teste: `tests/test_hal_serial.cpp`

**Passos:**
1. Teste falhando: `halSerialInject("STATS\n")` faz `Serial.available()` > 0 e `readStringUntil('\n')` devolver `"STATS"`; `printf` vai para o buffer de saída capturável.
2. Implementar fila thread-safe de entrada; saída para `stdout` e para um log em `%APPDATA%\TamaPoke\log.txt`. Métodos no-op: `begin`, `setRxBufferSize`, `setTxTimeoutMs`.
3. No `main` (Tarefa 3.1), uma thread lê `stdin` quando o app é iniciado com `--console` e injeta as linhas. Isso preserva os comandos `STATS`, `LVL`, `HATCH`, `WIPE` etc.
4. Passar, commit: `feat(hal): Serial com entrada injetável`.

### Tarefa 1.3: `Preferences` persistente em JSON

**Arquivos:** Criar `hal/Preferences.h`, `hal/preferences.cpp`. Teste: `tests/test_hal_prefs.cpp`

**Passos:**
1. Teste falhando: gravar `putUInt("ep", 123)`, `putBytes`, `putString("nick","Pika")` no namespace `"pet"`, destruir, recarregar do mesmo arquivo temporário e ler os mesmos valores; `clear()` apaga só aquele namespace; `isKey` e `remove` funcionam; `getString` respeita `maxLen`.
2. Implementar com a **mesma interface** de `upstream/test/shim/Preferences.h`, guardando tipos (para `getUInt` em chave gravada com `putUChar` devolver o default, como na NVS). `putBytes` vai em base64.
3. Gravação atômica: escrever `nvs.json.tmp` e `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING)`.
4. Caminho via `halSetDataDir()`; padrão `%APPDATA%\TamaPoke` (usar `SHGetKnownFolderPath(FOLDERID_RoamingAppData)`).
5. Passar, commit: `feat(hal): Preferences persistente em JSON`.

### Tarefa 1.4: `SD_MMC` e `File`

**Arquivos:** Criar `hal/FS.h`, `hal/SD_MMC.h`, `hal/sd.cpp`. Teste: `tests/test_hal_sd.cpp`

**Passos:**
1. Teste falhando: com raiz num diretório temporário contendo `mons/p001.bin`, `SD_MMC.open("/mons/p001.bin", FILE_READ)` é verdadeiro, `size()` bate e `read()` lê os bytes; arquivo inexistente dá `File` falso; `mkdir` e `cardSize` funcionam.
2. Implementar `File` com `operator bool`, `size`, `read(buf,n)`, `write`, `close`, `available`, `seek`, `name`, `isDirectory`, `openNextFile` (usado pelo `LS`). `begin(...)` retorna true se a raiz existe. `setPins` no-op.
3. Teste de integração: `PmdMon::load(25, false)` do upstream carregando o `p025.bin` real de `upstream/tools/sdcard/mons` e checando `pmd.loaded` e `pmd.has(PMD_IDLE)`.
4. Passar, commit: `feat(hal): SD_MMC sobre o sistema de arquivos`.

### Tarefa 1.5: stubs de hardware (Wire, GPIO, RTC, PMU, sleep)

**Arquivos:** Criar `hal/Wire.h`, `hal/esp_sleep.h`, `hal/driver/gpio.h`, `hal/SensorPCF85063.hpp`, `hal/XPowersLib.h`, `hal/stubs.cpp`. Teste: `tests/test_hal_rtc.cpp`

**Passos:**
1. Teste falhando: compilar `upstream/rtcbat.cpp` contra a HAL; `rtcEpoch()` fica a até 2 s de `time(nullptr)`; `batPercent()` = -1; `usbPresent()` = true; `batShouldShutdown()` = false.
2. Implementar o mínimo para `rtcbat.cpp` compilar sem patch: classes `SensorPCF85063` e `XPowersAXP2101` com os métodos que ele chama (levantar com `grep -oE '(rtc|pmu|PMU)\.[a-zA-Z]+' upstream/rtcbat.cpp | sort -u`). `rtcSetEpoch` grava um offset em memória (o relógio do Windows não é alterado).
3. `esp_light_sleep_start()` chama um gancho `halIdleWait(ms)` que o `main` liga a `SDL_WaitEventTimeout`.
4. `pwrShortPressed()` lê uma flag que o `main` liga à tecla **F2** (tela liga/desliga).
5. Passar, commit: `feat(hal): stubs de RTC, PMU, I2C e sleep`.

---

## Fase 2: tela

### Tarefa 2.1 (spike, máx. 1 dia): compilar o núcleo real do Arduino_GFX

Usar o código original garante desenho e texto pixel a pixel iguais, inclusive `getTextBounds` e fontes U8g2.

**Passos:**
1. Adicionar `moononournation/Arduino_GFX` como submódulo em `third_party/Arduino_GFX` (mesma versão que o CI do upstream instala; anotar).
2. Tentar compilar só `Arduino_GFX.cpp`, `Arduino_G.cpp`, `canvas/Arduino_Canvas.cpp`, `font/glcdfont.h` e o decodificador U8g2 embutido, com a HAL no include path e um `Print` mínimo em `hal/Print.h`.
3. **Critério de decisão:** se compilar com até ~10 defines/stubs, seguir com a Tarefa 2.2A. Se depender de `esp_lcd`, DMA ou `#ifdef ESP32` espalhado, seguir com a 2.2B.
4. Registrar a decisão em `docs/decisions/0001-canvas.md`. Commit.

### Tarefa 2.2A: painel falso para o Arduino_Canvas real

**Arquivos:** Criar `hal/Arduino_GFX_Library.h` (inclui o núcleo real e define `Arduino_ESP32QSPI` e `Arduino_CO5300` falsos), `hal/panel.cpp`. Teste: `tests/test_canvas.cpp`

**Passos:**
1. Teste falhando: `gfx->fillRect(10,10,4,4,0xF800); gfx->flush();` deixa `halFrame()[10*466+10] == 0xF800`; `panel->setBrightness(90)` deixa `halBrightness()==90`.
2. `Arduino_CO5300` falso herda de `Arduino_TFT`/`Arduino_GFX` e implementa `draw16bitRGBBitmap` copiando para um buffer global de 466×466 `uint16_t` protegido por mutex; `begin()` retorna true.
3. Passar, commit.

### Tarefa 2.2B (alternativa): canvas próprio compatível

Só se o spike falhar.

**Arquivos:** Criar `hal/Arduino_GFX_Library.h`, `hal/canvas.cpp`, `hal/glcdfont.c` (copiar o array 5×7 do Arduino_GFX/Adafruit_GFX, BSD). Teste: `tests/test_canvas.cpp`

**Passos:**
1. Levantar as primitivas usadas: `grep -oE 'gfx->[a-zA-Z_0-9]+' upstream/TamaPoke.ino | sort -u`.
2. Para cada primitiva, um teste com imagem de referência pequena (ex.: `fillCircle(5,5,3)` comparado com a saída esperada do algoritmo de Bresenham do Adafruit_GFX).
3. Implementar portando os algoritmos do Adafruit_GFX (mesmos arredondamentos). Texto: fonte 5×7 com avanço de 6 px × `textSize`, cursor e `getTextBounds` para a fonte clássica.
4. Fonte CJK fica para a Fase 5.
5. Passar, commit.

### Tarefa 2.3: apresentar o framebuffer no SDL

**Arquivos:** Criar `src/present.cpp`, `src/present.h`

**Passos:**
1. Textura `SDL_PIXELFORMAT_RGB565` 466×466, `SDL_SCALEMODE_NEAREST`; a cada frame novo, `SDL_UpdateTexture` + `SDL_RenderTexture` + `SDL_RenderPresent`.
2. Brilho: `SDL_SetTextureColorMod(tex, b, b, b)` com `b` mapeado de `setBrightness` (0..255). Brilho 0 = tela preta.
3. Máscara circular opcional (o AMOLED é redondo): desenhar o fundo fora do círculo de raio 233 em preto.
4. Verificação manual: um `main` de teste que desenha um gradiente e confirmar na tela.
5. Commit.

---

## Fase 3: o jogo rodando

### Tarefa 3.1: compilar o `TamaPoke.ino` e ligar `setup()`/`loop()`

**Arquivos:**
- Modificar: `CMakeLists.txt` (adicionar `upstream/TamaPoke.ino` com `set_source_files_properties(... LANGUAGE CXX)` e flag `/TP`, mais `upstream/pet.cpp`, `i18n.cpp`, `sdmon.cpp`, `rtcbat.cpp`, `audio.cpp`)
- Modificar: `src/main.cpp`

**Passos:**
1. O Arduino gera protótipos automaticamente; o compilador C++ não. Resolver **sem patch**: gerar `build/gen/TamaPoke_protos.h` num passo do CMake com um script Python (`tools/gen_protos.py`) que extrai as assinaturas de funções de nível superior do `.ino`, e incluí-lo com `/FI` (force include) só nesse arquivo. Teste do script: `tests/test_gen_protos.py` com um `.ino` de exemplo.
2. `main.cpp`: init SDL, `halSetDataDir`, `halSetSdRoot(<exe>/sdcard)`, `setup()`, depois laço: bombear eventos → `loop()` → se houve `flush()` novo, apresentar.
3. CMake copia `upstream/tools/sdcard/mons` para `<saída>/sdcard/mons` (comando `POST_BUILD`, só se mudou).
4. Rodar o exe. Esperado: tela de escolha do inicial (Bulbasaur/Charmander/Squirtle).
5. Commit: `feat: TamaPoke rodando no Windows (sem input)`.

### Tarefa 3.2: mouse vira toque

O upstream lê o toque assim (`TamaPoke.ino:547`): só consulta o chip se `gTouchIrq` foi setado pela ISR registrada em `attachInterrupt`, ou se o dedo ainda está pressionado; depois chama `touch.getPoint(&x,&y,1)`.

**Arquivos:** Criar `hal/TouchDrvCSTXXX.hpp`, `hal/touch.cpp`. Teste: `tests/test_touch.cpp`

**Passos:**
1. Teste falhando: `halTouchDown(100,200)` dispara a ISR registrada (seta `gTouchIrq`) e `getPoint` devolve 1 com (100,200); `halTouchUp()` faz `getPoint` devolver 0.
2. Implementar: `attachInterrupt` guarda o ponteiro da função; o `main` converte `SDL_EVENT_MOUSE_BUTTON_DOWN/MOTION/UP` para coordenadas lógicas (dividindo pela escala da janela) e chama `halTouchDown/Move/Up`, que invocam a ISR. `setMirrorXY`, `setMaxCoordinates`, `reset`, `setPins` são no-op; `begin` retorna true.
3. Teste manual dos gestos: clique no bicho = carinho; arrastar horizontal > 80 px em < 800 ms = Pokédex; arrastar para cima = ficha; para baixo = relógio/idioma; segurar 3 s = soltar.
4. Commit: `feat: mouse como touch`.

### Tarefa 3.3: teclado e atalhos

**Arquivos:** Modificar `src/main.cpp`

**Passos:**
1. **F2** = botão PWR curto (tela liga/desliga). **Setas** = swipes (para quem prefere teclado). **F12** = abre/fecha o console de debug.
2. `Esc` não fecha o app (evita perder o bicho por engano); fechar é pelo X ou bandeja.
3. Commit.

### Tarefa 3.4: tempo offline e save ao fechar

**Passos:**
1. Teste de integração (`tests/test_offline.cpp`): com a HAL, rodar `pet.begin()`, avançar o relógio do RTC falso em 2 h, chamar `pet.syncClock(rtcEpoch())` e checar que o nível subiu 2 (regra do upstream: +1 nível/hora, teto de 2 semanas).
2. No `SDL_EVENT_QUIT` e em `WM_ENDSESSION`, chamar `pet.flushSave()` antes de sair.
3. Commit.

---

## Fase 4: áudio

### Tarefa 4.1: `ESP_I2S` e FreeRTOS sobre SDL

**Arquivos:** Criar `hal/ESP_I2S.h`, `hal/i2s.cpp`, `hal/freertos_shim.h`. Teste: `tests/test_audio.cpp`

**Passos:**
1. Teste falhando: `sfxPlay(SFX_TAP)` com áudio habilitado faz `halAudioBytesWritten()` crescer em até 200 ms; com `audioSetEnabled(false)` não cresce.
2. `I2SClass::begin(mode, rate, bits, slots)` abre um `SDL_AudioStream` 16 kHz, S16, 2 canais; `write(buf, n)` faz `SDL_PutAudioStreamData` e, se a fila passar de ~100 ms, espera (para o `playTone` do upstream manter o ritmo). `setPins` no-op.
3. Fila e task: `xQueueCreate` → fila com mutex/condvar; `xTaskCreatePinnedToCore` → `std::thread` destacada; `vTaskDelay` → `sleep_for`.
4. O init do ES8311 via `Wire` vira no-op pelos stubs da Tarefa 1.5.
5. Teste manual: o jingle de arranque toca; comer/evoluir têm som. Commit.

---

## Fase 5: idiomas CJK

### Tarefa 5.1: fontes U8g2 para japonês e coreano

**Passos:**
1. Se a 2.2A foi adotada, só falta `hal/U8g2lib.h` expondo `u8g2_font_unifont_t_japanese3` e `u8g2_font_unifont_t_korean2` (arrays copiados de `olikraus/u8g2`, `csrc/u8g2_fonts.c`, licença BSD) e testar trocando o idioma.
2. Se foi a 2.2B, implementar o decodificador de fontes U8g2 no canvas próprio (portar `u8g2_font_decode` do Arduino_GFX) com testes de `getTextBounds` para "A" e "あ".
3. Teste manual: deslizar para baixo → idioma Japonês e Coreano, conferir textos e nomes (フシギダネ, 이상해씨).
4. Commit.

---

## Fase 6: experiência Windows

### Tarefa 6.1: ícone na bandeja e minimizar

**Passos:**
1. `SDL_CreateTray` com menu: Mostrar/Esconder, Som liga/desliga, Sempre no topo, Sair.
2. Fechar a janela esconde para a bandeja (o bicho continua vivo); "Sair" salva e encerra.
3. Commit.

### Tarefa 6.2: modo "desktop pet"

**Passos:**
1. Opção `petMode` (padrão **false**, ver premissa no topo) guardada em `settings.json`.
2. Ligado: janela `SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT | SDL_WINDOW_ALWAYS_ON_TOP`, pixels fora do círculo com alfa 0, arrastar a janela com o botão direito (o esquerdo continua sendo toque), escala 0,5×/1×/2× no menu da bandeja.
3. Teste manual em 100% e 150% de DPI. Commit.

### Tarefa 6.3: iniciar com o Windows (opcional)

**Passos:** item no menu da bandeja que cria/remove `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\TamaPoke`. Commit.

---

## Fase 7: empacotamento e release

### Tarefa 7.1: build Release e instalador

**Arquivos:** Criar `installer/tamapoke.iss`, `assets/tamapoke.ico`, `assets/app.rc`

**Passos:**
1. Release estático (`/MT`), SDL3 linkado estático ou `SDL3.dll` ao lado.
2. Inno Setup instala em `%LOCALAPPDATA%\Programs\TamaPoke`, com `sdcard\mons`, `LICENSE`, `CREDITS.md` e aviso CC BY-NC.
3. CI gera o `.exe` do instalador como artefato em tags `v*`.
4. Commit.

### Tarefa 7.2: verificação final

**Passos (usar `superpowers:verification-before-completion`):**
1. `ctest` verde (suíte do upstream + testes da HAL).
2. Checklist manual: escolher inicial, chocar ovo com 3 toques, alimentar, minijogo, saco, banho, dormir, Pokédex, ficha (4 páginas), renomear, trocar idioma para os 8, som on/off, fechar e reabrir (save mantido), adiantar o relógio com `TIME <epoch>` no console e ver a progressão offline.
3. Teste de 24 h rodando com `HEALTH` no console, observando memória.

---

## Riscos

- **Arduino_GFX não compilar fora do ESP32:** coberto pelo spike 2.1 com plano B (2.2B).
- **Protótipos do `.ino`:** o gerador pode errar com assinaturas incomuns; o teste do script e o build pegam isso cedo. Alternativa: um único patch mínimo mantido em `patches/` aplicado no CMake.
- **Mudanças futuras no upstream** podem pedir novos símbolos na HAL; o CI compilando contra o submódulo avisa na hora.
- **Timing:** o upstream assume ~10 fps e física do minijogo a 85 ms; `millis()` real do SDL preserva isso.
