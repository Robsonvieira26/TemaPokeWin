"""Gera assets/tamapoke.ico e assets/tamapoke-256.png (somente stdlib).

Fonte: assets/egg-scene-source.png, um quadro 466x466 da cena do ovo desenhada
proceduralmente pelo firmware (MIT), sem sprites de Pokemon. O icone e um recorte
circular dessa cena, transparente fora do circulo, reduzido por vizinho mais
proximo para manter o pixel art nitido. Tamanhos pequenos usam um recorte mais
fechado em volta do ovo.

Uso: python -I tools/make_icon.py [--source PNG] [--out-dir DIR]
"""
import argparse
import pathlib
import struct
import zlib

SIZES = (16, 32, 48, 256)
# (x, y, lado) do recorte quadrado na imagem 466x466, centrado no ovo (~235,210)
CROP_WIDE = (135, 112, 200)
CROP_TIGHT = (160, 135, 150)
SUPERSAMPLE = 4  # amostras por eixo para a borda do circulo (cobertura -> alfa)
PNG_SIG = b"\x89PNG\r\n\x1a\n"


def read_png(data):
    """Decodifica PNG 8 bits RGB/RGBA sem interlace -> (w, h, [ (r,g,b,a) por linha ])."""
    assert data[:8] == PNG_SIG, "nao e PNG"
    pos, idat, w = 8, b"", 0
    while pos < len(data):
        n, typ = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if typ == b"IHDR":
            w, h, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and ctype in (2, 6) and inter == 0, "PNG nao suportado"
            bpp = 3 if ctype == 2 else 4
        elif typ == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        cur = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = cur[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                cur[i] = (cur[i] + a) & 255
            elif f == 2:
                cur[i] = (cur[i] + b) & 255
            elif f == 3:
                cur[i] = (cur[i] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                cur[i] = (cur[i] + pr) & 255
        rows.append([tuple(cur[x * bpp:x * bpp + 3]) + ((cur[x * bpp + 3],) if bpp == 4 else (255,))
                     for x in range(w)])
        prev = cur
    return w, h, rows


def write_png(w, h, rgba_rows):
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + b"".join(bytes(p) for p in row) for row in rgba_rows)
    return (PNG_SIG + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def render(rows, size):
    """Recorte circular da cena em size x size (RGBA)."""
    cx0, cy0, side = CROP_TIGHT if size <= 32 else CROP_WIDE
    out = []
    r = size / 2.0
    for y in range(size):
        line = []
        sy = cy0 + min(side - 1, y * side // size)
        for x in range(size):
            sx = cx0 + min(side - 1, x * side // size)
            R, G, B, _ = rows[sy][sx]
            inside = 0
            for j in range(SUPERSAMPLE):
                for i in range(SUPERSAMPLE):
                    dx = x + (i + 0.5) / SUPERSAMPLE - r
                    dy = y + (j + 0.5) / SUPERSAMPLE - r
                    if dx * dx + dy * dy <= r * r:
                        inside += 1
            alpha = inside * 255 // (SUPERSAMPLE * SUPERSAMPLE)
            line.append((R, G, B, alpha) if alpha else (0, 0, 0, 0))
        out.append(line)
    return out


def build_ico(pngs):
    """pngs: lista de (tamanho, bytes PNG). Entradas ICO comprimidas em PNG."""
    head = struct.pack("<HHH", 0, 1, len(pngs))
    off = 6 + 16 * len(pngs)
    entries, blobs = b"", b""
    for size, data in pngs:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), off)
        blobs += data
        off += len(data)
    return head + entries + blobs


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", default=str(root / "assets" / "egg-scene-source.png"))
    ap.add_argument("--out-dir", default=str(root / "assets"))
    a = ap.parse_args()
    _, _, rows = read_png(pathlib.Path(a.source).read_bytes())
    out = pathlib.Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    pngs = [(s, write_png(s, s, render(rows, s))) for s in SIZES]
    (out / "tamapoke-256.png").write_bytes(pngs[-1][1])
    (out / "tamapoke.ico").write_bytes(build_ico(pngs))
    print("ok:", ", ".join(str(s) for s in SIZES))


if __name__ == "__main__":
    main()
