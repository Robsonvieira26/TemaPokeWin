#!/usr/bin/env python3
r"""Extrai arrays de fonte de u8g2_fonts.c (literais de string C) para um .cpp de bytes.

MSVC nao compila literais de string enormes (limite ~64 KB), entao os dados sao
re-emitidos como `{ 0x.., ... }`. Escapes tratados como no C: \xHH (guloso),
\NNN octal (ate 3 digitos), \\ \\" \' \? \a \b \f \n \r \t \v, e
concatenacao de literais adjacentes.

Uso: python -I tools/extract_u8g2_fonts.py --source u8g2_fonts.c --tag 2.37.1 \
         --license LICENSE --out hal/u8g2_fonts.cpp NOME [NOME ...]
"""
import argparse
import datetime
import re
import sys

SIMPLE = {"a": 7, "b": 8, "f": 12, "n": 10, "r": 13, "t": 9, "v": 11,
          "\\": 92, "'": 39, '"': 34, "?": 63}
HEXD = "0123456789abcdefABCDEF"


def parse_literal_body(s, i, out):
    """s[i] esta logo apos a aspa de abertura; consome ate a aspa de fechamento."""
    n = len(s)
    while True:
        if i >= n:
            raise ValueError("literal de string nao terminado")
        c = s[i]
        if c == '"':
            return i + 1
        if c == "\n":
            raise ValueError("quebra de linha dentro de literal")
        if c != "\\":
            if ord(c) > 126 or ord(c) < 32 and c != chr(9):
                raise ValueError("byte nao ASCII cru dentro de literal")
            out.append(ord(c))
            i += 1
            continue
        i += 1
        c = s[i]
        if c == "\n":                      # continuacao de linha
            i += 1
        elif c == "\r" and s[i + 1] == "\n":
            i += 2
        elif c == "x":
            i += 1
            j = i
            while j < n and s[j] in HEXD:
                j += 1
            if j == i:
                raise ValueError("hex escape sem digitos")
            v = int(s[i:j], 16)
            if v > 255:
                raise ValueError("hex escape fora de um byte: " + s[i - 2:j])
            out.append(v)
            i = j
        elif c in "01234567":
            j = i
            while j < n and j < i + 3 and s[j] in "01234567":
                j += 1
            v = int(s[i:j], 8)
            if v > 255:
                raise ValueError("octal fora de um byte")
            out.append(v)
            i = j
        elif c in SIMPLE:
            out.append(SIMPLE[c])
            i += 1
        else:
            raise ValueError("escape desconhecido: \\" + c)


def skip_ws_comments(s, i):
    n = len(s)
    while i < n:
        if s[i] in " \t\r\n":
            i += 1
        elif s.startswith("/*", i):
            e = s.find("*/", i + 2)
            if e < 0:
                raise ValueError("comentario nao terminado")
            i = e + 2
        elif s.startswith("//", i):
            e = s.find("\n", i)
            i = n if e < 0 else e + 1
        else:
            break
    return i


def extract_array(text, name):
    """Devolve (declared_len, bytes, comentario_precedente)."""
    m = re.search(r"\b" + re.escape(name) + r"\s*\[\s*(\d+)\s*\]", text)
    if not m:
        raise ValueError("array nao encontrado: " + name)
    declared = int(m.group(1))
    i = skip_ws_comments(text, m.end())
    # atributos entre o ] e o = (ex.: U8G2_FONT_SECTION("x"))
    eq = text.find("=", i)
    if eq < 0 or ";" in text[i:eq]:
        raise ValueError("sem '=' para " + name)
    i = skip_ws_comments(text, eq + 1)
    out = bytearray()
    while True:
        if i >= len(text):
            raise ValueError("fim de arquivo antes do ';'")
        if text[i] == '"':
            i = parse_literal_body(text, i + 1, out)
            i = skip_ws_comments(text, i)
        elif text[i] == ";":
            break
        else:
            raise ValueError("token inesperado em %d: %r" % (i, text[i:i + 20]))
    # comentario /* ... */ imediatamente antes da declaracao (pulando #if/#ifdef)
    head = text[:max(text.rfind(chr(10), 0, m.start()), 0)]
    lines = head.rstrip().split("\n")
    while lines and lines[-1].lstrip().startswith("#"):
        lines.pop()
    pre = "\n".join(lines).rstrip()
    comment = ""
    if pre.endswith("*/"):
        s = pre.rfind("/*")
        comment = pre[s:]
        comment.encode("ascii")  # vai para o .cpp gerado como esta
    # Regra do C: `T a[N] = "lit"` acrescenta o NUL implicito se sobrar espaco.
    if declared == len(out) + 1:
        out.append(0)
    return declared, bytes(out), comment


def emit(names, text, tag, license_text, cmdline, date):
    parts = []
    parts.append("// GERADO por tools/extract_u8g2_fonts.py - nao edite.\n")
    parts.append("//\n// Dados de fonte de https://github.com/olikraus/u8g2 (csrc/u8g2_fonts.c),\n"
                 "// tag %s. Gerado em %s com:\n//   %s\n//\n" % (tag, date, cmdline))
    parts.append("// ---- Licenca do codigo U8g2 (BSD 2 clausulas), de LICENSE do U8g2 %s ----\n" % tag)
    for ln in license_text.strip("\n").split("\n"):
        parts.append(("// " + ln).rstrip() + "\n")
    parts.append("//\n// As fontes tem licenca propria (ver comentario de cada array abaixo).\n\n")
    parts.append('#include "U8g2lib.h"\n\n')
    for name in names:
        declared, data, comment = extract_array(text, name)
        if declared != len(data):
            raise SystemExit("%s: tamanho declarado %d != %d bytes lidos" % (name, declared, len(data)))
        if comment:
            parts.append("/* origem: u8g2_fonts.c, " + name + " */\n")
            parts.append(comment)
            parts.append("\n")
        parts.append("const uint8_t %s[] = {\n" % name)
        for k in range(0, len(data), 16):
            parts.append("  " + ",".join("0x%02x" % b for b in data[k:k + 16]) + ",\n")
        parts.append("};\n")
        parts.append("extern const unsigned long %s_size = %d;\n\n" % (name, len(data)))
        print("%s: %d bytes" % (name, len(data)), file=sys.stderr)
    return "".join(parts)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", required=True)
    ap.add_argument("--tag", required=True)
    ap.add_argument("--license", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--date", default=datetime.date.today().isoformat())
    ap.add_argument("names", nargs="+")
    a = ap.parse_args(argv)
    with open(a.source, "rb") as f:
        text = f.read().decode("latin-1")  # o fonte tem bytes crus em comentarios de outras fontes
    with open(a.license, "rb") as f:
        lic = f.read().decode("utf-8")
    k = lic.index("============ BSD License")
    lic = lic[k:]
    end = lic.index("ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.") + len("ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.")
    lic = lic[:end]
    cmdline = "python -I tools/extract_u8g2_fonts.py --source u8g2_fonts.c --tag %s --license LICENSE --out hal/u8g2_fonts.cpp %s" % (a.tag, " ".join(a.names))
    out = emit(a.names, text, a.tag, lic, cmdline, a.date)
    with open(a.out, "wb") as f:
        f.write(out.encode("utf-8"))


if __name__ == "__main__":
    main(sys.argv[1:])
