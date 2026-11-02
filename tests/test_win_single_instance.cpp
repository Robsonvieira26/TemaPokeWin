// tests/test_win_single_instance.cpp
// Usa nomes unicos por execucao para nunca interferir num TamaPoke real aberto.
#include "framework.h"
#include "win/single_instance.h"
#include <windows.h>
#include <string>

namespace {
std::string base(const char* sufixo) {
  return "TamaPokeTest_" + std::to_string(GetCurrentProcessId()) + "_" +
         std::to_string(GetTickCount64()) + sufixo;
}
}  // namespace

TEST(single_instance, segunda_instancia_falha_e_sinaliza_a_primeira) {
  std::string n = base("_a");
  SingleInstance a, b;
  CHECK(a.acquire(n.c_str()));
  CHECK(!a.showRequested());
  CHECK(!b.acquire(n.c_str()));
  CHECK(a.showRequested());   // uma vez...
  CHECK(!a.showRequested());  // ...e some (auto-reset)
}

TEST(single_instance, pedidos_se_colapsam_em_um) {
  std::string n = base("_b");
  SingleInstance a, b, c;
  CHECK(a.acquire(n.c_str()));
  CHECK(!b.acquire(n.c_str()));
  CHECK(!c.acquire(n.c_str()));
  CHECK(a.showRequested());
  CHECK(!a.showRequested());
}

TEST(single_instance, apos_release_outra_adquire) {
  std::string n = base("_c");
  SingleInstance a;
  CHECK(a.acquire(n.c_str()));
  a.release();
  SingleInstance c;
  CHECK(c.acquire(n.c_str()));
}

TEST(single_instance, destrutor_libera) {
  std::string n = base("_d");
  {
    SingleInstance a;
    CHECK(a.acquire(n.c_str()));
  }
  SingleInstance c;
  CHECK(c.acquire(n.c_str()));
}

TEST(single_instance, nomes_diferentes_nao_interferem) {
  std::string n1 = base("_e1"), n2 = base("_e2");
  SingleInstance a, b;
  CHECK(a.acquire(n1.c_str()));
  CHECK(b.acquire(n2.c_str()));
  CHECK(!a.showRequested());
  CHECK(!b.showRequested());
}

TEST(single_instance, release_idempotente_e_sem_acquire) {
  SingleInstance a;
  a.release();                  // nunca adquiriu
  CHECK(!a.showRequested());
  std::string n = base("_f");
  CHECK(a.acquire(n.c_str()));
  a.release();
  a.release();
  CHECK(!a.showRequested());
}

TEST(single_instance, acquire_duas_vezes_no_mesmo_objeto) {
  std::string n = base("_g");
  SingleInstance a;
  CHECK(a.acquire(n.c_str()));
  CHECK(a.acquire(n.c_str()));  // ja e dono: true, sem sinalizar
  CHECK(!a.showRequested());
  SingleInstance b;
  CHECK(!b.acquire(n.c_str()));
  CHECK(a.showRequested());
}

TEST(single_instance, nome_com_utf8) {
  std::string n = base("_Jos\xC3\xA9");
  SingleInstance a, b;
  CHECK(a.acquire(n.c_str()));
  CHECK(!b.acquire(n.c_str()));
  CHECK(a.showRequested());
}
