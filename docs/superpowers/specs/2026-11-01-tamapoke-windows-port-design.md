# TamaPoke para Windows — spec de design

**Data:** 2026-11-01
**Status:** aprovado (brainstorming concluído com o Robson)
**Plano de implementação:** `docs/plans/2026-11-02-tamapoke-windows-port.md` (será reescrito a partir deste spec)

## 1. Objetivo

Rodar o firmware do TamaPoke (https://github.com/socquique/TamaPoke) como app nativo de
Windows, com o mesmo visual, as mesmas regras e o mesmo save, **sem alterar uma linha do
upstream**. O bicho vive como *desktop pet*: janela redonda sem borda na área de trabalho,
sempre no topo, que fica na bandeja e sobe junto com o logon.

## 2. Decisões tomadas

| Questão | Decisão |
|---|---|
| Modo padrão | Desktop pet + bandeja. Janela redonda, sem borda, sempre no topo, arrastável. Fechar esconde; o bicho continua vivo. |
| Autostart | Ligado por padrão. `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\TamaPoke`, desligável na bandeja. |
| Patches no upstream | **Zero.** Tudo resolvido na HAL, nos flags do CMake e no gerador de protótipos. Atualizar é `git submodule update` + recompilar. |
| Tick enquanto escondido | `loop()` a ~1 Hz, sem render. Progressão por hora vem do RTC. |
| Alertas de fome/sujeira | Só o visual na própria janela, mais som silenciável. Sem toast do Windows. |
| Shell vs. core | Separados por uma interface estreita (`IGameHost`). |

**Stack:** C++17, CMake ≥ 3.24, MSVC 2022 (ou clang-cl), SDL3 via `FetchContent` (tag de
release fixa), nlohmann/json para o save, GitHub Actions `windows-latest`, Inno Setup para
o instalador.

**Licença:** código MIT; sprites CC BY-NC. Repositório e instalador precisam levar
`CREDITS.md` e o aviso de uso não comercial.

## 3. Arquitetura

Três camadas, dependência só para baixo:

```
app_shell   Win32 + SDL: janela, bandeja, autostart, modo pet, escala, DPI, input bruto
    |  interface IGameHost
game_host   dono do ciclo: setup() uma vez, loop() por tick; dono do framebuffer
    |  chama direto
upstream/   TamaPoke.ino, pet.cpp, i18n.cpp, sdmon.cpp, rtcbat.cpp, audio.cpp (intocados)
    |  inclui
hal/        Arduino.h, Preferences.h, SD_MMC.h, Wire.h, ESP_I2S.h, GFX, touch, RTC/PMU, sleep
```

### 3.1 IGameHost — superfície completa

```cpp
struct IGameHost {
  virtual void boot() = 0;                    // halSetDataDir + halSetSdRoot + setup()
  virtual void tick() = 0;                    // um loop()
  virtual bool frameDirty() const = 0;        // houve flush() novo?
  virtual const uint16_t* frame() const = 0;  // 466*466 RGB565
  virtual uint8_t brightness() const = 0;
  virtual void touchDown(int x, int y) = 0;   // coords lógicas 0..465
  virtual void touchMove(int x, int y) = 0;
  virtual void touchUp() = 0;
  virtual void powerShortPress() = 0;         // F2 ou item da bandeja
  virtual void injectConsole(const char* line) = 0;
  virtual void save() = 0;                    // pet.flushSave()
  virtual bool wantsRestart() const = 0;      // upstream chamou ESP.restart()
  virtual void clearFrameDirty() = 0;
  virtual void setSoundEnabled(bool on) = 0;  // item "Som" da bandeja
  virtual const char* petName() const = 0;    // tooltip da bandeja
  virtual int petLevel() const = 0;
};
```

### 3.2 Invariantes de fronteira

- `app_shell` nunca inclui um header de `hal/` nem referencia símbolo do upstream.
- `game_host` nunca referencia `SDL_Window`, `HWND`, bandeja ou registro.
- Todo gancho de teste da HAL vive em **um** header, `hal/hal_control.h`:
  `halSetDataDir`, `halSetSdRoot`, `halAdvanceForTest`, `halIdleWait`, `halSerialInject`,
  `halTouchDown/Move/Up`, `halFrame`, `halBrightness`, `halAudioBytesWritten`.
- Nenhum arquivo de `src/` passa de ~300 linhas. Assunto novo vira arquivo em `src/win/`.

### 3.3 Arquivos

| Arquivo | Responsabilidade |
|---|---|
| `src/main.cpp` | ~40 linhas: monta shell + host, roda, sai |
| `src/app_shell.{h,cpp}` | loop de eventos, máquina de estados, input → touch lógico |
| `src/game_host.{h,cpp}` | implementa `IGameHost` |
| `src/present.{h,cpp}` | textura RGB565, escala, brilho, máscara circular, alfa |
| `src/win/tray.cpp` | bandeja e menu de contexto |
| `src/win/autostart.cpp` | chave `Run` do registro |
| `src/win/window_mode.cpp` | região elíptica (`SetWindowRgn`), hit-test, arrastar |
| `hal/` | 13 headers de hardware + `hal_control.h` |
| `tests/` | HAL e host headless |

## 4. Ciclo de vida e política de tick

Máquina de três estados no `app_shell`. O `game_host` só obedece.

| Estado | Como entra | `tick()` | Render | Áudio |
|---|---|---|---|---|
| `Visible` | janela na tela | contínuo: o firmware se auto-pacea (render a ~10 fps, 85 ms no minijogo; `delay(1)` entre passadas; toque lido a cada ~20 ms) | sim | sim |
| `Hidden` | fechou a janela, bandeja "Esconder", ou logon com `--hidden` | 1 Hz, mais um `tick()` imediato após qualquer entrada ou ao mostrar | não | sim |
| `ScreenOff` | F2 ou PWR curto | contínuo (o firmware já relaxa para `delay(20)` por passada e não renderiza) | janela preta (brilho 0) | sim |

- **`millis()` não desacelera.** Continua `SDL_GetTicks()` real. Em `Hidden` só chamamos
  `loop()` menos vezes; o upstream vê saltos de ~1000 ms, como já tolera quando a tela
  dorme no hardware. A física do minijogo a 85 ms só roda em `Visible`.
- **Progressão por hora vem do RTC** (`rtcEpoch()` = `time(nullptr)`), não do loop. Um só
  mecanismo cobre escondido, tela apagada e app fechado. Ao voltar para `Visible`, um
  `tick()` extra deixa o upstream ver o epoch novo; a regra +1 nível/hora com teto de duas
  semanas é dele.
- **Esperar de verdade:** em `Hidden`, `SDL_WaitEventTimeout` até o próximo evento ou 1 s.
  Em `Visible` e `ScreenOff` não há espera do shell: o `delay()` do próprio firmware (via `SDL_Delay`)
  segura a CPU. A cadência de 10 Hz do rascunho inicial foi **descartada**: o firmware foi escrito para
  o `loop()` girar continuamente e ler o toque a cada ~20 ms; chamá-lo a 10 Hz deixaria o toque e o
  arrasto com até 100 ms de latência.
- **Áudio toca escondido.** Silenciar é escolha explícita na bandeja, persistida via o
  `audioSetEnabled` do upstream.
- **Save** em: transição `Visible→Hidden`; a cada 5 min em `Hidden`; `SDL_EVENT_QUIT`;
  `WM_QUERYENDSESSION`/`WM_ENDSESSION`. Escrita atômica (`.tmp` + `MoveFileExW`), então
  logoff forçado ou kill no meio nunca corrompe o `nvs.json`.
- **Instância única:** mutex nomeado `Local\TamaPoke`. A segunda execução manda "mostrar"
  para a primeira e sai. Sem isso, autostart + atalho = dois bichos no mesmo save.
- **`wantsRestart()`:** shell salva, derruba o host, reconstrói e chama `boot()`. O
  processo não morre.

## 5. Janela, input e apresentação

**Flags:** `SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_UTILITY`
(sem `SDL_WINDOW_TRANSPARENT`: veja "Silhueta redonda"). `UTILITY` mantém o app fora do Alt-Tab e da barra de tarefas.
Escala 0,5× / 1× / 2×, default **1×** (466 px). Posição e escala em `settings.json`,
restauradas no logon.

**Silhueta redonda:** região elíptica (`SetWindowRgn`) do tamanho real da janela, aplicada na
criação e a cada mudança de escala. É o caminho **único** (decisão revista na execução): a região
recorta o que se vê e também o que recebe cliques, então o quadrado de 466 px fora do círculo
não rouba cliques do desktop. Alfa por pixel (`SDL_WINDOW_TRANSPARENT`) foi descartado como caminho
principal: uma janela com alfa por pixel continua capturando cliques nos pixels transparentes (o
Windows decide o hit-test pela região, não pelo alfa) e ainda depende de driver/compositor. Custo: a
borda do círculo fica sem antialias (1 px), irrelevante com preto fora dele.

**Hit-test:** cliques fora do círculo passam para a janela de trás — não só visualmente.
Sem isso, um quadrado invisível de 466 px rouba cliques do desktop.

| Gesto | Efeito |
|---|---|
| Botão esquerdo | toque (`touchDown/Move/Up`), coords divididas pela escala |
| Botão direito arrastando | move a janela; nunca vira toque |
| Botão direito clique seco | menu de contexto = menu da bandeja |
| Roda do mouse | nada (upstream não usa) |
| F2 | PWR curto → `ScreenOff` |
| Setas | swipes, para quem prefere teclado |
| F12 | console de debug (`STATS`, `LVL`, `HATCH`, `TIME <epoch>`, `WIPE`, ...) |
| Esc | nada. Não fecha o app. |

**Apresentação:** textura `SDL_PIXELFORMAT_RGB565` 466×466, `SDL_SCALEMODE_NEAREST`
(pixel art não pode borrar). Brilho por `SDL_SetTextureColorMod(b,b,b)`; brilho 0 = preto.
`SDL_WINDOW_HIGH_PIXEL_DENSITY` **desligado**: a escala é por fator inteiro nosso, mais
previsível em 150% de DPI do que a interpolação do Windows.

**Bandeja** (`SDL_CreateTray`): Mostrar/Esconder · Som · Sempre no topo · Escala
(0,5/1/2) · Iniciar com o Windows (marcado por padrão) · Sair. Tooltip com nome e nível.

**Autostart:** `HKCU\...\Run\TamaPoke` = `"<exe>" --hidden`, escrito no primeiro run e
pelo instalador, removível pela bandeja. `--hidden` nasce em `Hidden`, então o logon não
joga janela na cara de ninguém.

## 6. HAL

Cobre exatamente o que o upstream inclui. Um header por header de hardware:

| Upstream chama | Onde | Implementação desktop |
|---|---|---|
| `millis`, `delay`, `random`, `String`, `Serial`, `ESP.*`, `ps_malloc`, `IRAM_ATTR`, `pinMode`, `attachInterrupt`, `digitalWrite` | todos | `hal/Arduino.h` + `hal/arduino.cpp` + `hal/WString.h` |
| FreeRTOS (`xQueueCreate/Send/Receive`, `xTaskCreatePinnedToCore`, `vTaskDelay`) | `audio.cpp` | `hal/freertos_shim.h`: `std::thread` + fila com `condition_variable` |
| `Preferences` | `pet.cpp`, `i18n.cpp`, `audio.cpp` | `hal/Preferences.h` gravando JSON, com tipagem igual à NVS |
| `SD_MMC`, `File`, `FS.h` | `sdmon.cpp` | `hal/SD_MMC.h` sobre `std::FILE*`, raiz configurável |
| `Wire` | `.ino`, `rtcbat.cpp`, `audio.cpp` | no-op; `endTransmission()` = 0 |
| `Arduino_ESP32QSPI`, `Arduino_CO5300`, `Arduino_Canvas` | `.ino` | spike decide: núcleo real do Arduino_GFX ou canvas próprio |
| `TouchDrvCST92xx` | `.ino` | estado do mouse vindo do SDL, disparando a ISR registrada |
| `SensorPCF85063`, `XPowersAXP2101` | `rtcbat.cpp` | RTC = `time(nullptr)` + offset em memória; PMU = stub |
| `ESP_I2S` (`I2SClass::write`) | `audio.cpp` | `SDL_AudioStream` 16 kHz, S16, estéreo |
| `esp_light_sleep_start`, `gpio_wakeup_*` | `.ino` | `halIdleWait(ms)` → `SDL_WaitEventTimeout` |
| U8g2 (`unifont_t_japanese3`, `unifont_t_korean2`) | `.ino` | arrays de fonte copiados do U8g2 (BSD) |

**Stubs previsíveis:** bateria = -1 (sem bateria), `usbPresent()` = true (nunca desliga por
bateria), `batShouldShutdown()` = false, brilho aplicado na apresentação do frame.

**Dados:** save em `%APPDATA%\TamaPoke\nvs.json` (via `SHGetKnownFolderPath`), sprites em
`<pasta do exe>\sdcard\mons\` (303 arquivos, ~41 MB, copiados de
`upstream/tools/sdcard/mons`, incluindo `thumbs.bin`), log em `%APPDATA%\TamaPoke\log.txt`.

**Protótipos do `.ino`:** o Arduino gera automaticamente, o C++ não. `tools/gen_protos.py`
extrai as assinaturas de nível superior para `build/gen/TamaPoke_protos.h`, incluído com
`/FI` só nesse arquivo. O script tem teste próprio.

## 7. Estratégia de testes

**Nível 1 — suíte do upstream.** `upstream/test/*.cpp` com o shim do upstream, sem a HAL.
Prova que a lógica compila e passa no MSVC. É o canário: roda antes de tudo. Testes
marcados `TEST_KNOWN_ISSUE` são informados à parte e não quebram o build.

**Nível 2 — testes de HAL** (`tests/test_hal_*.cpp`), TDD tarefa por tarefa, com o
micro-framework do upstream (`TEST(suite, name)`):

- `Preferences`: round-trip por namespace, tipagem NVS (`getUInt` em chave gravada com
  `putUChar` devolve o default), `clear`/`isKey`/`remove`, `getString` respeitando `maxLen`.
- `SD_MMC`: abrir um `.bin` real de `upstream/tools/sdcard/mons`, `size()`, `read()`,
  arquivo inexistente dando `File` falso, `openNextFile`.
- `Serial`: `halSerialInject("STATS\n")` → `available()` > 0 e `readStringUntil('\n')`.
- RTC/PMU: `rtcEpoch()` a até 2 s de `time(nullptr)`; `batPercent()` = -1.
- Canvas: primitivas pixel a pixel contra a saída esperada dos algoritmos do Adafruit_GFX.
- Touch: `halTouchDown(100,200)` dispara a ISR e `getPoint` devolve 1 com (100,200).

**Nível 3 — host headless** (`tests/test_host_*.cpp`), sem janela nenhuma:

- `boot()` + N `tick()` e assert em `frameDirty()`.
- Sequência de toques escolhendo o inicial e chegando no estado esperado.
- Tempo offline: avançar o RTC em 2 h, `tick()`, checar +2 níveis.
- Save/reload preservando o bicho.

**Sem cobertura automatizada:** o `app_shell` (win32, bandeja, registro, transparência).
Verificação é o checklist manual da seção 9. Isso é declarado de propósito — ninguém deve
fingir cobertura que não existe.

## 8. Tratamento de erro

| Situação | Comportamento |
|---|---|
| SD ausente ou sprite faltando | HAL devolve `File` falso (o upstream já trata). Jogo abre sem sprites e loga. Sem exceção. |
| `nvs.json` corrompido ou de versão futura | Renomeia para `nvs.json.bad-<epoch>`, começa limpo, loga. Nunca trava no boot. |
| SDL sem áudio | Segue muda. Não é fatal. |
| SDL sem vídeo | Fatal: `MessageBoxW` com o motivo e sai. |
| `SetWindowRgn` falha | Loga o erro e segue com a janela retangular (nunca é fatal). |
| Log | `%APPDATA%\TamaPoke\log.txt`, rotação em 1 MB. |

## 9. Verificação manual (checklist)

Escolher o inicial · chocar o ovo com 3 toques · alimentar · minijogo · saco · banho ·
dormir · Pokédex · ficha nas 4 páginas · renomear · trocar para os 8 idiomas (incluindo
japonês e coreano) · som on/off · esconder e reaparecer pela bandeja · F2 liga/desliga a
tela · arrastar com o botão direito · clicar fora do círculo e acertar a janela de trás ·
100% e 150% de DPI · escala 0,5/1/2 · fechar e reabrir com o save mantido · `TIME <epoch>`
no console mostrando a progressão offline · logoff com o app aberto e save intacto ·
reiniciar o Windows e ver o app subir escondido · 24 h rodando com `HEALTH` no console,
observando memória.

## 10. Riscos

| Risco | Plano B |
|---|---|
| Arduino_GFX não compila fora do ESP32 | Spike de no máximo 1 dia decide entre o núcleo real e um canvas próprio portando o Adafruit_GFX. Decisão registrada em `docs/decisions/0001-canvas.md`. |
| Gerador de protótipos erra uma assinatura | Teste do script e o build pegam cedo. Fallback: header de protótipos escrito à mão e versionado — ainda zero patch no upstream. |
| Transparência per-pixel irregular por driver / captura de cliques | Não usada: `SetWindowRgn` é o caminho único. |
| `SDL_CreateTray` imaturo | `Shell_NotifyIconW` direto. `src/win/tray.cpp` já é isolado por isso. |
| Upstream muda e pede símbolo novo na HAL | O CI compila contra o submódulo e quebra na hora. |
| Timing | O upstream assume ~10 fps e física a 85 ms; `millis()` real do SDL preserva isso. |

## 11. Fora de escopo

Batalhas e encontros (não existem no upstream) · envio de sprites pela serial (o app lê do
disco) · toast do Windows · multi-bicho · sync em nuvem · Wasm/Tauri.
