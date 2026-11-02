#pragma once
#include <cstdint>

// Interface estreita entre o shell (SDL/Win32) e o jogo (upstream + HAL).
// Nada aqui menciona SDL, HWND ou bandeja.
struct IGameHost {
  virtual ~IGameHost() = default;
  virtual void boot() = 0;
  virtual void tick() = 0;
  virtual bool frameDirty() const = 0;
  virtual const uint16_t* frame() const = 0;
  virtual uint8_t brightness() const = 0;
  virtual void touchDown(int x, int y) = 0;
  virtual void touchMove(int x, int y) = 0;
  virtual void touchUp() = 0;
  virtual void powerShortPress() = 0;
  virtual void injectConsole(const char* line) = 0;
  // Apelido ou nome da especie no idioma ativo (UTF-8); "" e nivel 0 se ainda e ovo.
  virtual const char* petName() const = 0;
  virtual int petLevel() const = 0;
  virtual void save() = 0;
  virtual void setSoundEnabled(bool on) = 0;
  virtual bool wantsRestart() const = 0;
  virtual void clearFrameDirty() = 0;
};

// Implementacao sobre o upstream. dataDir e sdRoot sao absolutos.
IGameHost* createGameHost(const char* dataDir, const char* sdRoot);
void destroyGameHost(IGameHost* host);
