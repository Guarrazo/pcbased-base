#pragma once
#include <cstdint>
#include <vector>

namespace pas::cpu::ir {

enum class OpCode {
    Nop,
    LoadImm, LoadReg, StoreReg,
    LoadMem, StoreMem,
    Lea,
    Add, Sub, Mul,
    And, Or, Xor, Not,
    ShiftLeft, ShiftRight,
    AddImm,
    CompareAndSetFlags,
    Branch, BranchConditional,
    Call, Return,
    Push, Pop,
    CallNative,
};

struct Value {
    uint32_t id = 0;
};

struct Instruction {
    OpCode op = OpCode::Nop;
    Value dst;
    Value src[3];
    uint64_t immediate = 0;
    uint32_t native_target = 0;
    uint8_t reg_index = 0;
    bool patched = false;

    uint8_t mem_size = 4;
    int32_t mem_disp = 0;
    bool mem_has_base = false;
    bool mem_has_index = false;
    uint8_t mem_base_reg = 0;
    uint8_t mem_index_reg = 0;
    uint8_t mem_scale = 1;
};

struct Block {
    uint32_t guest_address = 0;
    uint32_t next_guest_address = 0xFFFFFFFF;
    std::vector<Instruction> instructions;
    uint8_t flags_live_out = 0;
};

} // namespace pas::cpu::ir
