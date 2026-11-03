// tests/test_host_offline.cpp - progressao offline (Pet::syncClock) atraves do RTC da HAL.
//
// Regra REAL (upstream/pet.cpp, Pet::syncClock; NAO e "+1 nivel por hora"):
//  - mins = (agora - "seen") / 60, so se houver "seen" salvo e agora > seen; senao 0.
//  - mins < 2, cerimonia em curso ou escolha de inicial pendente: nada e aplicado.
//  - tope de 14 dias (20160 min).
//  - por minuto acordado: ageMinutes++, comida -2, energia/higiene/alegria -1, cada uma
//    com piso 15 (dropTo: quem ja esta <= piso fica como esta). Sem descuidos.
//  - caca: poops += mins/240, teto 3 (nao se dormindo).
//  - a evolucao NAO e aplicada offline: fica pronta (canEvolveNow) para o usuario.
// O nivel so deriva da idade (1 + age/60), entao "ganhar nivel" = 1 nivel por hora de
// jogo aplicada, nao por hora desligada em bruto alem do tope.
//
// Estrategia: o 1o host cria o bicho pela API publica de Pet (chooseStarter + eggTap) e
// fixa o estado; pet.setClock(rtcEpoch()) persiste tudo (Pet::save e privado) com
// "seen" = agora. Depois halSetRtcOffsetForTest(N) e um novo host no MESMO diretorio.
// Tolerancia: o offset e sempre (minutos*60 + 20 s); os segundos reais que passam entre
// o setClock e o 2o boot ficam muito abaixo dos 40 s de folga, e uma pre-condicao
// confere isso (o minuto aplicado e floor(delta/60)).
#include "framework.h"
#include "game_host.h"
#include "hal_control.h"
#include "Arduino.h"
#include "pet.h"
#include "rtcbat.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

extern Pet pet;

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

const char* kSd = "upstream/tools/sdcard";

std::string dirLimpo(const char* nome) {
  auto d = fs::temp_directory_path() / nome;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d.string();
}
void removerDir(const std::string& d) {
  std::error_code ec;
  fs::remove_all(d, ec);
}

struct Estado {
  uint8_t full, joy, ene, hyg, poops, mist;
  uint32_t age, seen;
  int16_t sp, prevSp;
  bool sleeping, starter, canEvolve;
  uint16_t level;
};

Estado ler() {
  Estado e;
  e.full = pet.fullness; e.joy = pet.joy; e.ene = pet.energy; e.hyg = pet.hygiene;
  e.poops = pet.poops; e.mist = pet.careMistakes;
  e.age = pet.ageMinutes; e.seen = pet.lastSeenEpoch;
  e.sp = pet.speciesId; e.prevSp = pet.prevSpeciesId;
  e.sleeping = pet.sleeping; e.starter = pet.awaitingStarter();
  e.canEvolve = pet.canEvolveNow(); e.level = pet.level();
  return e;
}

void assentar(IGameHost* h, int ms) {
  auto fim = Clock::now() + std::chrono::milliseconds(ms);
  while (Clock::now() < fim) h->tick();
}

// Sobe o jogo com o RTC deslocado de `offset` s, deixa assentar e le o estado.
template <class F>
void sessao(const std::string& data, int64_t offset, F fn) {
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  halTouchUp();
  halSetRtcOffsetForTest(offset);
  IGameHost* h = createGameHost(data.c_str(), kSd);
  h->boot();
  assentar(h, 200);
  fn();
  destroyGameHost(h);
  halSetRtcOffsetForTest(0);
}

// 1a sessao: bicho eclodido (Charmander), estado fixado e persistido com seen = agora.
Estado criarBicho(const std::string& data, uint32_t age, uint8_t f, uint8_t j, uint8_t e,
                  uint8_t h) {
  Estado r{};
  sessao(data, 0, [&] {
    if (pet.awaitingStarter()) pet.chooseStarter(4);
    pet.eggTap(); pet.eggTap(); pet.eggTap();  // 3 toques: eclode na hora
    pet.ageMinutes = age;
    pet.fullness = f; pet.joy = j; pet.energy = e; pet.hygiene = h;
    pet.poops = 0; pet.sleeping = false; pet.careMistakes = 0;
    pet.setClock(rtcEpoch());  // persiste tudo, com "seen" = agora
    r = ler();
  });
  return r;
}

