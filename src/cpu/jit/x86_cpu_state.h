#pragma once
#include <cstdint>

namespace pas::cpu::jit {

struct X86CpuState {
    uint32_t regs[8];
    uint32_t eflags;
    uint32_t pc;
    uint8_t* mem_base;
    uint32_t mem_base_guest;
    uint32_t mem_size;
};

} // namespace pas::cpu::jit
