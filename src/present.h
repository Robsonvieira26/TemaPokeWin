#pragma once
#include <cstdint>
#include <vector>
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

// Funcoes puras, testaveis sem janela.
// Dentro do circulo: dx*dx+dy*dy <= r*r, r=233, centro (233,233) (borda inclusiva).
bool presentInsideCircle(int x, int y);
int  presentToLogical(int windowPixel, float scale);
// RGB565 -> ARGB8888 opaco (replicacao de bits: 0xFFFF -> 0xFFFFFFFF).
uint32_t presentRgb565ToArgb(uint16_t p);
// Converte um quadro 466x466; com circular, pixels fora do circulo viram 0x00000000.
void presentConvertFrame(const uint16_t* in, uint32_t* out, bool circular);

// Parte SDL: nao testavel sem janela (verificada no checklist manual).
class Presenter {
 public:
  bool init(SDL_Window* window);
  void shutdown();
  // Desenha o framebuffer da HAL. brightness 0..255; circular aplica alfa 0 fora.
  void draw(const uint16_t* frame, uint8_t brightness, bool circular);
  void setScale(float scale);
  float scale() const { return scale_; }

 private:
  SDL_Renderer* ren_ = nullptr;
  SDL_Texture* tex_ = nullptr;
  float scale_ = 1.0f;
  std::vector<uint32_t> buf_;  // reutilizado a cada quadro
};
