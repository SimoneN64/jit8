#include <cstdio>
#include <Chip8.hpp>

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

  return 0;
}