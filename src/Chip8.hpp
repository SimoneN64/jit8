#pragma once
#include <cstdint>
#include <filesystem>
#include <xbyak.h>

#define bswap_16(x) (((x) << 8) | ((x) >> 8))

namespace fs = std::filesystem;

constexpr auto kCpuFreq = 3355443;
constexpr auto kTimersRate = float(kCpuFreq)/60;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;

struct CoreState {
  u16 PC = 0x200, ip = 0, stack[16]{};
  u8 ram[0x1000]{}, v[16]{}, sp = 0, delay = 0, sound = 0;
  u32 cycles = 0;
  bool display[64 * 32]{};
  bool draw = false;
  static constexpr u8 font[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
  };

  CoreState();

  bool LoadProgram(const fs::path&);
  void RunInterpreter();
  void RunJit();
  void dxyn(u8, u8, u8);
private:
  template <typename T>
  void emitMemberFunctionCall(T func) {
    void* functionPtr;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    static_assert(sizeof(T) == 8, "[x64 JIT] Invalid size for member function pointer");
    std::memcpy(&functionPtr, &func, sizeof(T));
#else
    static_assert(sizeof(T) == 16, "[x64 JIT] Invalid size for member function pointer");
    uintptr_t arr[2];
    std::memcpy(arr, &func, sizeof(T));
    // First 8 bytes correspond to the actual pointer to the function
    functionPtr = reinterpret_cast<void*>(arr[0]);
    // Next 8 bytes correspond to the "this" pointer adjustment
    thisPtr += arr[1];
#endif

    gen->mov(gen->rcx, gen->rdi);
    gen->call(functionPtr);
  }

  void (*cache[0xE00])();
  u8* code{};
  Xbyak::CodeGenerator* gen;
  void EmitInstruction(u16);
};