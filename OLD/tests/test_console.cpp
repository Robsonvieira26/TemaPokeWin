// tests/test_console.cpp - editor de linha do console de debug (puro) e texto da tooltip.
#include "framework.h"
#include "shell_logic.h"
#include "win/console_line.h"
#include <string>

// Avalia a expressao uma vez e guarda a string (c_str de temporario penduraria).
#define CHECK_S(expr, lit) do { const std::string s_ = (expr); CHECK_STREQ(s_.c_str(), lit); } while (0)

namespace {
// Digita uma string UTF-16 e devolve a ultima linha completada ("" se nenhuma).
std::string typeAll(ConsoleLineEditor& e, const std::wstring& w, int* lines = nullptr) {
  std::string out, line;
  int n = 0;
  for (wchar_t c : w)
    if (e.feed(c, &line)) { out = line; ++n; }
  if (lines) *lines = n;
  return out;
}
}  // namespace

TEST(console_line, digitar_e_enter) {
  ConsoleLineEditor e;
  int n = 0;
  std::string l = typeAll(e, L"STATS\r", &n);
  CHECK_EQ(n, 1);
  CHECK_STREQ(l.c_str(), "STATS");
  CHECK(e.current().empty());
}

TEST(console_line, enter_so_devolve_ao_completar_e_ecoa) {
  ConsoleLineEditor e;
  std::string l;
  CHECK(!e.feed(L'L', &l));
  CHECK(e.lastEdit() == ConsoleLineEditor::Edit::Added);
  CHECK_STREQ(e.lastText().c_str(), "L");
  CHECK_STREQ(e.current().c_str(), "L");
  CHECK(e.feed(L'\n', &l));
  CHECK(e.lastEdit() == ConsoleLineEditor::Edit::Submitted);
  CHECK_STREQ(l.c_str(), "L");
}

TEST(console_line, backspace) {
  ConsoleLineEditor e;
  std::string l;
  typeAll(e, L"HATX");
  CHECK(!e.feed(L'\b', &l));
  CHECK(e.lastEdit() == ConsoleLineEditor::Edit::Erased);
  CHECK_STREQ(e.lastText().c_str(), "X");
  CHECK_STREQ(e.current().c_str(), "HAT");
  CHECK_S(typeAll(e, L"CH\r"), "HATCH");
}

TEST(console_line, backspace_em_linha_vazia) {
  ConsoleLineEditor e;
  std::string l;
  CHECK(!e.feed(L'\b', &l));
  CHECK(e.lastEdit() == ConsoleLineEditor::Edit::None);
  CHECK(e.current().empty());
  CHECK_S(typeAll(e, L"A\r"), "A");
}

TEST(console_line, nao_ascii_utf8) {
  ConsoleLineEditor e;
  CHECK_S(typeAll(e, L"NICK João\r"), "NICK Jo\xC3\xA3o");
  CHECK_S(typeAll(e, L"ピカ\r"), "\xE3\x83\x94\xE3\x82\xAB");
}

TEST(console_line, backspace_apaga_codepoint_inteiro) {
  ConsoleLineEditor e;
  std::string l;
  typeAll(e, L"aã");
  e.feed(L'\b', &l);
  CHECK_STREQ(e.current().c_str(), "a");
  CHECK_STREQ(e.lastText().c_str(), "\xC3\xA3");
  typeAll(e, L"ピ");
  e.feed(L'\b', &l);
  CHECK_STREQ(e.current().c_str(), "a");
}

TEST(console_line, par_substituto_emoji) {
  ConsoleLineEditor e;
  std::string l;
  CHECK(!e.feed(0xD83D, &l));
  CHECK(e.current().empty());  // metade: ainda nada
  CHECK(e.lastEdit() == ConsoleLineEditor::Edit::None);
  CHECK(!e.feed(0xDE00, &l));  // U+1F600
  CHECK_STREQ(e.current().c_str(), "\xF0\x9F\x98\x80");
  CHECK_STREQ(e.lastText().c_str(), "\xF0\x9F\x98\x80");
  e.feed(L'\b', &l);  // apaga o ponto de codigo inteiro (4 bytes)
  CHECK(e.current().empty());
}

