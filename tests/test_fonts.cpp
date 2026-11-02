// Fontes U8g2 (japones/coreano) com o Arduino_Canvas REAL e a HAL.
#include <cstdio>
#include <set>
#include <string>
#include "framework.h"
#include "Arduino_GFX_Library.h"
#include "U8g2lib.h"
#include "hal_control.h"
#include "pin_config.h"
#include "i18n.h"
#include "pet.h"
#include "dex.h"

// Definidos em hal/u8g2_fonts.cpp (gerado).
extern const unsigned long u8g2_font_unifont_t_japanese3_size;
extern const unsigned long u8g2_font_unifont_t_korean2_size;

namespace {
// Um unico canvas 466x466 para todos os testes do arquivo (nunca liberado de
// proposito: evita ordem de destruicao no encerramento do processo).
Arduino_Canvas *canvas() {
  static Arduino_Canvas *g = [] {
    Arduino_DataBus *bus = new Arduino_ESP32QSPI(
      LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
    Arduino_CO5300 *panel = new Arduino_CO5300(
      bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
    Arduino_Canvas *c = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
    c->begin(80000000);
    return c;
  }();
  return g;
}

struct Bounds { int x1, y1, w, h; };
Bounds bounds(Arduino_Canvas *g, const char *s) {
  int16_t x1, y1;
  uint16_t w, h;
  g->setTextSize(1);
  g->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return {x1, y1, (int)w, (int)h};
}

// Desenha s em (50,100) com a fonte ja ativa e conta pixels acesos na janela.
int litPixels(Arduino_Canvas *g, const char *s) {
  g->fillScreen(0x0000);
  g->setTextSize(1);
  g->setTextColor(0xFFFF);
  g->setCursor(50, 100);
  g->print(s);
  g->flush();
  int n = 0;
  for (int y = 60; y < 130; ++y)
    for (int x = 40; x < 200; ++x)
      if (halFrame()[(size_t)y * 466 + x] != 0) ++n;
  return n;
}

void useJa(Arduino_Canvas *g) { g->setFont(u8g2_font_unifont_t_japanese3); g->setUTF8Print(true); }
void useKo(Arduino_Canvas *g) { g->setFont(u8g2_font_unifont_t_korean2); g->setUTF8Print(true); }

bool same(const uint8_t *p, std::initializer_list<int> want) {
  size_t i = 0;
  for (int b : want) if (p[i++] != (uint8_t)b) return false;
  return true;
}

// Code points nao ASCII de uma string UTF-8 (assume UTF-8 valido).
void collect(const char *s, std::set<uint32_t> &out) {
  if (!s) return;
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    uint32_t cp; int n;
    if (*p < 0x80) { ++p; continue; }
    else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; n = 1; }
    else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; n = 2; }
    else { cp = *p & 0x07; n = 3; }
    ++p;
    while (n-- && (*p & 0xC0) == 0x80) cp = (cp << 6) | (*p++ & 0x3F);
    out.insert(cp);
  }
}

std::string utf8(uint32_t cp) {
  std::string r;
  if (cp < 0x800) { r += (char)(0xC0 | (cp >> 6)); r += (char)(0x80 | (cp & 0x3F)); }
  else if (cp < 0x10000) { r += (char)(0xE0 | (cp >> 12)); r += (char)(0x80 | ((cp >> 6) & 0x3F)); r += (char)(0x80 | (cp & 0x3F)); }
  else { r += (char)(0xF0 | (cp >> 18)); r += (char)(0x80 | ((cp >> 12) & 0x3F)); r += (char)(0x80 | ((cp >> 6) & 0x3F)); r += (char)(0x80 | (cp & 0x3F)); }
  return r;
}

// Todos os code points nao ASCII que o firmware pode mostrar em `lang`.
std::set<uint32_t> firmwareCodepoints(Lang lang) {
  Lang antes = gLang;
  gLang = lang;
  std::set<uint32_t> cps;
  for (int i = 0; i < STR_COUNT; ++i) collect(T((StrId)i), cps);
  for (int i = 0; i < MED_COUNT; ++i) {
    collect(medalName(i), cps);
    collect(medalLabel(i), cps);
    collect(medalDesc(i), cps);
  }
  for (int d = 1; d <= DEX_COUNT; ++d) collect(dexName((int16_t)d), cps);
  gLang = antes;
  return cps;
}
}  // namespace

