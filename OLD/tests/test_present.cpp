#include "framework.h"
#include "present.h"
#include <vector>

TEST(present, mascara_circular_marca_dentro_e_fora) {
  ASSERT_TRUE(presentInsideCircle(233, 233));   // centro
  ASSERT_TRUE(presentInsideCircle(1, 233));     // borda esquerda
  ASSERT_TRUE(!presentInsideCircle(0, 0));      // canto
  ASSERT_TRUE(!presentInsideCircle(465, 0));
  ASSERT_TRUE(!presentInsideCircle(465, 465));
}

// Regra: dentro quando dx*dx+dy*dy <= 233*233 (centro 233,233; borda inclusiva).
TEST(present, mascara_circular_borda_e_inclusiva) {
  ASSERT_TRUE(presentInsideCircle(0, 233));     // dx=-233, dy=0: exatamente no raio
  ASSERT_TRUE(presentInsideCircle(233, 0));
  ASSERT_TRUE(!presentInsideCircle(0, 234));    // 233^2 + 1 > r^2
  ASSERT_TRUE(!presentInsideCircle(0, 232));
  ASSERT_TRUE(!presentInsideCircle(1, 1));
}

TEST(present, escala_converte_pixel_de_janela_em_coord_logica) {
  ASSERT_EQ(presentToLogical(100, 2.0f), 50);
  ASSERT_EQ(presentToLogical(100, 1.0f), 100);
  ASSERT_EQ(presentToLogical(100, 0.5f), 200);
  ASSERT_EQ(presentToLogical(-5, 1.0f), 0);     // clamp
  ASSERT_EQ(presentToLogical(1000, 1.0f), 465); // clamp
}

TEST(present, rgb565_vira_argb8888) {
  ASSERT_EQ(presentRgb565ToArgb(0xF800), 0xFFFF0000u);
  ASSERT_EQ(presentRgb565ToArgb(0x07E0), 0xFF00FF00u);
  ASSERT_EQ(presentRgb565ToArgb(0x001F), 0xFF0000FFu);
  ASSERT_EQ(presentRgb565ToArgb(0xFFFF), 0xFFFFFFFFu);
  ASSERT_EQ(presentRgb565ToArgb(0x0000), 0xFF000000u);
}

TEST(present, converte_quadro_com_e_sem_mascara) {
  const size_t n = 466u * 466u;
  std::vector<uint16_t> in(n, 0xF800);
  std::vector<uint32_t> out(n);
  presentConvertFrame(in.data(), out.data(), true);
  ASSERT_EQ(out[0], 0x00000000u);
  ASSERT_EQ(out[465], 0x00000000u);
  ASSERT_EQ(out[465u * 466u + 465u], 0x00000000u);
  ASSERT_EQ(out[233u * 466u + 233u], 0xFFFF0000u);
  presentConvertFrame(in.data(), out.data(), false);
  ASSERT_EQ(out[0], 0xFFFF0000u);
  ASSERT_EQ(out[n - 1], 0xFFFF0000u);
}

TEST(present, brilho_do_painel_vira_brilho_de_desktop) {
  struct { int in, out; } c[] = {{0,0},{1,100},{8,100},{25,100},{60,100},{100,141},
                                 {145,205},{180,255},{200,255},{255,255}};
  for (auto& e : c) CHECK_EQ((int)presentDisplayBrightness((uint8_t)e.in), e.out);
}
