"""Lint de fumaca de installer/tamapoke.iss (stdlib unittest).

NAO substitui compilar com o ISCC (Inno Setup): nao faz parse de Pascal Script nem de ISPP.
So pega erros grosseiros: secoes ausentes, arquivos referenciados que nao existem, GUID
malformado, divergencia do nome do valor de autostart em relacao ao src/main.cpp.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
ISS = ROOT / "installer" / "tamapoke.iss"
TEXT = ISS.read_text(encoding="utf-8")


def section(name):
    m = re.search(r"^\[%s\]\s*$(.*?)(?=^\[\w+\]\s*$|\Z)" % re.escape(name), TEXT, re.M | re.S)
    return m.group(1) if m else None


class IssLint(unittest.TestCase):
    def test_sections_present(self):
        for s in ("Setup", "Files", "Icons", "Tasks", "Run", "Code"):
            self.assertIsNotNone(section(s), "secao [%s] ausente" % s)

    def test_appid_guid(self):
        m = re.search(r"^AppId=(.*)$", TEXT, re.M)
        self.assertIsNotNone(m)
        self.assertRegex(m.group(1).strip(),
                         r"^\{\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}$")

    def test_per_user_install(self):
        self.assertRegex(TEXT, r"(?m)^PrivilegesRequired=lowest\s*$")

    def test_relative_files_exist(self):
        setup = section("Setup")
        paths = re.findall(r"^\w+=(\.\.\\[^\r\n]+)$", setup, re.M)
        self.assertTrue(paths)
        for p in paths:
            if "{#" in p:  # variavel ISPP (ex.: OutputBaseFilename)
                continue
            rel = p.strip().replace("\\", "/")
            if rel.startswith("../dist"):  # pasta de saida, criada pelo ISCC
                continue
            self.assertTrue((ISS.parent / rel).exists(), "nao existe: " + p)

    def test_quit_and_run_value_match_source(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        m = re.search(r'autostartEnable\("([^"]+)"', main)
        self.assertIsNotNone(m)
        name = m.group(1)
        self.assertIn('"--quit"', main)
        self.assertIn("'--quit'", TEXT)
        self.assertIn('ValueName: "%s"' % name, TEXT)
        self.assertIn("'%s')" % name, TEXT)  # RegDeleteValue(..., 'TamaPoke')
        self.assertIn('--hidden"', section("Registry"))
        self.assertIn('" --hidden"', (ROOT / "src" / "app_shell.cpp").read_text(encoding="utf-8"))

    def test_code_begin_end_balance(self):
        code = re.sub(r"//[^\n]*", "", section("Code"))
        code = re.sub(r"'[^'\n]*'", "''", code)  # ignora literais
        words = re.findall(r"\b(begin|end|case|try)\b", code, re.I)
        opens = sum(1 for w in words if w.lower() in ("begin", "case", "try"))
        closes = sum(1 for w in words if w.lower() == "end")
        self.assertEqual(opens, closes)

    def test_ispp_defaults(self):
        self.assertIn("#ifndef AppVersion", TEXT)
        self.assertIn("#ifndef SourceDir", TEXT)


if __name__ == "__main__":
    unittest.main()
