// tests/test_audio.cpp
#include "framework.h"
#include "Arduino.h"
#include "hal_control.h"
#include "freertos_shim.h"
#include "audio.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

// audio.cpp tem estado global (fila, tarefa, ligado/desligado): cada teste
// chama preparar(), que inicializa uma unica vez por processo.
static bool gPronto = false;

static void preparar() {
  if (gPronto) return;
  gPronto = true;
  // Nada de gravar no repo: Preferences usa halDataDir().
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "tamapoke_test_audio";
  std::filesystem::create_directories(dir);
  static std::string dirStr = dir.string();
  halSetDataDir(dirStr.c_str());
  // Driver "dummy": sem placa de som, silencioso, mas consome em tempo real.
  SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
  bool ok = SDL_InitSubSystem(SDL_INIT_AUDIO);
  if (!ok) testFail(__FILE__, __LINE__, std::string("SDL audio nao inicializou: ") + SDL_GetError());
  audioBegin();
}

// Espera o audio ficar quieto (nenhum byte novo por 300 ms), no maximo 5 s.
static void esperarSilencio() {
  uint64_t ultimo = halAudioBytesWritten();
  int quieto = 0;
  for (int i = 0; i < 500 && quieto < 30; ++i) {
    SDL_Delay(10);
    uint64_t agora = halAudioBytesWritten();
    quieto = (agora == ultimo) ? quieto + 1 : 0;
    ultimo = agora;
  }
  CHECK_MSG(quieto >= 30, "audio nao silenciou em 5 s");
}

TEST(audio, som_habilitado_escreve_bytes) {
  preparar();
  // o jingle de arranque e escrito pela tarefa de audio, logo apos audioBegin
  for (int i = 0; i < 100 && halAudioBytesWritten() == 0; ++i) SDL_Delay(10);
  CHECK_MSG(halAudioBytesWritten() > 0, "I2S nao abriu (sem dispositivo SDL?) ou o jingle de arranque nao tocou");
  audioSetEnabled(true);
  esperarSilencio();
  uint64_t antes = halAudioBytesWritten();
  sfxPlay(SFX_TAP);
  for (int i = 0; i < 20 && halAudioBytesWritten() == antes; ++i) SDL_Delay(10);
  ASSERT_TRUE(halAudioBytesWritten() > antes);
}

TEST(audio, som_desabilitado_nao_escreve) {
  preparar();
  audioSetEnabled(false);
  esperarSilencio();   // deixa terminar um efeito que ja estava tocando
  uint64_t antes = halAudioBytesWritten();
  sfxPlay(SFX_TAP);
  SDL_Delay(200);
  ASSERT_EQ((long long)(halAudioBytesWritten() - antes), 0LL);
  audioSetEnabled(true);
}

TEST(audio, fila_do_shim_entrega_na_ordem) {
  QueueHandle_t q = xQueueCreate(4, sizeof(int));
  int a = 1, b = 2, lido = 0;
  ASSERT_EQ((int)xQueueSend(q, &a, 0), 1);
  ASSERT_EQ((int)xQueueSend(q, &b, 0), 1);
  ASSERT_EQ((int)xQueueReceive(q, &lido, 0), 1);
  ASSERT_EQ(lido, 1);
  ASSERT_EQ((int)xQueueReceive(q, &lido, 0), 1);
  ASSERT_EQ(lido, 2);
  ASSERT_EQ((int)xQueueReceive(q, &lido, 0), 0);   // vazia
  vQueueDelete(q);
}

TEST(audio, fila_cheia_recusa_envio_sem_espera) {
  QueueHandle_t q = xQueueCreate(2, sizeof(int));
  int v = 7;
  ASSERT_EQ((int)xQueueSend(q, &v, 0), 1);
  ASSERT_EQ((int)xQueueSend(q, &v, 0), 1);
  ASSERT_EQ((int)xQueueSend(q, &v, 0), 0);   // cheia
  vQueueDelete(q);
}

TEST(audio, receive_bloqueante_acorda_com_envio_de_outra_thread) {
  QueueHandle_t q = xQueueCreate(2, sizeof(int));
  std::thread produtor([q] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int v = 42;
    xQueueSend(q, &v, 0);
  });
  int lido = 0;
  ASSERT_EQ((int)xQueueReceive(q, &lido, 2000), 1);
  ASSERT_EQ(lido, 42);
  produtor.join();
  vQueueDelete(q);
}

TEST(audio, receive_com_timeout_falha_numa_fila_vazia) {
  QueueHandle_t q = xQueueCreate(2, sizeof(int));
  int lido = 0;
  ASSERT_EQ((int)xQueueReceive(q, &lido, 20), 0);
  vQueueDelete(q);
}
