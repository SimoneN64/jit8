#include <SDL_events.h>
#include <SDL_pixels.h>
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
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 64, 32);
  SDL_RenderSetLogicalSize(renderer, 64, 32);
  u32 texBuf[64*32]{};

  bool running = true;

  while(running) {
    core.RunJit();

    if(core.draw) {
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);
      for(int y = 0; y < 32; y++) {
        for(int x = 0; x < 64; x++) {
          if(core.display[y] & (1ull << x)) {
            texBuf[(y * 64 + x)] = 0xffffffff;
          } else {
            texBuf[(y * 64 + x)] = 0xff000000;
          }
        }
      }
      SDL_UpdateTexture(texture, nullptr, texBuf, 64*4);
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderPresent(renderer);
      core.draw = false;
    }

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