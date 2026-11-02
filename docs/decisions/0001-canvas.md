# 0001 — Origem do canvas de desenho

**Data:** 2026-11-02
**Decisão:** Arduino_GFX real (Caminho A)

## Contexto
O upstream desenha com Arduino_GFX (`Arduino_ESP32QSPI` + `Arduino_CO5300` + `Arduino_Canvas`).
Reusar o núcleo original dá paridade pixel a pixel; reimplementar dá controle, com risco de divergência.

## O que o spike mostrou

**Versão.** O upstream (v1.18) não fixa versão: README e CREDITS só citam "GFX Library for Arduino"
(moononournation), sem CI nem lista de libs. Usamos a tag de release mais recente, **v1.6.9**
(`c2e39350ff09c3720702bd3f050cb581fa45a8e9`), como submódulo em `third_party/Arduino_GFX`.

**Superfície usada pelo `TamaPoke.ino`** (o que o núcleo precisa prover): `Arduino_Canvas(466,466,panel)`,
`begin`, `flush`, `fillScreen`, `fillRect`, `fillRoundRect`, `drawRoundRect`, `fillCircle`, `drawCircle`,
`fillTriangle`, `drawLine`, `setTextColor`, `setTextSize`, `setCursor`, `getCursorX/Y`, `print`,
`setFont` (glcd padrão e U8g2: `u8g2_font_unifont_t_japanese3`, `u8g2_font_unifont_t_korean2`),
`setUTF8Print`, `getTextBounds`; mais `panel->setBrightness` (método do CO5300, não do núcleo).
Includes: `Arduino_GFX_Library.h`, `U8g2lib.h`.

**Compilação.** O alvo `gfx_core` (`Arduino_GFX.cpp`, `Arduino_G.cpp`, `Arduino_DataBus.cpp`,
`canvas/Arduino_Canvas.cpp`) compila e gera `.lib` com **6 stubs/defines** (limite do plano: ~10), com o
caminho de fontes U8g2 (`U8G2_FONT_SUPPORT`) ligado. Todo o código ESP-IDF (`esp_lcd`, DMA, `soc/*`) fica
em `#if defined(ESP32)` no `Arduino_DataBus.h`; sem definir `ESP32`, nada disso é visto, e o canvas cai
no `malloc` simples (não há `#ifdef ESP32` espalhado nas classes que usamos).

| # | Stub | Motivo |
|---|------|--------|
| 1 | `hal/Print.h` (já previsto no plano) | `Arduino_GFX` herda de `Print` |
| 2 | `Print::print(const String&)` e `print(const __FlashStringHelper*)` (em `Print.h`) | `printCenterText` |
| 3 | `hal/pgmspace.h`: `PROGMEM`, `PSTR`, `pgm_read_byte/word/dword`, `__FlashStringHelper`, `F()` | glcd font e fontes; fora de AVR/ESP o núcleo não inclui `pgmspace.h` |
| 4 | `#define __attribute__(x)` sob `_MSC_VER` em `hal/Arduino.h` | `GFX_INLINE` usa `__attribute__((always_inline))` |
| 5 | `using std::min/max` em `hal/Arduino.h` | o núcleo usa `max()` do Arduino (macro lá) |
| 6 | `hal/U8g2lib.h` (só declarações das fontes) | `__has_include(<U8g2lib.h>)` liga `U8G2_FONT_SUPPORT` |

Erros encontrados (primeira linha de cada):
- `Arduino_GFX.cpp(146,1): error C2065: 'always_inline': identificador não declarado` (stub 4)
- `Arduino_GFX.cpp(2050,23): error C3861: 'pgm_read_glyph_ptr': identificador não encontrado` (consequência do mesmo `GFX_INLINE`)
- `Arduino_GFX.cpp(3075,3): error C2665: 'Print::print': nenhuma função sobrecarregada pode converter todos os tipos de argumento` (stub 2)
- `Arduino_GFX.cpp(1963,18): error C3861: 'max': identificador não encontrado` (só com U8g2 ligado; stub 5)
- (`PROGMEM`/`pgm_read_*`/`__FlashStringHelper` ausentes: stub 3)

Nenhum arquivo da biblioteca foi editado. Avisos restantes: truncamento double→float e
C4828 (bytes não-UTF-8 num cabeçalho de fonte); inofensivos.

**Prova funcional (sonda descartável, não commitada).** Uma subclasse de `Arduino_G` (painel falso, 466x466
RGB565) recebendo `Arduino_Canvas`: `begin()`, `fillCircle`, `print`, `getTextBounds("A")` (w=6, h=8, 5x7 glcd
com escala 1) e `flush()` rodaram; o `draw16bitRGBBitmap` do painel falso recebeu o framebuffer inteiro (pixel
central = 0xF800) e `getFramebuffer()` também o expõe.

**Perguntas do plano para o Caminho A:**
- (a) `Arduino_Canvas::flush()` chama `_output->draw16bitRGBBitmap(x, y, framebuffer, w, h)`, e
  `getFramebuffer()` é público. Basta o painel falso copiar para o buffer 466x466 RGB565.
- (b) O decodificador U8g2 (`u8g2_font_decode_*`, `setFont(const uint8_t*)`, `setUTF8Print`) vive dentro de
  `Arduino_GFX.cpp`, independente da lib U8g2; só os **dados** das fontes (`u8g2_font_unifont_t_japanese3`,
  `..._korean2`) vêm de `u8g2_fonts.c` da U8g2.

## Consequência
Seguimos a Task 11A. O painel falso herda de `Arduino_G` e implementa `begin` e os cinco `draw*Bitmap`
puramente virtuais (`drawBitmap`, `drawIndexedBitmap`, `draw3bitRGBBitmap`, `draw16bitRGBBitmap`,
`draw24bitRGBBitmap`); só o `draw16bitRGBBitmap` precisa fazer algo (copiar para o buffer). `Arduino_ESP32QSPI`
e `Arduino_CO5300` não são compilados: a HAL fornece um `Arduino_GFX_Library.h` próprio com classes de mesmo
nome (bus inerte; panel = `Arduino_G` falso com `setBrightness`). Fontes CJK na Fase 5 reusam o decodificador do
Arduino_GFX, mas exigem trazer os dados de fonte da U8g2 (`csrc/u8g2_fonts.c`, C puro): submódulo ou cópia
dos dois arrays, e a nossa `hal/U8g2lib.h` passa a declarar/incluir esses dados. Ponto de atenção:
`Arduino_GFX_Library.h` real não pode ser incluído (puxa todos os painéis e barramentos).
