#pragma once
#include "cpu/x86/decoder.h"
#include "cpu/translator/ir_builder.h"
#include "cpu/jit/arm64_codegen.h"
#include "cpu/jit/code_cache.h"
#include "cpu/jit/x86_cpu_state.h"
#include <memory>

namespace pas::cpu::jit {

class Jit {
public:
    explicit Jit(std::unique_ptr<IExecutableMemory> executable_memory,
                 uint8_t* guest_mem_base,
                 uint32_t guest_mem_base_guest,
                 uint32_t guest_mem_size);

    void RunFrom(uint32_t guest_entry_point, X86CpuState& state);

    const CachedBlock* FindTranslatedBlock(uint32_t guest_address) const {
        return code_cache_.Find(guest_address);
    }
    uint8_t* GetExecutableCode(const CachedBlock& block) const {
        return code_cache_.ExecutableAddress(block);
    }

private:
    const CachedBlock* TranslateBlock(uint32_t guest_address,
                                       const uint8_t* guest_memory_base,
                                       size_t guest_memory_size);

    x86::Decoder decoder_;
    translator::IrBuilder ir_builder_;
    CodeCache code_cache_;
    uint8_t* guest_mem_base_ = nullptr;
    uint32_t guest_mem_base_guest_ = 0;
    uint32_t guest_mem_size_ = 0;
};

} // namespace pas::cpu::jit
