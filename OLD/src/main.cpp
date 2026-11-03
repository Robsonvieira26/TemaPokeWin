// Ponto de entrada do desktop pet. Flags: --hidden, --console (abre o console de debug), --no-autostart, --data-dir <p>,
// --sd-root <p>, --quit (pede a instancia em execucao para salvar e sair), --instance <nome> (oculta; para fumaca/dev coexistir com uma instalacao real).
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
#include "app_shell.h"
#include "game_host.h"
#include "hal_control.h"
#include "settings.h"
#include "win/autostart.h"
#include "win/console.h"
#include "win/single_instance.h"

namespace {
std::string defaultDataDir() {
  PWSTR raw = nullptr;
  std::string out = ".";
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw))) {
    char utf8[MAX_PATH * 4] = {0};
    WideCharToMultiByte(CP_UTF8, 0, raw, -1, utf8, (int)sizeof utf8, nullptr, nullptr);
    out = std::string(utf8) + "\\TamaPoke";
  }
  if (raw) CoTaskMemFree(raw);
  return out;
}

void fatal(const char* msg) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TamaPoke", msg, nullptr);
}
}  // namespace

int main(int argc, char** argv) {
  bool quitOnly = false, hidden = false, noAutostart = false, openConsole = false;
  std::string dataDir, sdRoot, instance = "TamaPoke";
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const bool hasNext = i + 1 < argc;
    if (a == "--quit") quitOnly = true;
    else if (a == "--hidden") hidden = true;
    else if (a == "--console") openConsole = true;
    else if (a == "--no-autostart") noAutostart = true;
    else if (a == "--data-dir" && hasNext) dataDir = argv[++i];
    else if (a == "--sd-root" && hasNext) sdRoot = argv[++i];
    else if (a == "--instance" && hasNext) instance = argv[++i];
  }
  if (quitOnly) {
    // Sem efeitos colaterais: nada de dados, janela, mutex ou registro.
    if (!SingleInstance::requestQuit(instance.c_str())) return 0;  // nada rodando
    return SingleInstance::waitForExit(instance.c_str(), 10000) ? 0 : 2;
  }
  if (dataDir.empty()) dataDir = defaultDataDir();
  if (sdRoot.empty()) {
    const char* base = SDL_GetBasePath();
    sdRoot = std::string(base ? base : "./") + "sdcard";
  }

  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::u8path(dataDir), ec);
  halSetDataDir(dataDir.c_str());  // antes de qualquer coisa que possa logar (Serial -> log.txt)

  SingleInstance single;
  if (!single.acquire(instance.c_str())) return 0;  // a instancia em execucao foi avisada

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    fatal(SDL_GetError());
    return 1;
  }
  SDL_InitSubSystem(SDL_INIT_AUDIO);  // falha nao e fatal: o jogo roda mudo

  Settings settings = settingsLoad();
  settings.startHidden = hidden;

  if (!noAutostart) {
    // Sincroniza o registro com a configuracao; sobrescrever corrige instalacao movida.
    if (settings.autostart) {
      const std::string cmd = appAutostartCommand();
      if (!cmd.empty()) autostartEnable("TamaPoke", cmd.c_str());
    } else {
      autostartDisable("TamaPoke");
    }
  }

  IGameHost* host = createGameHost(dataDir.c_str(), sdRoot.c_str());
  host->boot();

  AppShell shell;
  shell.setAutostartEnabled(!noAutostart);
  if (!shell.init(host, settings, &single)) {
    fatal("Nao foi possivel criar a janela.");
    shell.shutdown();
    destroyGameHost(shell.host());
    SDL_Quit();
    return 1;
  }
  if (openConsole) consoleToggle();
  shell.run();
  shell.host()->save();  // o host pode ter sido recriado por um reinicio
  shell.shutdown();
  destroyGameHost(shell.host());
  SDL_Quit();
  return 0;
}