Estado reabrir(const std::string& data, int64_t offset) {
  Estado r{};
  sessao(data, offset, [&] { r = ler(); });
  return r;
}

// Modelo de referencia, independente do firmware: fechado, sem laco.
uint8_t baixa(uint8_t v, int perMin, uint32_t mins, uint8_t piso) {
  if (v <= piso) return v;
  int64_t n = (int64_t)v - (int64_t)perMin * mins;
  return n < piso ? piso : (uint8_t)n;
}
uint8_t cacas(uint8_t p, uint32_t mins) {
  uint32_t n = p + mins / 240;
  return n > 3 ? 3 : (uint8_t)n;
}

constexpr uint32_t kTopeMin = 14u * 24 * 60;

}  // namespace

TEST(offline_host, progressao_parcial_confere_com_a_regra) {
  std::string data = dirLimpo("tamapoke_off_a");
  const uint32_t mins = 20;
  Estado a = criarBicho(data, 0, 80, 80, 80, 100);
  ASSERT_EQ(a.sp, (int16_t)4);
  Estado b = reabrir(data, mins * 60 + 20);
  ASSERT_EQ((b.seen - a.seen) / 60, mins);  // pre-condicao: o relogio andou o esperado
  CHECK_EQ(b.age, a.age + mins);
  CHECK_EQ(b.full, baixa(80, 2, mins, 15));  // 40
  CHECK_EQ(b.ene, baixa(80, 1, mins, 15));   // 60
  CHECK_EQ(b.joy, baixa(80, 1, mins, 15));   // 60
  CHECK_EQ(b.hyg, baixa(100, 1, mins, 15));  // 80
  CHECK_EQ(b.poops, cacas(0, mins));
  CHECK_EQ(b.mist, (uint8_t)0);
  CHECK_EQ(b.sp, (int16_t)4);
  CHECK_EQ(b.level, a.level);  // 20 min nao completam uma hora de jogo
  removerDir(data);
}

TEST(offline_host, dez_horas_apagado_chegam_ao_piso_de_15) {
  std::string data = dirLimpo("tamapoke_off_b");
  const uint32_t mins = 600;  // o mesmo caso de aplica_el_tiempo_apagado_con_suelo_de_15
  Estado a = criarBicho(data, 0, 80, 80, 80, 100);
  Estado b = reabrir(data, mins * 60 + 20);
  ASSERT_EQ((b.seen - a.seen) / 60, mins);
  CHECK_EQ(b.age, (uint32_t)600);
  CHECK_EQ(b.full, (uint8_t)15);
  CHECK_EQ(b.ene, (uint8_t)15);
  CHECK_EQ(b.joy, (uint8_t)15);
  CHECK_EQ(b.hyg, (uint8_t)15);
  CHECK_EQ(b.poops, (uint8_t)2);       // uma caca a cada 4 h
  CHECK_EQ(b.mist, (uint8_t)0);        // sem descuidos em ausencia
  CHECK_EQ(b.level, (uint16_t)(1 + 600 / MINUTES_PER_LEVEL));  // 1 nivel por hora de jogo
  removerDir(data);
}

TEST(offline_host, menos_de_dois_minutos_nao_faz_nada) {
  std::string data = dirLimpo("tamapoke_off_c");
  Estado a = criarBicho(data, 100, 80, 70, 60, 90);
  Estado b = reabrir(data, 90);  // 90 s (+ segundos reais) = 1 minuto
  ASSERT_TRUE((b.seen - a.seen) / 60 < 2);  // pre-condicao
  CHECK_EQ(b.age, a.age);
  CHECK_EQ(b.full, (uint8_t)80);
  CHECK_EQ(b.joy, (uint8_t)70);
  CHECK_EQ(b.ene, (uint8_t)60);
  CHECK_EQ(b.hyg, (uint8_t)90);
  CHECK_EQ(b.poops, (uint8_t)0);
  removerDir(data);
}

