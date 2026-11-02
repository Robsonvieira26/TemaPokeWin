"""Testes de tools/make_icon.py e do assets/tamapoke.ico gerado (stdlib unittest)."""
import pathlib
import struct
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.dont_write_bytecode = True
import make_icon  # noqa: E402

SIZES = (16, 32, 48, 256)


def parse_ico(data):
    reserved, typ, count = struct.unpack_from("<HHH", data, 0)
    assert (reserved, typ) == (0, 1)
    entries = []
    for i in range(count):
        w, h, colors, res, planes, bpp, size, off = struct.unpack_from("<BBBBHHII", data, 6 + 16 * i)
        entries.append(dict(w=w or 256, h=h or 256, colors=colors, res=res,
                            planes=planes, bpp=bpp, size=size, off=off))
    return count, entries


class IcoTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # gera numa pasta temporaria a partir da fonte versionada (reprodutibilidade)
        cls.tmp = tempfile.TemporaryDirectory()
        out = pathlib.Path(cls.tmp.name)
        _, _, rows = make_icon.read_png((ROOT / "assets" / "egg-scene-source.png").read_bytes())
        pngs = [(s, make_icon.write_png(s, s, make_icon.render(rows, s))) for s in SIZES]
        cls.generated = make_icon.build_ico(pngs)
        (out / "x.ico").write_bytes(cls.generated)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def check_ico(self, data):
        count, entries = parse_ico(data)
        self.assertEqual(count, len(SIZES))
        self.assertEqual([e["w"] for e in entries], list(SIZES))
        for e in entries:
            self.assertEqual((e["w"], e["h"]), (e["w"], e["w"]))
            self.assertEqual((e["planes"], e["bpp"]), (1, 32))
            self.assertLessEqual(e["off"] + e["size"], len(data))
            png = data[e["off"]:e["off"] + e["size"]]
            self.assertEqual(png[:8], make_icon.PNG_SIG)
            w, h = struct.unpack(">II", png[16:24])
            self.assertEqual((w, h), (e["w"], e["w"]))
            _, _, rows = make_icon.read_png(png)
            n = e["w"]
            for (x, y) in ((0, 0), (n - 1, 0), (0, n - 1), (n - 1, n - 1)):
                self.assertEqual(rows[y][x][3], 0, "canto deve ser transparente")
            self.assertEqual(rows[n // 2][n // 2][3], 255, "centro deve ser opaco")

    def test_generated_ico(self):
        self.check_ico(self.generated)

    def test_committed_ico_matches_generator(self):
        committed = (ROOT / "assets" / "tamapoke.ico").read_bytes()
        self.check_ico(committed)
        # compara pixels, nao bytes: a saida do zlib varia entre versoes do Python
        _, a = parse_ico(committed)
        _, b = parse_ico(self.generated)
        for ea, eb in zip(a, b):
            pa = make_icon.read_png(committed[ea["off"]:ea["off"] + ea["size"]])
            pb = make_icon.read_png(self.generated[eb["off"]:eb["off"] + eb["size"]])
            self.assertEqual(pa, pb, "rode: python -I tools/make_icon.py")

    def test_committed_png_256(self):
        data = (ROOT / "assets" / "tamapoke-256.png").read_bytes()
        self.assertEqual(data[:8], make_icon.PNG_SIG)
        self.assertEqual(struct.unpack(">II", data[16:24]), (256, 256))


if __name__ == "__main__":
    unittest.main()
