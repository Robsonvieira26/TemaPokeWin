#include "game_host.h"
#include "hal_control.h"
#include "pet.h"     // do upstream: classe Pet
#include "audio.h"   // do upstream: audioSetEnabled
#include <string>
#include <utility>

// Pontos de entrada e global do .ino (TamaPoke.gen.cpp).
void setup();
void loop();
extern Pet pet;

namespace {
class GameHost : public IGameHost {
 public:
  GameHost(std::string dataDir, std::string sdRoot)
      : dataDir_(std::move(dataDir)), sdRoot_(std::move(sdRoot)) {}

  void boot() override {
    halSetDataDir(dataDir_.c_str());
    halSetSdRoot(sdRoot_.c_str());
    setup();
    booted_ = true;
  }
  void tick() override { if (booted_) loop(); }

  bool frameDirty() const override { return halFrameDirty(); }
  void clearFrameDirty() override { halClearFrameDirty(); }
  const uint16_t* frame() const override { return halFrame(); }
  uint8_t brightness() const override { return halBrightness(); }

  void touchDown(int x, int y) override { halTouchDown(x, y); }
  void touchMove(int x, int y) override { halTouchMove(x, y); }
  void touchUp() override { halTouchUp(); }
  void powerShortPress() override { halPwrShortPress(); }

  void injectConsole(const char* line) override {
    if (!line) return;
    std::string s(line);
    if (s.empty() || s.back() != '\n') s.push_back('\n');
    halSerialInject(s.c_str());
  }

  // O upstream grava na hora nos eventos importantes (p.ex. chooseStarter chama
  // save()); o que sobra e o guardado periodico pendente, que o loop so volca
  // num momento sem animacao. flushSave() volca esse pendente (no-op sem pendencia).
  void save() override { if (booted_) pet.flushSave(); }
  void setSoundEnabled(bool on) override { if (booted_) audioSetEnabled(on); }
  bool wantsRestart() const override { return halRestartRequested(); }

 private:
  std::string dataDir_, sdRoot_;
  bool booted_ = false;
};
}  // namespace

IGameHost* createGameHost(const char* dataDir, const char* sdRoot) {
  return new GameHost(dataDir ? dataDir : ".", sdRoot ? sdRoot : "./sdcard");
}
void destroyGameHost(IGameHost* h) { delete h; }
