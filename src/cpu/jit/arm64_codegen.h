#pragma once
#include "cpu/translator/ir.h"
#include "cpu/arm64/emitter.h"
#include "cpu/jit/x86_cpu_state.h"
#include <unordered_map>

namespace pas::cpu::jit {

class Arm64CodeGen {
public:
    explicit Arm64CodeGen(uint8_t* mem_base, uint32_t mem_base_guest);
    bool Generate(const ir::Block& block, cpu::arm64::Emitter& emitter);
    static uint8_t MappedRegister(uint8_t x86_reg_index) { return 19 + x86_reg_index; }

private:
    uint8_t* mem_base_ = nullptr;
    uint32_t mem_base_guest_ = 0;
    std::unordered_map<uint32_t, uint8_t> value_location_;
    uint8_t next_scratch_ = 9;

    uint8_t AllocScratch();
    uint8_t LocationOf(ir::Value v) const;
    void EmitPrologue(cpu::arm64::Emitter& emitter);
    void EmitEpilogue(cpu::arm64::Emitter& emitter, uint32_t next_pc);
    bool EmitEffectiveAddress(const ir::Instruction& inst, uint8_t out_reg,
                              cpu::arm64::Emitter& emitter);
};

} // namespace pas::cpu::jit
