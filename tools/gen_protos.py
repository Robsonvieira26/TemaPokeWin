#!/usr/bin/env python3
"""Converte um sketch Arduino (.ino) em uma unidade de traducao C++ compilavel.

Reproduz o que o builder do Arduino faz implicitamente, sem tocar no original:

1. injeta `#include <Arduino.h>` no topo se o sketch nao o incluir (seguido de
   `#line 1 "<nome>"`);
2. gera prototipos (declaracoes antecipadas) de toda funcao definida no nivel
   superior, em um ou mais blocos delimitados por marcadores;
3. emite `#line N "<nome>"` depois de cada bloco, para que diagnosticos do
   compilador apontem para o arquivo e as linhas originais.

Regra de insercao (a do Arduino, refinada para tipos definidos tarde):
  - o bloco principal entra imediatamente antes da primeira definicao de funcao
    (portanto depois de includes, #defines, comentarios e de qualquer tipo ou
    variavel declarado antes dela);
  - um prototipo cujo texto cite um tipo (struct/class/union/enum/typedef/using)
    definido no sketch DEPOIS desse ponto nao pode ficar la; ele vai para um
    bloco proprio, logo apos a linha em que o ultimo tipo necessario termina.
    Tipos vindos de headers incluidos (ex.: PowerCtx de power.h) ja existem no
    ponto principal e nao entram nessa conta. No TamaPoke.ino, `struct Btn`
    (definido depois da primeira funcao) nao aparece em nenhuma assinatura, entao
    sai um unico bloco; o mecanismo cobre o caso geral.

O que vira prototipo: apenas definicoes de funcao com profundidade 0. Ignorados:
corpos de struct/class/enum, `struct {..} x;`, inicializadores `= {..};`,
lambdas, ponteiros para funcao, macros e diretivas, blocos `extern "C" {}` e
`namespace {}` (conteudo nao e promovido; o sketch real nao os usa).
Funcoes com argumento padrao NA DEFINICAO nao geram prototipo (repetir o padrao
seria erro em C++; a funcao ja precisa estar definida antes do uso). Sobrecargas
geram um prototipo por assinatura distinta.

Uso: gen_protos.py ENTRADA.ino -o SAIDA.cpp
"""
import argparse
import os
import re
import sys

BEGIN = "// --- gen_protos: prototipos ---"
END = "// --- gen_protos: fim ---"

_ID = r"[A-Za-z_]\w*"


def _blank(chars, a, b):
    """Troca [a,b) por espacos, mantendo quebras de linha (indices estaveis)."""
    for k in range(a, b):
        if chars[k] not in "\r\n":
            chars[k] = " "


def mask(text):
    """Devolve (sem_comentarios, estrutural, diretivas).

    sem_comentarios: comentarios e diretivas viram espacos; literais ficam.
    estrutural: alem disso o conteudo de strings/chars vira espacos.
    diretivas: lista de textos de linhas de preprocessador (para achar includes).
    Ambos tem o mesmo tamanho do texto, entao os indices coincidem.
    """
    n = len(text)
    nc = list(text)   # sem comentarios
    st = list(text)   # estrutural
    directives = []
    i = 0
    line_start = True
    while i < n:
        c = text[i]
        if line_start and c in " \t":
            i += 1
            continue
        if line_start and c == "#":
            # diretiva: ate o fim da linha, com continuacao por '\' e comentarios
            j = i
            while j < n:
                if text.startswith("/*", j):
                    e = text.find("*/", j + 2)
                    j = n if e < 0 else e + 2
                    continue
                if text.startswith("//", j):
                    while j < n and text[j] != "\n":
                        j += 1
                    break
                if text[j] == "\n":
                    k = j - 1
                    if k >= 0 and text[k] == "\r":
                        k -= 1
                    if k >= 0 and text[k] == "\\":
                        j += 1
                        continue
                    break
                j += 1
            directives.append(text[i:j])
            _blank(nc, i, j)
            _blank(st, i, j)
            i = j
            line_start = False
            continue
        line_start = False
        if c == "\n":
            line_start = True
            i += 1
        elif text.startswith("//", i):
            j = i
            while j < n and text[j] != "\n":
                j += 1
            _blank(nc, i, j)
            _blank(st, i, j)
            i = j
        elif text.startswith("/*", i):
            e = text.find("*/", i + 2)
            j = n if e < 0 else e + 2
            _blank(nc, i, j)
            _blank(st, i, j)
            i = j
        elif c == '"':
            if re.search(r"(?<![\w])(?:u8|u|U|L)?R$", text[max(0, i - 3):i]):
                # string crua R"delim( ... )delim"
                p = text.find("(", i)
                delim = text[i + 1:p]
                e = text.find(")" + delim + '"', p)
                j = n if e < 0 else e + len(delim) + 2
            else:
                j = i + 1
                while j < n and text[j] != '"' and text[j] != "\n":
                    j += 2 if text[j] == "\\" else 1
                j = min(j + 1, n)
            _blank(st, i + 1, max(i + 1, j - 1))
            i = j
        elif c == "'":
            # separador de digitos (1'000) nao abre literal
            k = i
            while k > 0 and (text[k - 1].isalnum() or text[k - 1] == "_"):
                k -= 1
            if k < i and text[k].isdigit():
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != "'" and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            _blank(st, i + 1, max(i + 1, j - 1))
            i = j
        else:
            i += 1
    return "".join(nc), "".join(st), directives


def _match_brace(s, i):
    """i aponta para '{' em s (estrutural); devolve indice do '}' casado."""
    depth = 0
    for k in range(i, len(s)):
        if s[k] == "{":
            depth += 1
        elif s[k] == "}":
            depth -= 1
            if depth == 0:
                return k
    return len(s) - 1