TEST(offline_host, tope_de_duas_semanas) {
  std::string d90 = dirLimpo("tamapoke_off_d90");
  std::string d14 = dirLimpo("tamapoke_off_d14");
  Estado a90 = criarBicho(d90, 0, 80, 80, 80, 100);
  Estado a14 = criarBicho(d14, 0, 80, 80, 80, 100);
  Estado b90 = reabrir(d90, 90LL * 86400 + 20);
  Estado b14 = reabrir(d14, 14LL * 86400 + 20);
  ASSERT_TRUE((b90.seen - a90.seen) / 60 > kTopeMin);
  ASSERT_TRUE((b14.seen - a14.seen) / 60 >= kTopeMin);
  CHECK_EQ(b90.age, kTopeMin);
  CHECK_EQ(b14.age, kTopeMin);
  CHECK_EQ(b90.full, b14.full);
  CHECK_EQ(b90.ene, b14.ene);
  CHECK_EQ(b90.joy, b14.joy);
  CHECK_EQ(b90.hyg, b14.hyg);
  CHECK_EQ(b90.poops, b14.poops);
  CHECK_EQ(b90.level, (uint16_t)(1 + kTopeMin / MINUTES_PER_LEVEL));
  // as 4 barras ficam no piso, nunca abaixo
  CHECK_EQ(b90.full, (uint8_t)15);
  CHECK_EQ(b90.hyg, (uint8_t)15);
  CHECK_EQ(b90.poops, (uint8_t)3);
  removerDir(d90);
  removerDir(d14);
}

TEST(offline_host, relogio_voltando_no_tempo_nao_aplica_nada) {
  std::string data = dirLimpo("tamapoke_off_e");
  Estado a = criarBicho(data, 300, 80, 70, 60, 90);
  // 1 h para tras: agora < seen. O upstream so subtrai quando agora > seen (sem
  // underflow do uint32_t), entao mins = 0: nada muda. O efeito colateral e que
  // "seen" passa a ser a hora antiga (lastSeenEpoch = agora), documentado aqui.
  Estado b = reabrir(data, -3600);
  ASSERT_TRUE(b.seen < a.seen);
  CHECK_EQ(b.age, a.age);
  CHECK_EQ(b.full, (uint8_t)80);
  CHECK_EQ(b.joy, (uint8_t)70);
  CHECK_EQ(b.ene, (uint8_t)60);
  CHECK_EQ(b.hyg, (uint8_t)90);
  CHECK_EQ(b.poops, (uint8_t)0);
  removerDir(data);
}

TEST(offline_host, rtc_invalido_por_ser_muito_antigo_nao_aplica_nada) {
  std::string data = dirLimpo("tamapoke_off_f");
  Estado a = criarBicho(data, 300, 80, 70, 60, 90);
  // 10 anos para tras: ano < 2025 -> rtcEpoch() = 0; o setup() resemeia o RTC com o
  // ultimo "seen" (nunca retrocede o tempo de jogo), logo nada e aplicado.
  Estado b = reabrir(data, -10LL * 365 * 86400);
  CHECK_EQ(b.age, a.age);
  CHECK_EQ(b.full, (uint8_t)80);
  CHECK_EQ(b.ene, (uint8_t)60);
  CHECK_EQ(b.sp, (int16_t)4);
  CHECK_MSG(b.seen >= a.seen, "o tempo de jogo nao deve retroceder");
  removerDir(data);
}

TEST(offline_host, primeiro_boot_nao_aplica_progressao) {
  std::string data = dirLimpo("tamapoke_off_g");
  // diretorio virgem com o relogio 10 dias a frente: ainda nao ha "seen"
  Estado b = reabrir(data, 10LL * 86400);
  CHECK(b.starter);
  CHECK_EQ(b.age, (uint32_t)0);
  CHECK_EQ(b.full, (uint8_t)80);
  CHECK_EQ(b.hyg, (uint8_t)100);
  CHECK(b.seen != 0);
  removerDir(data);
}

TEST(offline_host, evolucao_nao_e_aplicada_offline_mas_fica_pronta) {
  std::string data = dirLimpo("tamapoke_off_h");
  // Charmander evolui no nivel 16 (idade 900). Antes: nivel 15, ainda nao pode.
  Estado a = criarBicho(data, 890, 80, 80, 80, 100);
  ASSERT_EQ(a.level, (uint16_t)15);
  ASSERT_TRUE(!a.canEvolve);
  Estado b = reabrir(data, 20 * 60 + 20);  // +20 min: nivel 16, barras >= 40
  ASSERT_EQ((b.seen - a.seen) / 60, (uint32_t)20);
  CHECK_EQ(b.level, (uint16_t)16);
  CHECK(b.canEvolve);               // pronta para o usuario disparar
  CHECK_EQ(b.sp, (int16_t)4);       // mas NAO evoluiu sozinha
  CHECK_EQ(b.prevSp, a.prevSp);
  removerDir(data);
}
