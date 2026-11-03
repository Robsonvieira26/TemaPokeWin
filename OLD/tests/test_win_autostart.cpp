// tests/test_win_autostart.cpp
// Mexe na chave Run REAL do HKCU, mas SEMPRE com nome unico por execucao
// (nunca o valor "TamaPoke"); o guard RAII apaga o valor mesmo se um CHECK falhar.
#include "framework.h"
#include "win/autostart.h"
#include <windows.h>
#include <string>

namespace {
const wchar_t* kRunKeyW = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

std::string nomeUnico(const char* sufixo = "") {
  return "TamaPokeTest_" + std::to_string(GetCurrentProcessId()) + "_" +
         std::to_string(GetTickCount64()) + sufixo;
}

std::wstring utf8ToWide(const std::string& s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring w(n > 0 ? n - 1 : 0, L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
  return w;
}

struct Guard {
  std::string nome;
  explicit Guard(std::string n) : nome(std::move(n)) { autostartDisable(nome.c_str()); }
  ~Guard() { autostartDisable(nome.c_str()); }
};

// Le o dado REG_SZ armazenado (sem o NUL final); false se ausente/tipo errado.
bool lerValor(const std::string& nome, std::wstring& out, DWORD* bytes = nullptr) {
  HKEY key{};
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyW, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    return false;
  std::wstring wn = utf8ToWide(nome);
  DWORD type = 0, size = 0;
  bool ok = false;
  if (RegQueryValueExW(key, wn.c_str(), nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
      type == REG_SZ) {
    std::wstring buf(size / sizeof(wchar_t) + 1, L'\0');
    DWORD sz2 = size;
    if (RegQueryValueExW(key, wn.c_str(), nullptr, &type, (BYTE*)&buf[0], &sz2) == ERROR_SUCCESS) {
      out = buf.c_str();
      if (bytes) *bytes = sz2;
      ok = true;
    }
  }
  RegCloseKey(key);
  return ok;
}
}  // namespace

TEST(autostart, habilita_le_e_desabilita) {
  Guard g(nomeUnico());
  CHECK(!autostartEnabled(g.nome.c_str()));

  const char* cmd = "C:\\nao\\existe\\tamapoke.exe --hidden";
  CHECK(autostartEnable(g.nome.c_str(), cmd));
  CHECK(autostartEnabled(g.nome.c_str()));

  std::wstring lido;
  DWORD bytes = 0;
  CHECK(lerValor(g.nome, lido, &bytes));
  CHECK(lido == utf8ToWide(cmd));
  CHECK_EQ((int)bytes, (int)((lido.size() + 1) * sizeof(wchar_t)));  // inclui o NUL

  CHECK(autostartDisable(g.nome.c_str()));
  CHECK(!autostartEnabled(g.nome.c_str()));
}

TEST(autostart, desabilitar_o_que_nao_existe_nao_eh_erro) {
  std::string n = nomeUnico("_nunca");
  CHECK(autostartDisable(n.c_str()));
}

TEST(autostart, habilitar_duas_vezes_sobrescreve) {
  Guard g(nomeUnico("_dup"));
  CHECK(autostartEnable(g.nome.c_str(), "C:\\a.exe"));
  CHECK(autostartEnable(g.nome.c_str(), "C:\\b.exe --hidden"));
  std::wstring lido;
  CHECK(lerValor(g.nome, lido));
  CHECK(lido == L"C:\\b.exe --hidden");
}

TEST(autostart, nome_e_comando_nao_ascii) {
  // "Jos" + U+00E9 + katakana, escritos como bytes UTF-8
  const std::string nome = nomeUnico("_Jos\xC3\xA9_\xE3\x83\x9D");
  Guard g(nome);
  const std::string cmd =
      "\"C:\\Users\\Jos\xC3\xA9\\AppData\\Local\\TamaPoke\\tamapoke.exe\" --hidden";
  CHECK(autostartEnable(nome.c_str(), cmd.c_str()));
  CHECK(autostartEnabled(nome.c_str()));
  std::wstring lido;
  CHECK(lerValor(nome, lido));
  CHECK(lido == utf8ToWide(cmd));
  CHECK(lido.find(L"Jos\u00E9") != std::wstring::npos);
  CHECK(autostartDisable(nome.c_str()));
  CHECK(!autostartEnabled(nome.c_str()));
}

TEST(autostart, entradas_invalidas_falham) {
  CHECK(!autostartEnable(nullptr, "x"));
  CHECK(!autostartEnable("TamaPokeTest_nulo", nullptr));
  CHECK(!autostartEnabled(nullptr));
}
