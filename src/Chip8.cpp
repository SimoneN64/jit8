#include <Chip8.hpp>
#include <fstream>
#include <vector>
#include <cassert>
#include <ctime>

CoreState::CoreState() {
  srand(time(nullptr));
  std::copy(std::begin(font), std::end(font), std::begin(ram)+0x50);
}

static inline std::vector<u8> ReadFileBinary(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator{file}, {}};
}

bool CoreState::LoadProgram(const fs::path &path) {
  auto binary = ReadFileBinary(path.string());
  if(binary.size() > (0x1000 - 0x200)) return false;
  std::copy(binary.begin(), binary.end(), std::begin(ram)+0x200);
  return true;
}