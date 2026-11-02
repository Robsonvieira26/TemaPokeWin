// tests/test_hal_sd.cpp
#include "framework.h"
#include "SD_MMC.h"
#include "hal_control.h"
#include <filesystem>
#include <fstream>
#include <string>

static std::filesystem::path prepararCartao() {
  auto root = std::filesystem::temp_directory_path() / "tamapoke_sd";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root / "mons", ec);
  std::ofstream f(root / "mons" / "p001.bin", std::ios::binary);
  const char data[8] = {'T','A','M','A',1,2,3,4};
  f.write(data, sizeof data);
  f.close();
  static std::string keep;
  keep = root.string();
  halSetSdRoot(keep.c_str());
  return root;
}

TEST(hal_sd, abre_le_e_fecha_um_arquivo_existente) {
  prepararCartao();
  ASSERT_TRUE(SD_MMC.begin("/sdcard", true, true));
  File f = SD_MMC.open("/mons/p001.bin", FILE_READ);
  ASSERT_TRUE((bool)f);
  ASSERT_EQ((int)f.size(), 8);
  uint8_t buf[8] = {0};
  ASSERT_EQ((int)f.read(buf, 8), 8);
  ASSERT_EQ((int)buf[0], 'T');
  ASSERT_EQ((int)buf[7], 4);
  f.close();
  ASSERT_TRUE(!(bool)f);
}

TEST(hal_sd, arquivo_inexistente_da_File_falso) {
  prepararCartao();
  File f = SD_MMC.open("/mons/naoexiste.bin", FILE_READ);
  ASSERT_TRUE(!(bool)f);
  ASSERT_EQ((int)f.size(), 0);
  uint8_t b = 0;
  ASSERT_EQ((int)f.read(&b, 1), 0);
}

TEST(hal_sd, openNextFile_lista_o_diretorio_e_termina) {
  prepararCartao();
  File dir = SD_MMC.open("/mons");  // modo padrao = FILE_READ
  ASSERT_TRUE((bool)dir);
  int n = 0;
  File e;
  bool achou = false;
  while ((e = dir.openNextFile())) {
    if (std::string(e.name()) == "p001.bin") { achou = true; ASSERT_EQ((int)e.size(), 8); }
    e.close();
    n++;
  }
  ASSERT_EQ(n, 1);
  ASSERT_TRUE(achou);
  dir.close();
}

TEST(hal_sd, mkdir_exists_escrita_e_remove) {
  auto root = prepararCartao();
  ASSERT_TRUE(SD_MMC.mkdir("/novo"));
  ASSERT_TRUE(SD_MMC.exists("/novo"));
  ASSERT_TRUE(!SD_MMC.exists("/novo/x.bin"));
  File f = SD_MMC.open("/novo/x.bin", FILE_WRITE);
  ASSERT_TRUE((bool)f);
  const uint8_t b[2] = {9, 9};
  ASSERT_EQ((int)f.write(b, 2), 2);
  f.close();
  ASSERT_TRUE(std::filesystem::exists(root / "novo" / "x.bin"));
  ASSERT_TRUE(SD_MMC.remove("/novo/x.bin"));
  ASSERT_TRUE(!SD_MMC.exists("/novo/x.bin"));
}

TEST(hal_sd, handle_fecha_quando_a_ultima_copia_morre) {
  auto root = prepararCartao();
  {
    File a = SD_MMC.open("/w.bin", FILE_WRITE);
    ASSERT_TRUE((bool)a);
    File b = a;  // copia barata, mesmo handle
    const uint8_t d[3] = {1, 2, 3};
    ASSERT_EQ((int)b.write(d, 3), 3);
  }  // sem close(): o destrutor da ultima copia deve dar fclose (flush)
  ASSERT_EQ((int)std::filesystem::file_size(root / "w.bin"), 3);
}

TEST(hal_sd, cardSize_e_bytes_nao_sao_zero_quando_a_raiz_existe) {
  prepararCartao();
  ASSERT_TRUE(SD_MMC.cardSize() > 0);
  ASSERT_TRUE(SD_MMC.totalBytes() > 0);
  ASSERT_TRUE(SD_MMC.usedBytes() <= SD_MMC.totalBytes());
}

TEST(hal_sd, begin_falha_se_a_raiz_nao_existe) {
  auto root = prepararCartao();
  static std::string fora;
  fora = (root / "nao_existe").string();
  halSetSdRoot(fora.c_str());
  ASSERT_TRUE(!SD_MMC.begin());
  prepararCartao();
}

TEST(hal_sd, caminhos_nao_escapam_da_raiz) {
  auto root = prepararCartao();
  { std::ofstream o(root.parent_path() / "tamapoke_fora.bin", std::ios::binary); o << "x"; }
  ASSERT_TRUE(std::filesystem::exists(root.parent_path() / "tamapoke_fora.bin"));
  ASSERT_TRUE(!(bool)SD_MMC.open("/../tamapoke_fora.bin", FILE_READ));
  ASSERT_TRUE(!(bool)SD_MMC.open("/mons/../../tamapoke_fora.bin", FILE_READ));
  ASSERT_TRUE(!(bool)SD_MMC.open("..\tamapoke_fora.bin", FILE_READ));
  ASSERT_TRUE(!(bool)SD_MMC.open("/..", FILE_READ));
  ASSERT_TRUE(!(bool)SD_MMC.open("/C:/Windows/win.ini", FILE_READ));
  ASSERT_TRUE(!SD_MMC.exists("/../tamapoke_fora.bin"));
  ASSERT_TRUE(!SD_MMC.mkdir("/../tamapoke_fora_dir"));
  ASSERT_TRUE(!SD_MMC.remove("/../tamapoke_fora.bin"));
  ASSERT_TRUE(!(bool)SD_MMC.open("/../tamapoke_novo.bin", FILE_WRITE));
  ASSERT_TRUE(std::filesystem::exists(root.parent_path() / "tamapoke_fora.bin"));
  ASSERT_TRUE(!std::filesystem::exists(root.parent_path() / "tamapoke_novo.bin"));
  std::error_code ec;
  std::filesystem::remove(root.parent_path() / "tamapoke_fora.bin", ec);
}
