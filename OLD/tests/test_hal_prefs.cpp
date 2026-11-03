// tests/test_hal_prefs.cpp
#include "framework.h"
#include "Preferences.h"
#include "hal_control.h"
#include <cstdio>
#include <filesystem>
#include <string>

static void usarDirTemporario(const char* nome) {
  auto dir = std::filesystem::temp_directory_path() / nome;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  static std::string keep;
  keep = dir.string();
  halSetDataDir(keep.c_str());
}

TEST(hal_prefs, round_trip_sobrevive_a_recarga) {
  usarDirTemporario("tamapoke_prefs_a");
  {
    Preferences p;
    ASSERT_TRUE(p.begin("pet", false));
    p.putUInt("ep", 123u);
    p.putUChar("lvl", 7);
    p.putString("nick", "Pika");
    const uint8_t blob[3] = {1, 2, 3};
    p.putBytes("blob", blob, sizeof blob);
    p.end();
  }
  {
    Preferences p;
    ASSERT_TRUE(p.begin("pet", true));
    ASSERT_EQ((int)p.getUInt("ep", 0), 123);
    ASSERT_EQ((int)p.getUChar("lvl", 0), 7);
    char nick[16] = {0};
    p.getString("nick", nick, sizeof nick);
    ASSERT_TRUE(std::string(nick) == "Pika");
    uint8_t blob[3] = {0};
    ASSERT_EQ((int)p.getBytes("blob", blob, sizeof blob), 3);
    ASSERT_EQ((int)blob[2], 3);
    p.end();
  }
}

TEST(hal_prefs, tipo_errado_devolve_default_como_na_nvs) {
  usarDirTemporario("tamapoke_prefs_b");
  Preferences p;
  p.begin("pet", false);
  p.putUChar("lvl", 7);
  ASSERT_EQ((int)p.getUInt("lvl", 99u), 99);   // gravado como UChar, lido como UInt
  ASSERT_EQ((int)p.getUChar("lvl", 0), 7);
  p.end();
}

TEST(hal_prefs, clear_so_apaga_o_proprio_namespace) {
  usarDirTemporario("tamapoke_prefs_c");
  { Preferences p; p.begin("pet", false); p.putUInt("ep", 1u); p.end(); }
  { Preferences p; p.begin("i18n", false); p.putUChar("lang", 3); p.end(); }
  { Preferences p; p.begin("pet", false); p.clear(); p.end(); }
  { Preferences p; p.begin("pet", true);  ASSERT_TRUE(!p.isKey("ep")); p.end(); }
  { Preferences p; p.begin("i18n", true); ASSERT_EQ((int)p.getUChar("lang", 0), 3); p.end(); }
}

TEST(hal_prefs, isKey_e_remove) {
  usarDirTemporario("tamapoke_prefs_d");
  Preferences p;
  p.begin("pet", false);
  p.putUInt("ep", 5u);
  ASSERT_TRUE(p.isKey("ep"));
  p.remove("ep");
  ASSERT_TRUE(!p.isKey("ep"));
  p.end();
}

TEST(hal_prefs, getString_respeita_maxLen) {
  usarDirTemporario("tamapoke_prefs_e");
  Preferences p;
  p.begin("pet", false);
  p.putString("nick", "NomeMuitoLongo");
  char buf[5] = {0};
  p.getString("nick", buf, sizeof buf);
  ASSERT_EQ((int)std::string(buf).size(), 4);  // 4 chars + terminador
  p.end();
}

TEST(hal_prefs, arquivo_corrompido_nao_trava_e_vira_bad) {
  usarDirTemporario("tamapoke_prefs_f");
  std::string path = std::string(halDataDir()) + "/nvs.json";
  { std::FILE* f = std::fopen(path.c_str(), "wb"); std::fputs("{nao e json", f); std::fclose(f); }
  Preferences p;
  ASSERT_TRUE(p.begin("pet", true));            // nao trava
  ASSERT_EQ((int)p.getUInt("ep", 42u), 42);     // comeca limpo
  p.end();
  bool achouBad = false;
  for (auto& e : std::filesystem::directory_iterator(halDataDir()))
    if (e.path().filename().string().rfind("nvs.json.bad-", 0) == 0) achouBad = true;
  ASSERT_TRUE(achouBad);
}

// Extra: dois namespaces coexistem, sobrescrita vence, e a releitura no mesmo
// processo (sem destruir o objeto) ve o valor novo; handle read-only nao grava.
TEST(hal_prefs, namespaces_coexistem_sobrescrita_e_readonly_nao_grava) {
  usarDirTemporario("tamapoke_prefs_g");
  Preferences a, b;
  ASSERT_TRUE(a.begin("pet", false));
  ASSERT_TRUE(b.begin("i18n", false));
  a.putString("nick", "Um");
  a.putString("nick", "Dois");
  b.putString("nick", "Outro");
  char buf[16] = {0};
  a.getString("nick", buf, sizeof buf);
  CHECK_STREQ(buf, "Dois");
  b.getString("nick", buf, sizeof buf);
  CHECK_STREQ(buf, "Outro");
  Preferences ro;
  ASSERT_TRUE(ro.begin("pet", true));
  ASSERT_EQ((int)ro.putUChar("x", 1), 0);
  ASSERT_TRUE(!ro.isKey("x"));
  ASSERT_TRUE(!ro.clear());
  CHECK_EQ((int)a.getBytes("ausente", buf, sizeof buf), 0);
  a.end(); b.end(); ro.end();
}
