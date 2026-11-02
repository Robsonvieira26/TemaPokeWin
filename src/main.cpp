#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int, char**) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TamaPoke", SDL_GetError(), nullptr);
    return 1;
  }
  SDL_Window* win = SDL_CreateWindow("TamaPoke", 466, 466, 0);
  if (!win) { SDL_Quit(); return 1; }

  bool running = true;
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_EVENT_QUIT) running = false;
    }
    SDL_Delay(16);
  }
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
