#include <Chip8.hpp>
#include <fstream>
#include <vector>
#include <cassert>
#include <ctime>

#define vx v[x]
#define vy v[y]
#define vf v[0xf]
#define unimplemented(fmt, ...) do { printf("Unimplemented opcode for group " fmt, __VA_ARGS__); exit(1); } while(0)

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

void CoreState::RunInterpreter() {
  u16 op = bswap_16(*reinterpret_cast<u16*>(&ram[PC]));
  u16 pc = PC;
  pc+=2;
  u16 addr = op & 0xfff;
  u8 kk = addr & 0xff;
  u8 n = kk & 0xf;
  u8 x = (op >> 8) & 0xf;
  u8 y = (op >> 4) & 0xf;

  switch(op & 0xf000) {
    case 0x0000: {
      switch(addr) {
        case 0x0E0: break;
        case 0x0EE: pc = sp--; break;
      }
    } break;
    case 0x1000: pc = addr; break;
    case 0x2000: stack[++sp] = pc; pc = addr; break;
    case 0x3000: pc += 2 * !!(vx == kk); break;
    case 0x4000: pc += 2 * !!(vx != kk); break;
    case 0x5000: pc += 2 * !!(vx == vy); break;
    case 0x6000: vx  = kk; break;
    case 0x7000: vx += kk; break;
    case 0x8000:
      switch(n) {
        case 0x0: vx  = vy; break;
        case 0x1: vx |= vy; break;
        case 0x2: vx &= vy; break;
        case 0x3: vx ^= vy; break;
        case 0x4: 
          vf = !!(u16(vx) + u16(vy) > 255);
          vx += vy;
          break;
        case 0x5: 
          vf = !!(vx > vy);
          vx -= vy;
          break;
        case 0x6: 
          vf = vx & 1;
          vx >>= 1;
          break;
        case 0x7: 
          vf = !!(vx < vy);
          vx = vy - vx;
          break;
        case 0xE:
          vf = vx & 0x80;
          vx <<= 1;
          break;
      }
      break;
    case 0x9000: pc += 2 * !!(vx != vy); break;
    case 0xA000: ip = addr; break;
    case 0xB000: pc = v[0] + addr; break;
    case 0xC000: vx = rand() & kk; break;
    case 0xD000: break;
    case 0xE000: break;
    case 0xF000:
      switch(kk) {
        case 0x07: vx = delay; break;
        case 0x15: delay = vx; break;
        case 0x18: sound = vx; break;
        case 0x1E: ip += vx; break;
        case 0x29: break;
        case 0x33:
          ram[ip] = vx / 100;
          ram[ip+1] = (vx / 10) % 10;
          ram[ip+2] = (vx % 100) % 10;
          break;
        case 0x55: memcpy(&ram[ip], v, x+1); break;
        case 0x65: memcpy(v, &ram[ip], x+1); break;
        default: unimplemented("0xF000: %02X", kk);
      }
      break;  
    default: unimplemented("%04X", op & 0xf000);
  }

  cycles++;
  if(cycles >= kTimersRate) {
    cycles = 0;
    delay--;
    sound--;
  }

  if(delay <= 0) delay = 60;
  if(sound <= 0) sound = 60;
}