TEST(fonts, arrays_tem_cabecalho_e_cauda_do_u8g2) {
  // Impressao digital tirada do u8g2_fonts.c (tag 2.37.1), nao do gerador.
  ASSERT_EQ(u8g2_font_unifont_t_japanese3_size, 149411ul);
  ASSERT_EQ(u8g2_font_unifont_t_korean2_size, 77513ul);
  ASSERT_TRUE(same(u8g2_font_unifont_t_japanese3,
    {0x41,0x00,0x03,0x02,0x05,0x05,0x05,0x05,0x06,0x10,0x10,0x00,0xfe,0x0a,0xfe,0x0b}));
  ASSERT_TRUE(same(u8g2_font_unifont_t_korean2,
    {0x8e,0x00,0x04,0x02,0x05,0x05,0x04,0x05,0x06,0x10,0x10,0x00,0xfe,0x0a,0xfe,0x0b}));
  ASSERT_TRUE(same(u8g2_font_unifont_t_japanese3 + 149411 - 8,
    {0xa9,0x26,0xa7,0x71,0x9a,0x01,0x00,0x00}));
  ASSERT_TRUE(same(u8g2_font_unifont_t_korean2 + 77513 - 8,
    {0x64,0x24,0x99,0x19,0x5c,0x00,0x00,0x00}));
}

// getTextBounds() devolve a caixa da TINTA, nao o avanco. O avanco por glifo sai
// da diferenca entre medir dois glifos iguais e um so.
static int advance(Arduino_Canvas *g, const char *um, const char *dois) {
  return bounds(g, dois).w - bounds(g, um).w;
}

TEST(fonts, char_sem_sinal_como_no_esp32) {
  // O Arduino_GFX compara bytes UTF-8 como (c >= 0xE0) em getTextBounds().
  ASSERT_TRUE((char)0xE3 > 0);
}

TEST(fonts, japones_larguras_e_pixels) {
  Arduino_Canvas *g = canvas();
  useJa(g);
  Bounds a = bounds(g, "A");
  Bounds kana = bounds(g, u8"あ");   // medidos: tinta de A 6x11, de kana 13x14
  ASSERT_EQ(a.w, 6);
  ASSERT_EQ(a.h, 11);
  ASSERT_EQ(a.y1, -10);                // ancora na linha de base: y1 negativo
  ASSERT_EQ(kana.w, 13);
  ASSERT_EQ(kana.h, 14);
  ASSERT_TRUE(kana.w > a.w);
  ASSERT_EQ(advance(g, "A", "AA"), 8);                              // latino: 8 px
  ASSERT_EQ(advance(g, u8"あ", u8"ああ"), 16);          // kana: 16 px
  ASSERT_TRUE(litPixels(g, u8"あ") > 10);
}

TEST(fonts, coreano_larguras_e_pixels) {
  Arduino_Canvas *g = canvas();
  useKo(g);
  Bounds a = bounds(g, "A");
  Bounds hangul = bounds(g, u8"이");   // tinta de hangul 12x14
  ASSERT_EQ(a.w, 6);
  ASSERT_EQ(a.h, 11);
  ASSERT_EQ(hangul.w, 12);
  ASSERT_EQ(hangul.h, 14);
  ASSERT_EQ(advance(g, "A", "AA"), 8);
  ASSERT_EQ(advance(g, u8"이", u8"이이"), 16);
  ASSERT_TRUE(litPixels(g, u8"이") > 10);
}

TEST(fonts, a_fonte_do_idioma_errado_nao_tem_o_glifo) {
  // korean2 nao traz kana: confirma que o teste de pixels de verdade discrimina.
  Arduino_Canvas *g = canvas();
  useKo(g);
  int ko = litPixels(g, u8"あ");
  useJa(g);
  int ja = litPixels(g, u8"あ");
  ASSERT_TRUE(ja > 10);
  ASSERT_TRUE(ko < ja);
}

TEST(fonts, voltar_para_fonte_padrao_5x7) {
  Arduino_Canvas *g = canvas();
  useJa(g);
  ASSERT_EQ(advance(g, "A", "AA"), 8);
  g->setFont();
  g->setUTF8Print(false);
  ASSERT_EQ(bounds(g, "A").w, 6);
  ASSERT_EQ(bounds(g, "A").h, 8);
  ASSERT_EQ(advance(g, "A", "AA"), 6);
}

static void coverage(Lang lang, void (*use)(Arduino_Canvas *), const char *label) {
  Arduino_Canvas *g = canvas();
  std::set<uint32_t> cps = firmwareCodepoints(lang);
  use(g);
  int missing = 0;
  for (uint32_t cp : cps) {
    std::string s = utf8(cp);
    if (litPixels(g, s.c_str()) == 0) {
      ++missing;
      printf("  [%s] sem glifo: U+%04X\n", label, (unsigned)cp);
    }
  }
  printf("  [%s] %d code points nao ASCII verificados, %d ausentes\n", label, (int)cps.size(), missing);
  ASSERT_TRUE(cps.size() > 50);
  ASSERT_EQ(missing, 0);
}

TEST(fonts, cobertura_das_cadeias_japonesas_do_firmware) {
  coverage(LANG_JA, useJa, "ja");
}

TEST(fonts, cobertura_das_cadeias_coreanas_do_firmware) {
  coverage(LANG_KO, useKo, "ko");
}
