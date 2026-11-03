// Arduino_Canvas REAL (third_party/Arduino_GFX) desenhando no painel falso da HAL.
#include "framework.h"
#include "Arduino_GFX_Library.h"
#include "hal_control.h"
#include "pin_config.h"  // upstream: LCD_CS, LCD_SCLK, LCD_SDIO0..3, LCD_RESET, LCD_WIDTH/HEIGHT

namespace {
// Copiado verbatim de upstream/TamaPoke.ino (linhas 34-39).
struct Rig {
  Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
  Arduino_CO5300 *panel = new Arduino_CO5300(
    bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
  Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);
  ~Rig() { delete gfx; delete panel; delete bus; }
};
inline int px(int x, int y) { return (int)halFrame()[y * 466 + x]; }
}  // namespace

TEST(canvas, fillRect_chega_no_framebuffer_depois_do_flush) {
  Rig r;
  ASSERT_TRUE(r.gfx->begin(80000000));
  r.gfx->fillScreen(0x0000);
  r.gfx->fillRect(10, 10, 4, 4, 0xF800);
  r.gfx->flush();
  ASSERT_EQ(px(10, 10), 0xF800);
  ASSERT_EQ(px(13, 13), 0xF800);
  ASSERT_EQ(px(9, 10), 0);
  ASSERT_EQ(px(10, 9), 0);
  ASSERT_EQ(px(14, 14), 0);
  ASSERT_TRUE(halFrameDirty());
  halClearFrameDirty();
  ASSERT_TRUE(!halFrameDirty());
}

TEST(canvas, desenho_sem_flush_nao_chega_na_hal) {
  Rig r;
  ASSERT_TRUE(r.gfx->begin(80000000));
  r.gfx->fillScreen(0x0000);
  r.gfx->flush();
  halClearFrameDirty();
  r.gfx->fillRect(50, 50, 10, 10, 0x07E0);
  ASSERT_EQ(px(55, 55), 0);
  ASSERT_TRUE(!halFrameDirty());
  r.gfx->flush();
  ASSERT_EQ(px(55, 55), 0x07E0);
  ASSERT_TRUE(halFrameDirty());
  halClearFrameDirty();
}

TEST(canvas, brilho_do_painel_chega_na_hal) {
  Rig r;
  r.panel->setBrightness(90);
  ASSERT_EQ((int)halBrightness(), 90);
  r.panel->setBrightness(0);
  ASSERT_EQ((int)halBrightness(), 0);
  r.panel->setBrightness(180);
  ASSERT_EQ((int)halBrightness(), 180);
}

TEST(canvas, halFrame_nunca_e_nulo) {
  ASSERT_TRUE(halFrame() != nullptr);
}

TEST(canvas, getTextBounds_fonte_classica) {
  Rig r;
  ASSERT_TRUE(r.gfx->begin(80000000));
  r.gfx->setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;
  r.gfx->getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
  ASSERT_EQ((int)w, 6);
  ASSERT_EQ((int)h, 8);
}

TEST(canvas, formas_basicas) {
  Rig r;
  ASSERT_TRUE(r.gfx->begin(80000000));
  r.gfx->fillScreen(0x0000);
  r.gfx->drawLine(10, 100, 50, 100, 0xFFFF);
  r.gfx->fillTriangle(100, 100, 140, 100, 120, 140, 0x001F);
  r.gfx->drawRoundRect(200, 200, 60, 40, 10, 0xF81F);
  r.gfx->flush();
  // linha
  ASSERT_EQ(px(10, 100), 0xFFFF);
  ASSERT_EQ(px(30, 100), 0xFFFF);
  ASSERT_EQ(px(50, 100), 0xFFFF);
  ASSERT_EQ(px(30, 101), 0);
  ASSERT_EQ(px(30, 99), 0);
  // triangulo
  ASSERT_EQ(px(120, 110), 0x001F);
  ASSERT_EQ(px(101, 101), 0x001F);
  ASSERT_EQ(px(101, 138), 0);
  ASSERT_EQ(px(139, 138), 0);
  // retangulo arredondado: borda sim, centro e quina nao
  ASSERT_EQ(px(230, 200), 0xF81F);
  ASSERT_EQ(px(200, 220), 0xF81F);
  ASSERT_EQ(px(230, 220), 0);
  ASSERT_EQ(px(200, 200), 0);
  halClearFrameDirty();
}

TEST(canvas, bitmap_parcialmente_fora_da_tela_e_recortado) {
  Rig r;
  // canvas pequeno com saida deslocada: sai pela direita, pela esquerda e por baixo
  {
    Arduino_Canvas c(100, 100, r.panel, 400, 400);
    ASSERT_TRUE(c.begin());
    c.fillScreen(0xAAAA);
    c.flush();  // cobre x,y 400..499: so 400..465 existe
    ASSERT_EQ(px(465, 465), 0xAAAA);
    ASSERT_EQ(px(400, 400), 0xAAAA);
  }
  {
    Arduino_Canvas c(100, 100, r.panel, -50, -50);
    ASSERT_TRUE(c.begin());
    c.fillScreen(0x5555);
    c.flush();  // so 0..49 existe
    ASSERT_EQ(px(0, 0), 0x5555);
    ASSERT_EQ(px(49, 49), 0x5555);
  }
  // chamada direta, totalmente fora e com ponteiro nulo: nao pode quebrar
  uint16_t bmp[4] = {1, 2, 3, 4};
  r.panel->draw16bitRGBBitmap(-1000, -1000, bmp, 2, 2);
  r.panel->draw16bitRGBBitmap(1000, 1000, bmp, 2, 2);
  r.panel->draw16bitRGBBitmap(0, 0, nullptr, 2, 2);
  halClearFrameDirty();
}
