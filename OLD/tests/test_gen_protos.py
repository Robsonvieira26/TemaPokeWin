"""Testes de tools/gen_protos.py (stdlib unittest). Uso: python -I tests/test_gen_protos.py"""
import os
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "..", "tools", "gen_protos.py")
FIXTURE = os.path.join(HERE, "fixtures", "exemplo.ino")

BEGIN = "// --- gen_protos: prototipos ---"
END = "// --- gen_protos: fim ---"


def read(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8")


def gen_text(text, name="x.ino"):
    """Gera a partir de texto; devolve o .cpp como string (bytes preservados)."""
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, name)
        out = os.path.join(d, "out.cpp")
        with open(src, "wb") as f:
            f.write(text.encode("utf-8"))
        r = subprocess.run([sys.executable, "-I", SCRIPT, src, "-o", out],
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise AssertionError("gen_protos falhou: " + r.stderr)
        return read(out)


def lf(text):
    """Normaliza CRLF -> LF: o checkout do Windows (autocrlf) pode entregar a fixture com CRLF."""
    return text.replace("\r\n", "\n")


def blocks(out):
    """Lista de blocos de prototipos (cada um uma lista de linhas)."""
    res, cur = [], None
    for ln in out.replace("\r\n", "\n").split("\n"):
        if ln == BEGIN:
            cur = []
        elif ln == END:
            res.append(cur)
            cur = None
        elif cur is not None:
            cur.append(ln)
    return res


def strip_injected(out):
    """Remove tudo que a ferramenta injeta; deve sobrar o original."""
    keep, inblock = [], False
    for ln in out.split("\n"):
        c = ln.rstrip("\r")
        if c == BEGIN:
            inblock = True
            continue
        if inblock:
            if c == END:
                inblock = False
            continue
        if re.match(r'^#line \d+ ".*"$', c):
            continue
        keep.append(ln)
    if re.match(r'#include <Arduino\.h>\r?\n#line 1 ', out):
        keep = keep[1:]
    return "\n".join(keep)


class FixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = read(FIXTURE)
        cls.out = gen_text(cls.src, "exemplo.ino")

    def test_injeta_arduino_h_no_topo(self):
        lines = lf(self.out).split("\n")
        self.assertEqual(lines[0], "#include <Arduino.h>")
        self.assertEqual(lines[1], '#line 1 "exemplo.ino"')

    def test_prototipos_exatos(self):
        b = blocks(self.out)
        self.assertEqual(len(b), 2)
        self.assertEqual(b[0], [
            "int first(Early e);",
            "void IRAM_ATTR isr();",
            "FakeType useFake(FakeType t);",
            "void over(int a);",
            "void over(const char *s);",
            "void declared();",
        ])
        # dependem de Late (definido depois da primeira funcao)
        self.assertEqual(b[1], [
            "static bool helper(const Late &l, int n);",
            "uint16_t multiLine(int a, const char *b, Late *l);",
        ])

    def test_primeiro_bloco_antes_da_primeira_funcao(self):
        i = self.out.index(BEGIN)
        self.assertLess(self.out.index("struct Early"), i)
        self.assertLess(i, self.out.index("int first(Early e) {"))

    def test_segundo_bloco_depois_de_Late(self):
        j = self.out.index(BEGIN, self.out.index(BEGIN) + 1)
        self.assertGreater(j, self.out.index("struct Late { int v; };"))
        self.assertLess(j, self.out.index("struct { int x, y; } anon"))

    def test_nao_prototipa_nao_funcoes(self):
        todos = "\n".join("\n".join(b) for b in blocks(self.out))
        for nome in ("anon", "TABLE", "fp", "lam", "counter", "MACRO", "LONGA",
                     "if", "for"):
            self.assertIsNone(re.search(r"\b%s\b" % nome, todos), nome)
        self.assertNotIn("struct", todos)
        self.assertNotIn("=", todos)

    def test_line_directives_apontam_para_o_original(self):
        orig = lf(self.src).split("\n")
        out = lf(self.out).split("\n")
        achou = 0
        for k, ln in enumerate(out):
            m = re.match(r'^#line (\d+) "exemplo.ino"$', ln)
            if m:
                achou += 1
                self.assertEqual(out[k + 1], orig[int(m.group(1)) - 1])
        self.assertEqual(achou, 3)  # topo + 2 blocos

    def test_fim_de_linha_da_fixture_preservado(self):
        # a saida usa exatamente o fim de linha da entrada (CRLF -> so CRLF; LF -> so LF)
        if "\r\n" in self.src:
            self.assertIsNone(re.search(r"(?<!\r)\n", self.out))
        else:
            self.assertNotIn("\r", self.out)

    def test_so_difere_pelas_linhas_injetadas(self):
        self.assertEqual(strip_injected(self.out), self.src)


class OutrosCasos(unittest.TestCase):
    def test_nao_duplica_arduino_h(self):
        src = "#include <Arduino.h>\nvoid f() {}\n"
        out = gen_text(src)
        self.assertEqual(out.count("#include <Arduino.h>"), 1)
        self.assertNotIn('#line 1 "x.ino"', out)
        self.assertEqual(blocks(out), [["void f();"]])
        self.assertEqual(strip_injected(out), src)

    def test_arduino_h_so_em_comentario_ainda_injeta(self):
        src = "// #include <Arduino.h>\nvoid f() {}\n"
        self.assertTrue(gen_text(src).startswith("#include <Arduino.h>\n"))

    def test_argumento_padrao_na_definicao_nao_gera_prototipo(self):
        src = "void a(int x = 3) {}\nvoid b(int y) {}\n"
        self.assertEqual(blocks(gen_text(src)), [["void b(int y);"]])

    def test_extern_c_e_namespace_ignorados(self):
        src = ('extern "C" {\nvoid ignorada() {}\n}\n'
               'namespace n { void tambem() {} }\nvoid f() {}\n')
        self.assertEqual(blocks(gen_text(src)), [["void f();"]])

    def test_raw_string_e_separador_de_digitos(self):
        src = ('const char *r = R"x( } { ( " )x";\nint n = 1\'000;\n'
               'void f() { char c = \'}\'; }\n')
        out = gen_text(src)
        self.assertEqual(blocks(out), [["void f();"]])
        self.assertEqual(strip_injected(out), src)

    def test_template_ponteiros_e_referencias(self):
        src = ("template <typename T> T id(T v) { return v; }\n"
               "static inline const char *const &g(int *p) { static const char *c; return *(&c); }\n")
        self.assertEqual(blocks(gen_text(src)), [[
            "template <typename T> T id(T v);",
            "static inline const char *const &g(int *p);"]])

    def test_sem_funcoes_nao_gera_bloco(self):
        out = gen_text("int x = 1;\n")
        self.assertEqual(blocks(out), [])
        self.assertEqual(strip_injected(out), "int x = 1;\n")

    def test_crlf_preservado(self):
        src = "int a;\r\nvoid f() {}\r\n"
        out = gen_text(src)
        self.assertEqual(blocks(out), [["void f();"]])
        self.assertEqual(strip_injected(out), src)
        self.assertIsNone(re.search(r"(?<!\r)\n", out))

    def test_tipo_definido_entre_funcoes(self):
        src = "void a() {}\nstruct T { int v; };\nvoid b(T t) {}\n"
        self.assertEqual(blocks(gen_text(src)), [["void a();"], ["void b(T t);"]])


if __name__ == "__main__":
    unittest.main()
