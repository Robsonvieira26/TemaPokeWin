#pragma once
#include <cstdint>
#include "pgmspace.h"

// A lib U8g2 real define isto em u8g2.h. Sem ele o decodificador do Arduino_GFX
// (Arduino_GFX.cpp) ignora os glifos acima de U+00FF (kana, hangul...).
#ifndef U8G2_WITH_UNICODE
#define U8G2_WITH_UNICODE
#endif

// Fonte u8g2 so por declaracao: os dados (u8g2_fonts.c) vem da lib U8g2 real.
// A mera existencia deste header liga U8G2_FONT_SUPPORT no Arduino_GFX.h.
extern const uint8_t u8g2_font_unifont_t_japanese3[];
extern const uint8_t u8g2_font_unifont_t_korean2[];
