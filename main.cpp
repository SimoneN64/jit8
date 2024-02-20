#include <SDL_events.h>
#include <SDL_render.h>
#include <SDL_video.h>
#include <cstdio>
#include <Chip8.hpp>
#include <SDL2/SDL.h>

int main(int argc, char** argv) {
  if(argc < 2) {
    printf("Usage: jit8 <chip-8 executable>\n");
    return -1;
  }

  fs::path romPath(argv[1]);
  if(!fs::exists(romPath)) {
    printf("This file doesn't exist!\n");
    return -1;
  }

  CoreState core;
  if(!core.LoadProgram(romPath)) {
    printf("Failed to read Chip8 program (maybe too big?)\n");
    return -1;
  }

  SDL_Window* window = SDL_CreateWindow(
    "Jit8",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    1024, 512,
    SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  bool running = true;

  while(running) {
    core.RunInterpreter();
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
      if(e.type == SDL_QUIT) running = false;
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}