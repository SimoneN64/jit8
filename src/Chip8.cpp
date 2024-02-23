#include <Chip8.hpp>
#include <fstream>
#include <vector>
#include <ctime>

#define vx v[x]
#define vy v[y]
#define vf v[0xf]
#define unimplemented(fmt, ...) do { printf("Unimplemented opcode for group " fmt "\n", __VA_ARGS__); exit(1); } while(0)

CoreState::CoreState() {
  srand(time(nullptr));
  std::copy(std::begin(font), std::end(font), std::begin(ram)+0x50);
  memset(cache, 0, sizeof(*cache) * BLOCKS_SIZE);

  gen = new Xbyak::CodeGenerator;
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
        if (display[yy + y] & (1ull << (xx + x))) {
          vf = 1;
          display[yy + y] &= ~(1ull << (xx + x));
        } else {
          vf = 0;
          display[yy + y] |= (1ull << (xx + x));
        }
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
        case 0x0E0: memset(display, 0, 32*sizeof(u64)); draw = true; PC += 2; break;
        case 0x0EE: PC = stack[--sp]; PC += 2; break;
        default: unimplemented("0x0000: %04X", addr);
      }
    } break;
    case 0x1000: PC = addr; break;
    case 0x2000: stack[sp++] = PC; PC = addr; break;
    case 0x3000: PC += 2 * (vx == kk); PC += 2; break;
    case 0x4000: PC += 2 * (vx != kk); PC += 2; break;
    case 0x5000: PC += 2 * (vx == vy); PC += 2; break;
    case 0x6000: vx  = kk; PC += 2; break;
    case 0x7000: vx += kk; PC += 2; break;
    case 0x8000:
      switch(n) {
        case 0x0: vx  = vy; PC += 2; break;
        case 0x1: vx |= vy; PC += 2; break;
        case 0x2: vx &= vy; PC += 2; break;
        case 0x3: vx ^= vy; PC += 2; break;
        case 0x4: 
          vf = (u16(vx) + u16(vy) > 255);
          vx += vy;
          PC += 2;
          break;
        case 0x5:
          vf = (vx > vy);
          vx -= vy;
          PC += 2;
          break;
        case 0x6:
          vf = vx & 1;
          vx >>= 1;
          PC += 2;
          break;
        case 0x7:
          vf = (vx < vy);
          vx = vy - vx;
          PC += 2;
          break;
        case 0xE:
          vf = (vx & 0x80) != 0;
          vx <<= 1;
          PC += 2;
          break;
        default: unimplemented("0x8000: %02X\n", n);
      }
      break;
    case 0x9000: PC += 2 * (vx != vy); PC += 2; break;
    case 0xA000: ip = addr; PC += 2; break;
    case 0xB000: PC = v[0] + addr; break;
    case 0xC000: vx = rand() & kk; PC += 2; break;
    case 0xD000: dxyn(vx, vy, n); PC += 2; break;
    case 0xE000: unimplemented("0xE000: %02X", kk);
    case 0xF000:
      switch(kk) {
        case 0x07: vx = delay; break;
        case 0x15: delay = vx; break;
        case 0x18: sound = vx; break;
        case 0x1E: ip += vx; break;
        case 0x29: ip = 0x50 + vx * 5; break;
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
    case 0x1000: case 0x2000:
    case 0x3000: case 0x4000:
    case 0x9000: case 0xB000:
    case 0x5000: return true;
    default: return false;
  }
}

#define thisOffset(x) (((uintptr_t)&x) - ((uintptr_t)this))
#undef vx
#undef vy
#undef vf
#define vx (contextPtr) + thisOffset(v[x])
#define vy (contextPtr) + thisOffset(v[y])
#define vf (contextPtr) + thisOffset(v[0xf])
#define IncPC do { \
  gen->mov(gen->r8w, gen->word[contextPtr + thisOffset(PC)]); \
  gen->add(gen->r8w, 2); \
  gen->mov(gen->word[contextPtr + thisOffset(PC)], gen->r8w); \
} while(0)