_QUAL = re.compile(r"(?:\s+(?:const|volatile|noexcept|override|final))+\s*$")


def _is_function_header(header):
    """header = texto estrutural entre o inicio do item e o '{'."""
    h = re.sub(r"\boperator\s*[^\s(]+", "operator_", header)
    h = _QUAL.sub("", h).rstrip()
    h = re.sub(r"\s*->\s*[^()]*$", "", h)  # tipo de retorno final
    if not h.endswith(")"):
        return False
    p = h.find("(")
    if p < 0:
        return False
    if "=" in h[:p]:       # variavel inicializada (lambda, array...)
        return False
    if re.match(r"\s*(?:struct|class|union|enum|namespace|typedef|using)\b", h) and \
            not re.search(r"\)\s*$", h):
        return False
    return True


_TYPE_HDR = re.compile(
    r"^\s*(?:typedef\s+)?(?:struct|class|union|enum(?:\s+class|\s+struct)?)\s+"
    r"(" + _ID + r")\s*(?::[^{]*)?$")


def scan(nc, st):
    """Percorre o nivel superior. Devolve (funcoes, tipos).

    funcoes: [(inicio, fim, cabecalho_sem_comentarios)]
    tipos:   [(nome, fim_do_item)]
    """
    n = len(st)
    funcs, types = [], []
    i = 0
    while True:
        while i < n and st[i].isspace():
            i += 1
        if i >= n:
            break
        start = i
        j = i
        pdepth = 0
        end = None
        kind = "decl"
        while j < n:
            ch = st[j]
            if ch in "([":
                pdepth += 1
            elif ch in ")]":
                pdepth -= 1
            elif ch == ";" and pdepth <= 0:
                end = j + 1
                break
            elif ch == "{" and pdepth <= 0:
                header = st[start:j]
                close = _match_brace(st, j)
                if _is_function_header(header):
                    funcs.append((start, close + 1, nc[start:j]))
                    end = close + 1
                    kind = "func"
                    break
                if re.match(r'\s*(?:extern\s*"[^"]*"|namespace\b[^{]*)\s*$', header):
                    end = close + 1
                    kind = "block"
                    break
                m = _TYPE_HDR.match(header)
                if m:
                    types.append([m.group(1), None])
                j = close + 1
                continue
            j += 1
        if end is None:
            end = n
        if kind == "decl":
            item = st[start:end]
            for t in types:
                if t[1] is None:
                    t[1] = end
            m = re.match(r"\s*typedef\b.*?(" + _ID + r")\s*(?:\[[^\]]*\]\s*)*;\s*$", item, re.S)
            if m and not re.search(r"\(\s*\*", item):
                types.append([m.group(1), end])
            m = re.match(r"\s*using\s+(" + _ID + r")\s*=", item)
            if m:
                types.append([m.group(1), end])
        i = end
    return funcs, [(a, b) for a, b in types if b is not None]


def _has_default_arg(header):
    p = header.find("(")
    depth = 0
    for ch in header[p:]:
        if ch in "([<{":
            depth += 1 if ch != "<" else 0
        elif ch in ")]}":
            depth -= 1
        elif ch == "=" and depth == 1:
            return True
    return False


def _prototype(header):
    p = re.sub(r"\s+", " ", header).strip()
    p = re.sub(r"\(\s+", "(", p)
    p = re.sub(r"\s+\)", ")", p)
    return p + ";"


def _line_start_after(text, pos):
    k = text.find("\n", pos)
    return len(text) if k < 0 else k + 1


def _line_start_of(text, pos):
    return text.rfind("\n", 0, pos) + 1


def generate(text, name):
    nl = "\r\n" if "\r\n" in text else "\n"
    nc, st, directives = mask(text)
    funcs, types = scan(nc, st)

    has_arduino = any(re.match(r'#\s*include\s*[<"]Arduino\.h[>"]', d) for d in directives)

    groups = {}   # posicao -> [prototipos]
    seen = set()
    main_pos = _line_start_of(text, funcs[0][0]) if funcs else 0
    type_end = {}
    for nm, e in types:
        type_end[nm] = max(type_end.get(nm, 0), e)
    for start, _end, header in funcs:
        if _has_default_arg(header):
            continue
        proto = _prototype(header)
        if proto in seen:
            continue
        seen.add(proto)
        pos = main_pos
        for word in set(re.findall(_ID, proto)):
            e = type_end.get(word)
            if e is not None and e > main_pos:
                pos = max(pos, _line_start_after(text, e))
        groups.setdefault(pos, []).append(proto)

    out = []
    prev = 0
    for pos in sorted(groups):
        out.append(text[prev:pos])
        line = text.count("\n", 0, pos) + 1
        out.append(nl.join([BEGIN] + groups[pos] + [END, '#line %d "%s"' % (line, name)]) + nl)
        prev = pos
    out.append(text[prev:])
    body = "".join(out)
    if not has_arduino:
        body = "#include <Arduino.h>" + nl + '#line 1 "%s"' % name + nl + body
    return body


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("entrada")
    ap.add_argument("-o", "--saida", required=True)
    a = ap.parse_args(argv)
    with open(a.entrada, "rb") as f:
        text = f.read().decode("utf-8")
    result = generate(text, os.path.basename(a.entrada))
    d = os.path.dirname(os.path.abspath(a.saida))
    os.makedirs(d, exist_ok=True)
    with open(a.saida, "wb") as f:
        f.write(result.encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
