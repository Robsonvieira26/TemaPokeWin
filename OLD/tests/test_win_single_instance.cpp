// tests/test_win_single_instance.cpp
// Usa nomes unicos por execucao para nunca interferir num TamaPoke real aberto.
#include "framework.h"
#include "win/single_instance.h"
#include <windows.h>
#include <string>
#include <thread>

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

// ---- --quit: pedido de saida da instancia em execucao ----

TEST(single_instance, quit_pedido_chega_uma_vez_e_colapsa) {
  std::string n = base("_q1");
  SingleInstance a;
  CHECK(a.acquire(n.c_str()));
  CHECK(!a.quitRequested());
  CHECK(SingleInstance::requestQuit(n.c_str()));
  CHECK(SingleInstance::requestQuit(n.c_str()));
  CHECK(SingleInstance::requestQuit(n.c_str()));
  CHECK(a.quitRequested());
  CHECK(!a.quitRequested());
}

TEST(single_instance, quit_sem_primaria_nao_cria_nada) {
  std::string n = base("_q2");
  CHECK(!SingleInstance::requestQuit(n.c_str()));
  CHECK(!SingleInstance::isRunning(n.c_str()));
  SingleInstance a;
  CHECK(a.acquire(n.c_str()));
  CHECK(!a.quitRequested());  // o pedido perdido nao ficou pendente
  CHECK(SingleInstance::isRunning(n.c_str()));
}

TEST(single_instance, quit_requested_antes_de_acquire_e_apos_release) {
  std::string n = base("_q3");
  SingleInstance a;
  CHECK(!a.quitRequested());
  CHECK(a.acquire(n.c_str()));
  a.release();
  CHECK(!a.quitRequested());
}

TEST(single_instance, is_running_segue_o_ciclo_de_vida) {
  std::string n = base("_q4");
  CHECK(!SingleInstance::isRunning(n.c_str()));
  SingleInstance a;
  CHECK(a.acquire(n.c_str()));
  CHECK(SingleInstance::isRunning(n.c_str()));
  a.release();
  CHECK(!SingleInstance::isRunning(n.c_str()));
}

TEST(single_instance, wait_for_exit_imediato_quando_nao_roda) {
  std::string n = base("_q5");
  ULONGLONG t0 = GetTickCount64();
  CHECK(SingleInstance::waitForExit(n.c_str(), 5000));
  CHECK(GetTickCount64() - t0 < 500);
}

TEST(single_instance, wait_for_exit_estoura_o_prazo) {
  std::string n = base("_q6");
  SingleInstance a;
  CHECK(a.acquire(n.c_str()));
  ULONGLONG t0 = GetTickCount64();
  CHECK(!SingleInstance::waitForExit(n.c_str(), 150));
  ULONGLONG dt = GetTickCount64() - t0;
  CHECK(dt >= 130);
  CHECK(dt < 2000);
}

TEST(single_instance, wait_for_exit_ve_a_primaria_sair_em_outra_thread) {
  std::string n = base("_q7");
  SingleInstance a;
  CHECK(a.acquire(n.c_str()));
  std::thread t([&a] {
    Sleep(100);
    a.release();
  });
  ULONGLONG t0 = GetTickCount64();
  bool ok = SingleInstance::waitForExit(n.c_str(), 5000);
  t.join();
  CHECK(ok);
  CHECK(GetTickCount64() - t0 < 2000);
}

TEST(single_instance, show_e_quit_sao_independentes) {
  std::string n = base("_q8");
  SingleInstance a, b;
  CHECK(a.acquire(n.c_str()));
  CHECK(SingleInstance::requestQuit(n.c_str()));
  CHECK(!a.showRequested());
  CHECK(a.quitRequested());
  CHECK(!b.acquire(n.c_str()));
  CHECK(!a.quitRequested());
  CHECK(a.showRequested());
}

TEST(single_instance, quit_com_nome_utf8) {
  std::string n = base("_Jos\xC3\xA9_q9");
  SingleInstance a;
  CHECK(a.acquire(n.c_str()));
  CHECK(SingleInstance::isRunning(n.c_str()));
  CHECK(SingleInstance::requestQuit(n.c_str()));
  CHECK(a.quitRequested());
  a.release();
  CHECK(!SingleInstance::isRunning(n.c_str()));
}