void CoreState::EmitInstruction(u16 op) {
  u16 addr = op & 0xfff;
  u8 kk = addr & 0xff;
  u8 n = kk & 0xf;
  u8 x = (op >> 8) & 0xf;
  u8 y = (op >> 4) & 0xf;

  switch (op & 0xf000) {
  case 0x0000: {
    switch (addr) {
    case 0x0E0:
      gen->mov(arg1, thisOffset(display));
      gen->add(arg1, contextPtr);
      gen->mov(arg2.cvt32(), 0);
      gen->mov(arg3, 32*sizeof(u64));
      gen->mov(contextPtr, (uintptr_t)memset);
      gen->call(contextPtr);
      gen->mov(contextPtr, (uintptr_t)this);
      gen->mov(gen->r8b, 1);
      gen->mov(gen->byte[contextPtr + thisOffset(draw)], gen->r8b);
      IncPC;
      break;
    case 0x0EE:
      gen->mov(gen->r8b, gen->byte[contextPtr + thisOffset(sp)]);
      gen->sub(gen->r8b, 1);
      gen->mov(gen->byte[contextPtr + thisOffset(sp)], gen->r8b);
      gen->mov(gen->r9w, gen->word[contextPtr + thisOffset(stack) + thisOffset(sp)]);
      gen->mov(gen->word[contextPtr + thisOffset(PC)], gen->r9w);
      IncPC;
      break;
    default: unimplemented("0x0000: %04X", addr);
    }
  } break;
  case 0x1000:
    gen->mov(gen->word[contextPtr + thisOffset(PC)], addr);
    break;
  case 0x2000:
    gen->mov(gen->r9w, gen->word[contextPtr + thisOffset(PC)]);
    gen->mov(gen->word[contextPtr + thisOffset(stack) + thisOffset(sp)], gen->r9w);
    gen->mov(gen->r8b, gen->byte[contextPtr + thisOffset(sp)]);
    gen->add(gen->r8b, 1);
    gen->mov(gen->byte[contextPtr + thisOffset(sp)], gen->r8b);
    gen->mov(gen->word[contextPtr + thisOffset(PC)], addr);
    break;
  case 0x3000:
    gen->xor_(gen->r8, gen->r8);
    gen->mov(gen->r10w, 2);
    gen->mov(gen->r9b, gen->byte[vx]);
    gen->cmp(gen->r9b, kk);
    gen->cmove(gen->r8w, gen->r10w);
    gen->add(gen->word[contextPtr + thisOffset(PC)], gen->r8w);
    IncPC;
    break;
  case 0x4000:
    gen->xor_(gen->r8, gen->r8);
    gen->mov(gen->r10w, 2);
    gen->mov(gen->r9b, gen->byte[vx]);
    gen->cmp(gen->r9b, kk);
    gen->cmovne(gen->r8w, gen->r10w);
    gen->add(gen->word[contextPtr + thisOffset(PC)], gen->r8w);
    IncPC;
    break;
  case 0x5000:
    gen->xor_(gen->r8, gen->r8);
    gen->mov(gen->r10w, 2);
    gen->mov(gen->r9b, gen->byte[vx]);
    gen->cmp(gen->r9b, gen->byte[vy]);
    gen->cmove(gen->r8w, gen->r10w);
    gen->add(gen->word[contextPtr + thisOffset(PC)], gen->r8w);
    IncPC;
    break;
  case 0x6000:
    gen->mov(gen->byte[vx], kk);
    IncPC;
    break;
  case 0x7000:
    gen->mov(gen->r8b, gen->byte[vx]);
    gen->add(gen->r8b, kk);
    gen->mov(gen->byte[vx], gen->r8b);
    IncPC;
    break;
  case 0x8000:
    switch (n) {
    case 0x0:
      gen->mov(gen->r8b, gen->byte[vy]);
      gen->mov(gen->byte[vx], gen->r8b);
      IncPC;
      break;
    case 0x1:
      gen->mov(gen->r8b, gen->byte[vy]);
      gen->or_(gen->byte[vx], gen->r8b);
      IncPC;
      break;
    case 0x2:
      gen->mov(gen->r8b, gen->byte[vy]);
      gen->and_(gen->byte[vx], gen->r8b);
      IncPC;
      break;
    case 0x3:
      gen->mov(gen->r8b, gen->byte[vy]);
      gen->xor_(gen->byte[vx], gen->r8b);
      IncPC;
      break;
    case 0x4:
      gen->mov(gen->r8b, gen->byte[vx]);
      gen->mov(gen->r9b, gen->byte[vy]);
      gen->add(gen->r8w, gen->r9w);
      gen->setc(gen->r9b);
      gen->mov(gen->byte[vf], gen->r9b);
      gen->mov(gen->byte[vx], gen->r8b);
      IncPC;
      break;
    case 0x5:
      gen->mov(gen->r8b, gen->byte[vx]);
      gen->mov(gen->r9b, gen->byte[vy]);
      gen->cmp(gen->r8b, gen->r9b);
      gen->setg(gen->r9b);
      gen->mov(gen->byte[vf], gen->r9b);
      gen->sub(gen->r8b, gen->r9b);
      gen->mov(gen->byte[vx], gen->r8b);
      IncPC;
      break;
    case 0x6:
      gen->mov(gen->r8b, gen->byte[vx]);
      gen->mov(gen->r9b, gen->r8b);
      gen->and_(gen->r9b, 1);
      gen->setnz(gen->r9b);
      gen->mov(gen->byte[vf], gen->r9b);
      gen->sar(gen->r8b, 1);
      gen->mov(gen->byte[vx], gen->r8b);
      IncPC;
      break;
    case 0x7:
      gen->mov(gen->r8b, gen->byte[vx]);
      gen->mov(gen->r9b, gen->byte[vy]);
      gen->cmp(gen->r8b, gen->r9b);
      gen->setg(gen->r10b);
      gen->mov(gen->byte[vf], gen->r10b);
      gen->sub(gen->r9b, gen->r8b);
      gen->mov(gen->byte[vx], gen->r9b);
      IncPC;
      break;
    case 0xE:
      gen->mov(gen->r8b, gen->byte[vx]);
      gen->mov(gen->r9b, gen->r8b);
      gen->and_(gen->r9b, 0x80);
      gen->setnz(gen->r9b);
      gen->mov(gen->byte[vf], gen->r9b);
      gen->sal(gen->r8b, 1);
      gen->mov(gen->byte[vx], gen->r8b);
      IncPC;
      break;
    default: unimplemented("0x8000: %02X\n", n);
    }
    break;
  case 0x9000:
    gen->xor_(gen->r8, gen->r8);
    gen->mov(gen->r10w, 2);
    gen->mov(gen->r9b, gen->byte[vx]);
    gen->cmp(gen->r9b, gen->byte[vy]);
    gen->cmovne(gen->r8w, gen->r10w);
    gen->add(gen->word[contextPtr + thisOffset(PC)], gen->r8w);
    IncPC;
    break;
  case 0xA000:
    gen->mov(gen->r8w, addr);
    gen->mov(gen->word[contextPtr + thisOffset(ip)], gen->r8w);
    IncPC;
    break;
  case 0xB000:
    gen->mov(gen->r8b, gen->byte[contextPtr + thisOffset(v[0])]);
    gen->add(gen->r8w, addr);
    gen->mov(gen->word[contextPtr + thisOffset(PC)], gen->r8w);
    break;
  case 0xC000:
    gen->mov(gen->byte[vx], rand() & kk);
    IncPC;
    break;
  case 0xD000:
    gen->mov(gen->dl, gen->byte[vx]);
    gen->mov(gen->r8b, gen->byte[vy]);
    gen->mov(gen->r9b, n);
    gen->mov(arg2.cvt8(), gen->byte[vx]);
    gen->mov(arg3.cvt8(), gen->byte[vy]);
    gen->mov(arg4.cvt8(), n);
    emitMemberCall(&CoreState::dxyn, this);
    IncPC;
    break;
  case 0xE000: unimplemented("0xE000: %02X", kk);
  case 0xF000:
    switch (kk) {
    case 0x07:
      gen->mov(gen->byte[vx], delay);
      break;
    case 0x15:
      gen->mov(gen->r8b, gen->byte[vx]);
      gen->mov(gen->byte[contextPtr + thisOffset(delay)], gen->r8b);
      break;
    case 0x18:
      gen->mov(gen->r8b, gen->byte[vx]);
      gen->mov(gen->byte[contextPtr + thisOffset(sound)], gen->r8b);
      break;
    case 0x1E:
      gen->mov(gen->r8w, gen->word[contextPtr + thisOffset(ip)]);
      gen->add(gen->r8w, gen->byte[vx]);
      gen->mov(gen->word[contextPtr + thisOffset(ip)], gen->r8w);
      break;
    case 0x29:
      gen->mov(gen->r8b, gen->byte[vx]);
      gen->mov(contextPtr, 5);
      gen->mul(gen->r8b);
      gen->mov(gen->r8b, contextPtr.cvt8());
      gen->mov(contextPtr, (uintptr_t)this);
      gen->add(gen->r8b, 0x50);
      gen->mov(gen->word[contextPtr + thisOffset(ip)], gen->r8w);
      break;
    case 0x33:
      gen->mov(gen->r9, contextPtr);
      gen->xor_(gen->rdx, gen->rdx);
      gen->mov(gen->al, gen->byte[vx]);
      gen->mov(gen->r8b, 100);
      gen->div(gen->r8b);
      gen->mov(gen->byte[gen->r9 + thisOffset(ram[ip])], gen->al);

      gen->xor_(gen->rdx, gen->rdx);
      gen->mov(gen->al, gen->byte[vx]);
      gen->mov(gen->r8b, 10);
      gen->div(gen->r8b);
      gen->div(gen->r8b);
      gen->mov(gen->byte[gen->r9 + thisOffset(ram[ip + 1])], gen->al);

      gen->xor_(gen->rdx, gen->rdx);
      gen->mov(gen->al, gen->byte[vx]);
      gen->mov(gen->r8b, 100);
      gen->div(gen->r8b);
      gen->mov(gen->r8b, 10);
      gen->sar(gen->ax, 8);
      gen->div(gen->r8b);
      gen->mov(gen->byte[gen->r9 + thisOffset(ram[ip + 2])], gen->al);
      gen->mov(contextPtr, gen->r9);

      invalidate(ip);
      invalidate(ip + 1);
      invalidate(ip + 2);
      break;
    case 0x55:
      gen->mov(arg1, thisOffset(ram[ip]));
      gen->add(arg1, contextPtr);
      gen->mov(arg2, thisOffset(v[0]));
      gen->mov(arg2, contextPtr);
      gen->mov(arg3.cvt8(), x + 1);
      gen->mov(contextPtr, (uintptr_t)memcpy);
      gen->call(contextPtr);
      gen->mov(contextPtr, (uintptr_t)this);

      invalidate(ip);
      break;
    case 0x65:
      gen->mov(arg1, thisOffset(v[0]));
      gen->add(arg1, contextPtr);
      gen->mov(arg2, thisOffset(ram[ip]));
      gen->add(arg2, contextPtr);
      gen->mov(arg3.cvt8(), x + 1);
      gen->mov(contextPtr, (uintptr_t)memcpy);
      gen->call(contextPtr);
      gen->mov(contextPtr, (uintptr_t)this);
      break;
    default: unimplemented("0xF000: %02X", kk);
    }
    IncPC;
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

static inline void Push(Xbyak::CodeGenerator& code, const std::initializer_list<Xbyak::Reg64>& regs) {
  for (auto reg: regs) {
    code.push(reg);
  }
}

static inline void Pop(Xbyak::CodeGenerator& code, const std::initializer_list<Xbyak::Reg64>& regs) {
  auto end = std::rend(regs);
  for(auto it = std::rbegin(regs); it != end; ++it) {
    code.pop(*it);
  }
}

void CoreState::invalidate(u16 addr) {
  auto [cks, start_addr, end_addr, func] = cache[addr & BLOCKS_DSIZE];
  // we do not care about non-program stuff
  if(addr < 0x200) return;

  // early exit if it's not compiled in the first place
  if(!func) return;

  printf("Invalidating block @ %04X\n", addr);
  cache[addr & BLOCKS_DSIZE].func = nullptr;
  cache[addr & BLOCKS_DSIZE].start_addr = -1;
  cache[addr & BLOCKS_DSIZE].end_addr = -1;
}

void CoreState::RunJit() {
  u16 pc = PC;
  
  if (cache[(PC - 0x200) & BLOCKS_DSIZE].func) {
    cache[(PC - 0x200) & BLOCKS_DSIZE].func();
  } else {
    cache[(PC - 0x200) & BLOCKS_DSIZE].start_addr = pc;
    cache[(PC - 0x200) & BLOCKS_DSIZE].func = gen->getCurr<void(*)()>();

    Push(*gen, {gen->rbx, gen->rbp, gen->r12, gen->r13, gen->r14, gen->r15});
#ifdef _WIN32
    Push(*gen, {gen->rsi, gen->rdi});
#endif
    gen->mov(gen->rbp, gen->rsp);
    gen->mov(contextPtr, (uintptr_t)this);

    u16 op = bswap_16(*reinterpret_cast<u16*>(&ram[pc]));
    EmitInstruction(op);
    while (!modifiesPC(op)) {
      pc += 2;
      op = bswap_16(*reinterpret_cast<u16*>(&ram[pc]));
      EmitInstruction(op);
    }

#ifdef _WIN32
    Pop(*gen, {gen->rsi, gen->rdi});
#endif
    Pop(*gen, {gen->rbx, gen->rbp, gen->r12, gen->r13, gen->r14, gen->r15});
    gen->ret();
    cache[(PC - 0x200) & BLOCKS_DSIZE].end_addr = pc;
    cache[(PC - 0x200) & BLOCKS_DSIZE].func();
  }
}