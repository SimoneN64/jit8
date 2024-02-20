#include <Chip8.hpp>
#include <fstream>
#include <vector>
#include <cassert>
#include <ctime>

#define vx v[x]
#define vy v[y]
#define vf v[0xf]
#define unimplemented(fmt, ...) do { printf("Unimplemented opcode for group " fmt "\n", __VA_ARGS__); exit(1); } while(0)

CoreState::CoreState() {
  srand(time(nullptr));
  std::copy(std::begin(font), std::end(font), std::begin(ram)+0x50);

  gen = new Xbyak::CodeGenerator(4096, cache);
  gen->setProtectMode(Xbyak::CodeGenerator::PROTECT_RWE);
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

void CoreState::dxyn(u8 x, u8 y, u8 n) {
  vf = 0;
  for(int yy = 0; yy < n; yy++) {
    auto pixel = ram[ip + yy];
    for(int xx = 0; xx < 8; xx++) {
      if((pixel & (0x80 >> xx)) != 0) {
        if (display[(xx + x) + ((yy + y) * 64)]) {
          vf = 1;
        }
        display[(xx + x) + ((yy + y) * 64)] ^= 1;
      }
    }
  }

  draw = true;
}

void CoreState::RunInterpreter() {
  u16 op = bswap_16(*reinterpret_cast<u16*>(&ram[PC]));
  u16 addr = op & 0xfff;
  u8 kk = addr & 0xff;
  u8 n = kk & 0xf;
  u8 x = (op >> 8) & 0xf;
  u8 y = (op >> 4) & 0xf;

  switch(op & 0xf000) {
    case 0x0000: {
      switch(addr) {
        case 0x0E0: memset(display, 0, 64*32); draw = true; PC += 2; break;
        case 0x0EE: PC = stack[--sp]; PC += 2; break;
        default: unimplemented("0x0000: %04X", addr);
      }
    } break;
    case 0x1000: PC = addr; break;
    case 0x2000: stack[sp++] = PC; PC = addr; break;
    case 0x3000: PC += 2 * !!(vx == kk); PC += 2; break;
    case 0x4000: PC += 2 * !!(vx != kk); PC += 2; break;
    case 0x5000: PC += 2 * !!(vx == vy); PC += 2; break;
    case 0x6000: vx  = kk; PC += 2; break;
    case 0x7000: vx += kk; PC += 2; break;
    case 0x8000:
      switch(n) {
        case 0x0: vx  = vy; PC += 2; break;
        case 0x1: vx |= vy; PC += 2; break;
        case 0x2: vx &= vy; PC += 2; break;
        case 0x3: vx ^= vy; PC += 2; break;
        case 0x4: 
          vf = !!(u16(vx) + u16(vy) > 255);
          vx += vy;
          PC += 2;
          break;
        case 0x5:
          vf = !!(vx > vy);
          vx -= vy;
          PC += 2;
          break;
        case 0x6:
          vf = vx & 1;
          vx >>= 1;
          PC += 2;
          break;
        case 0x7:
          vf = !!(vx < vy);
          vx = vy - vx;
          PC += 2;
          break;
        case 0xE:
          vf = vx & 0x80;
          vx <<= 1;
          PC += 2;
          break;
      }
      break;
    case 0x9000: PC += 2 * !!(vx != vy); PC += 2; break;
    case 0xA000: ip = addr; PC += 2; break;
    case 0xB000: PC = v[0] + addr; break;
    case 0xC000: vx = rand() & kk; PC += 2; break;
    case 0xD000: dxyn(vx, vy, n); PC += 2; break;
    case 0xE000: unimplemented("0xE000: %02X", kk); break;
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
      PC += 2;
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

static inline bool modifiesPC(u16 op) {
  switch (op & 0xf000) {
    case 0x0000:
      switch (op & 0x0fff) {
        case 0x0EE: return true;
        default: return false;
      }
      break;
    case 0x1000: case 0x2000:
    case 0x3000: case 0x4000:
    case 0x9000: case 0xB000:
    case 0x5000: return true;
    default: return false;
  }
}

#define regsOffset(i) (((uintptr_t)&v[i]) - ((uintptr_t)this))
#undef vx
#undef vy
#undef vf
#define vx (((uintptr_t)this) + regsOffset(x))
#define vy (((uintptr_t)this) + regsOffset(y))
#define vf (((uintptr_t)this) + regsOffset(0xf))

void CoreState::EmitInstruction(u16 op) {
  u16 addr = op & 0xfff;
  u8 kk = addr & 0xff;
  u8 n = kk & 0xf;
  u8 x = (op >> 8) & 0xf;
  u8 y = (op >> 4) & 0xf;

  switch (op & 0xf000) {
  case 0x0000: {
    switch (addr) {
    case 0x0E0: memset(display, 0, 64 * 32); draw = true; PC += 2; break;
    case 0x0EE: PC = stack[--sp]; PC += 2; break;
    default: unimplemented("0x0000: %04X", addr);
    }
  } break;
  case 0x1000: PC = addr; break;
  case 0x2000: stack[sp++] = PC; PC = addr; break;
  case 0x3000: PC += 2 * !!(vx == kk); PC += 2; break;
  case 0x4000: PC += 2 * !!(vx != kk); PC += 2; break;
  case 0x5000: PC += 2 * !!(vx == vy); PC += 2; break;
  case 0x6000: vx = kk; PC += 2; break;
  case 0x7000: vx += kk; PC += 2; break;
  case 0x8000:
    switch (n) {
    case 0x0: vx = vy; PC += 2; break;
    case 0x1: vx |= vy; PC += 2; break;
    case 0x2: vx &= vy; PC += 2; break;
    case 0x3: vx ^= vy; PC += 2; break;
    case 0x4:
      vf = !!(u16(vx) + u16(vy) > 255);
      vx += vy;
      PC += 2;
      break;
    case 0x5:
      vf = !!(vx > vy);
      vx -= vy;
      PC += 2;
      break;
    case 0x6:
      vf = vx & 1;
      vx >>= 1;
      PC += 2;
      break;
    case 0x7:
      vf = !!(vx < vy);
      vx = vy - vx;
      PC += 2;
      break;
    case 0xE:
      vf = vx & 0x80;
      vx <<= 1;
      PC += 2;
      break;
    }
    break;
  case 0x9000: PC += 2 * !!(vx != vy); PC += 2; break;
  case 0xA000: ip = addr; PC += 2; break;
  case 0xB000: PC = v[0] + addr; break;
  case 0xC000: vx = rand() & kk; PC += 2; break;
  case 0xD000: dxyn(vx, vy, n); PC += 2; break;
  case 0xE000: unimplemented("0xE000: %02X", kk); break;
  case 0xF000:
    switch (kk) {
    case 0x07: vx = delay; break;
    case 0x15: delay = vx; break;
    case 0x18: sound = vx; break;
    case 0x1E: ip += vx; break;
    case 0x29: break;
    case 0x33:
      ram[ip] = vx / 100;
      ram[ip + 1] = (vx / 10) % 10;
      ram[ip + 2] = (vx % 100) % 10;
      cache[ip] = nullptr;
      break;
    case 0x55:
      memcpy(&ram[ip], v, x + 1);
      cache[ip] = nullptr;
      break;
    case 0x65: memcpy(v, &ram[ip], x + 1); break;
    default: unimplemented("0xF000: %02X", kk);
    }
    PC += 2;
    break;
  default: unimplemented("%04X", op & 0xf000);
  }

  cycles++;
  if (cycles >= kTimersRate) {
    cycles = 0;
    delay--;
    sound--;
  }

  if (delay <= 0) delay = 60;
  if (sound <= 0) sound = 60;
}

void CoreState::RunJit() {
  u16 pc = PC;
  u16 op = bswap_16(*reinterpret_cast<u16*>(&ram[pc]));
  
  if (cache[pc & 0xfff]) {
    cache[pc & 0xfff]();
    return;
  } else {
    while (!modifiesPC(op)) {
      EmitInstruction(op);

      pc += 2;
      op = bswap_16(*reinterpret_cast<u16*>(&ram[pc]));
    }

    gen->ready();
    cache[pc & 0xfff] = gen->getCode<void(*)()>();
    cache[pc & 0xfff]();
  }
}