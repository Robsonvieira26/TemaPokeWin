"""Testes de tools/extract_u8g2_fonts.py (stdlib unittest). Uso: python -I tests/test_extract_u8g2_fonts.py"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import extract_u8g2_fonts as x  # noqa: E402

SNIPPET = r'''
#include "u8g2.h"
/* outro */
const uint8_t other[2] U8G2_FONT_SECTION("other") = "\1\2";
/*
  Fontname: demo
  Copyright: nobody
*/
#ifdef U8G2_USE_LARGE_FONTS
const uint8_t font_a[16] U8G2_FONT_SECTION("font_a") =
  "AB\x41\x7a" "\101\60\0\377" /* comentario */
  "\x1""2" "\\\"\n\t"   // fim de linha
  "z\
";
#endif
'''


class Extract(unittest.TestCase):
    def test_bytes_exatos(self):
        declared, data, comment = x.extract_array(SNIPPET, "font_a")
        # AB, \x41='A', \x7a='z', \101='A', \60='0', \0, \377, \x1 + '2', '\', '"', '\n', '\t', 'z'
        want = bytes([65, 66, 0x41, 0x7A, 65, 0x30, 0, 255, 1, 0x32, 92, 34, 10, 9, 0x7A, 0])  # + NUL implicito
        self.assertEqual(data, want)
        self.assertEqual(declared, 16)
        self.assertIn("Fontname: demo", comment)

    def test_hex_guloso_separado_por_literais(self):
        _, data, _ = x.extract_array(r'const uint8_t f[2] = "\x1" "2";', "f")
        self.assertEqual(data, bytes([1, 0x32]))
        _, data, _ = x.extract_array(r'const uint8_t f[1] = "\x12";', "f")
        self.assertEqual(data, bytes([0x12]))

    def test_octal_tres_digitos_no_maximo(self):
        _, data, _ = x.extract_array(r'const uint8_t f[2] = "\1234";', "f")
        self.assertEqual(data, bytes([0o123, ord("4")]))
        _, data, _ = x.extract_array(r'const uint8_t f[2] = "\0" "1";', "f")
        self.assertEqual(data, bytes([0, ord("1")]))

    def test_nome_inexistente_e_hex_grande(self):
        with self.assertRaises(ValueError):
            x.extract_array(SNIPPET, "nao_existe")
        with self.assertRaises(ValueError):
            x.extract_array(r'const uint8_t f[1] = "\x141";', "f")

    def test_emit_confere_tamanho_declarado(self):
        with self.assertRaises(SystemExit):
            x.emit(["font_a"], SNIPPET.replace("font_a[16]", "font_a[20]"), "t", "lic", "cmd", "2000-01-01")
        ok = SNIPPET
        out = x.emit(["font_a"], ok, "t", "lic", "cmd", "2000-01-01")
        self.assertIn("const uint8_t font_a[] = {", out)
        self.assertIn("0x41,0x42,0x41,0x7a", out)
        self.assertIn("font_a_size = 16;", out)
        self.assertIn('#include "U8g2lib.h"', out)


if __name__ == "__main__":
    unittest.main()