TEST(console_line, substituto_solto_e_ignorado) {
  ConsoleLineEditor e;
  std::string l;
  e.feed(0xDE00, &l);  // baixo sem alto
  CHECK(e.current().empty());
  e.feed(0xD83D, &l);  // alto seguido de char normal: descarta o alto
  e.feed(L'x', &l);
  CHECK_STREQ(e.current().c_str(), "x");
}

TEST(console_line, enter_em_linha_vazia_ou_branca) {
  ConsoleLineEditor e;
  int n = 0;
  typeAll(e, L"\r\r", &n);
  CHECK_EQ(n, 0);
  typeAll(e, L"   \r", &n);
  CHECK_EQ(n, 0);
  CHECK(e.current().empty());
}

TEST(console_line, apara_espacos) {
  ConsoleLineEditor e;
  CHECK_S(typeAll(e, L"  LVL 5  \r"), "LVL 5");
}

TEST(console_line, crlf_nao_gera_duas_linhas) {
  ConsoleLineEditor e;
  int n = 0;
  std::string l = typeAll(e, L"STATS\r\n", &n);
  CHECK_EQ(n, 1);
  CHECK_STREQ(l.c_str(), "STATS");
}

TEST(console_line, linha_longa_corta) {
  ConsoleLineEditor e;
  std::wstring w(400, L'a');
  w += L"\r";
  int n = 0;
  std::string l = typeAll(e, w, &n);
  CHECK_EQ(n, 1);
  CHECK_EQ((int)l.size(), (int)ConsoleLineEditor::kMaxBytes);
  // codepoint multibyte que nao cabe e descartado inteiro, sem meio char
  ConsoleLineEditor f;
  std::wstring v(ConsoleLineEditor::kMaxBytes - 1, L'a');
  v += L"ピ\r";
  l = typeAll(f, v);
  CHECK_EQ((int)l.size(), (int)ConsoleLineEditor::kMaxBytes - 1);
}

TEST(console_line, controles_ignorados) {
  ConsoleLineEditor e;
  std::string l;
  const wchar_t ctrl[] = {0x01, 0x03, 0x07, L'\t', 0x1B, 0x1F, 0x7F};
  for (wchar_t c : ctrl) {
    CHECK(!e.feed(c, &l));
  }
  CHECK(e.current().empty());
  CHECK_S(typeAll(e, L"A\tB\r"), "AB");
}

// ---- texto da tooltip ----

TEST(shell_tooltip, normal) {
  CHECK_S(shellTooltipText("Pikachu", 12), "TamaPoke \xE2\x80\x94 Pikachu, n\xC3\xADvel 12");
}

TEST(shell_tooltip, vazio_nulo_ou_nivel_zero) {
  CHECK_S(shellTooltipText("", 5), "TamaPoke");
  CHECK_S(shellTooltipText(nullptr, 5), "TamaPoke");
  CHECK_S(shellTooltipText("Pikachu", 0), "TamaPoke \xE2\x80\x94 Pikachu");
}

TEST(shell_tooltip, nome_longo_e_nao_ascii) {
  std::string big(200, 'N');
  std::string t = shellTooltipText(big.c_str(), 3);  // a bandeja corta depois
  CHECK(t.size() > 200);
  CHECK_S(shellTooltipText("\xE3\x83\x94\xE3\x82\xAB\xE3\x83\x81\xE3\x83\xA5\xE3\x82\xA6", 7),
              "TamaPoke \xE2\x80\x94 \xE3\x83\x94\xE3\x82\xAB\xE3\x83\x81\xE3\x83\xA5\xE3\x82\xA6, n\xC3\xADvel 7");
}
