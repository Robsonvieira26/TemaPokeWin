// tests/test_settings.cpp
#include "framework.h"
#include "settings.h"
#include "hal_control.h"
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
std::filesystem::path dirLimpo(const char* nome) {
  auto d = std::filesystem::temp_directory_path() / nome;
  std::error_code ec;
  std::filesystem::remove_all(d, ec);
  std::filesystem::create_directories(d, ec);
  halSetDataDir(d.string().c_str());
  return d;
}

void limpa(const std::filesystem::path& d) {
  std::error_code ec;
  std::filesystem::remove_all(d, ec);
}

void escreve(const std::filesystem::path& d, const char* conteudo) {
  std::string path = (d / "settings.json").string();
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (f) { std::fputs(conteudo, f); std::fclose(f); }
}

void checaDefaults(const Settings& s) {
  CHECK_EQ(s.scale, 1.0f);
  CHECK(s.alwaysOnTop);
  CHECK(s.autostart);
  CHECK(s.soundEnabled);
  CHECK(!s.hasPosition);
  CHECK_EQ(s.windowX, -1);
  CHECK_EQ(s.windowY, -1);
  CHECK(!s.startHidden);
}
}  // namespace

TEST(settings, defaults_do_spec) {
  auto d = dirLimpo("tamapoke_set_a");
  checaDefaults(settingsLoad());
  limpa(d);
}

TEST(settings, round_trip) {
  auto d = dirLimpo("tamapoke_set_b");
  Settings s = settingsLoad();
  s.scale = 2.0f; s.alwaysOnTop = false; s.autostart = false;
  s.soundEnabled = false; s.hasPosition = true; s.windowX = 300; s.windowY = 400;
  settingsSave(s);

  Settings r = settingsLoad();
  CHECK_EQ(r.scale, 2.0f);
  CHECK(!r.alwaysOnTop);
  CHECK(!r.autostart);
  CHECK(!r.soundEnabled);
  CHECK(r.hasPosition);
  CHECK_EQ(r.windowX, 300);
  CHECK_EQ(r.windowY, 400);
  limpa(d);
}

TEST(settings, escala_meia_faz_round_trip_exato) {
  auto d = dirLimpo("tamapoke_set_half");
  Settings s;
  s.scale = 0.5f;
  settingsSave(s);
  CHECK_EQ(settingsLoad().scale, 0.5f);
  limpa(d);
}

TEST(settings, posicao_negativa_real_sobrevive) {
  auto d = dirLimpo("tamapoke_set_neg");
  Settings s;
  s.hasPosition = true; s.windowX = -1200; s.windowY = -30;
  settingsSave(s);
  Settings r = settingsLoad();
  CHECK(r.hasPosition);
  CHECK_EQ(r.windowX, -1200);
  CHECK_EQ(r.windowY, -30);
  limpa(d);
}

TEST(settings, sem_posicao_nao_grava_posicao) {
  auto d = dirLimpo("tamapoke_set_nopos");
  Settings s;
  s.windowX = 55; s.windowY = 66;   // hasPosition=false: ignorados
  settingsSave(s);
  Settings r = settingsLoad();
  CHECK(!r.hasPosition);
  CHECK_EQ(r.windowX, -1);
  CHECK_EQ(r.windowY, -1);
  limpa(d);
}

TEST(settings, startHidden_nao_eh_persistido) {
  auto d = dirLimpo("tamapoke_set_hidden");
  Settings s;
  s.startHidden = true;
  settingsSave(s);
  CHECK(!settingsLoad().startHidden);
  limpa(d);
}

TEST(settings, arquivo_corrompido_volta_aos_defaults) {
  auto d = dirLimpo("tamapoke_set_c");
  escreve(d, "lixo");
  checaDefaults(settingsLoad());
  escreve(d, "");
  checaDefaults(settingsLoad());
  limpa(d);
}

TEST(settings, raiz_que_nao_eh_objeto_volta_aos_defaults) {
  auto d = dirLimpo("tamapoke_set_arr");
  escreve(d, "[1,2,3]");
  checaDefaults(settingsLoad());
  escreve(d, "42");
  checaDefaults(settingsLoad());
  limpa(d);
}

TEST(settings, campos_com_tipo_errado_usam_default) {
  auto d = dirLimpo("tamapoke_set_types");
  escreve(d, R"({"scale":"x","alwaysOnTop":"sim","autostart":0,"soundEnabled":null,"windowX":"a","windowY":2.5})");
  checaDefaults(settingsLoad());

  // um campo ruim nao derruba os bons
  escreve(d, R"({"scale":"x","alwaysOnTop":false,"windowX":10,"windowY":20})");
  Settings s = settingsLoad();
  CHECK_EQ(s.scale, 1.0f);
  CHECK(!s.alwaysOnTop);
  CHECK(s.hasPosition);
  CHECK_EQ(s.windowX, 10);
  CHECK_EQ(s.windowY, 20);
  limpa(d);
}

TEST(settings, posicao_so_com_um_eixo_eh_ignorada) {
  auto d = dirLimpo("tamapoke_set_half_pos");
  escreve(d, R"({"windowX":10})");
  Settings s = settingsLoad();
  CHECK(!s.hasPosition);
  CHECK_EQ(s.windowX, -1);
  limpa(d);
}

TEST(settings, chaves_extras_ignoradas_e_parcial_mantem_defaults) {
  auto d = dirLimpo("tamapoke_set_extra");
  escreve(d, R"({"futuro":{"a":1},"soundEnabled":false})");
  Settings s = settingsLoad();
  CHECK(!s.soundEnabled);
  CHECK(s.alwaysOnTop);
  CHECK(s.autostart);
  CHECK_EQ(s.scale, 1.0f);
  CHECK(!s.hasPosition);
  limpa(d);
}

TEST(settings, escala_fora_do_permitido_eh_corrigida) {
  auto d = dirLimpo("tamapoke_set_d");
  Settings s;
  s.scale = 7.0f;
  settingsSave(s);
  CHECK_EQ(settingsLoad().scale, 1.0f);   // so 0.5, 1, 2 sao validos

  escreve(d, R"({"scale":3})");
  CHECK_EQ(settingsLoad().scale, 1.0f);
  escreve(d, R"({"scale":0.0})");
  CHECK_EQ(settingsLoad().scale, 1.0f);
  escreve(d, R"({"scale":-2})");
  CHECK_EQ(settingsLoad().scale, 1.0f);
  escreve(d, R"({"scale":2})");               // inteiro no JSON
  CHECK_EQ(settingsLoad().scale, 2.0f);
  limpa(d);
}

TEST(settings, falha_ao_salvar_nao_quebra) {
  auto d = dirLimpo("tamapoke_set_ro");
  // Um diretorio no lugar do .tmp e do destino impede a escrita/rename.
  std::filesystem::create_directories(d / "settings.json.tmp");
  Settings s;
  s.scale = 2.0f;
  settingsSave(s);                      // nao pode lancar
  checaDefaults(settingsLoad());        // nada foi gravado
  limpa(d);
}
