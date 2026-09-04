#include "cpu/jit/jit.h"
#include "core/log.h"

namespace pas::cpu::jit {

Jit::Jit(std::unique_ptr<IExecutableMemory> executable_memory,
         uint8_t* guest_mem_base,
         uint32_t guest_mem_base_guest,
         uint32_t guest_mem_size)
    : code_cache_(std::move(executable_memory)),
      guest_mem_base_(guest_mem_base),
      guest_mem_base_guest_(guest_mem_base_guest),
      guest_mem_size_(guest_mem_size) {}

void Jit::RunFrom(uint32_t guest_entry_point, X86CpuState& state) {
    PAS_LOG_INFO("Jit", "RunFrom(entry=0x%08x)", guest_entry_point);
    state.pc = guest_entry_point;
    state.mem_base = guest_mem_base_;
    state.mem_base_guest = guest_mem_base_guest_;
    state.mem_size = guest_mem_size_;

    constexpr size_t kMaxIterations = 1000000;
    size_t iterations = 0;

    while (state.pc != 0xFFFFFFFF && iterations < kMaxIterations) {
        const CachedBlock* block = code_cache_.Find(state.pc);
        if (!block) {
            block = TranslateBlock(state.pc, guest_mem_base_, guest_mem_size_);
            if (!block) {
                PAS_LOG_ERROR("Jit", "Fallo traduccion en 0x%08x", state.pc);
                return;
            }
        }

        uint8_t* code = code_cache_.ExecutableAddress(*block);
        if (!code) {
            PAS_LOG_ERROR("Jit", "ExecutableAddress nullptr en 0x%08x", state.pc);
            return;
        }

        using BlockFn = void (*)(X86CpuState*);
        auto fn = reinterpret_cast<BlockFn>(code);
        fn(&state);

        PAS_LOG_INFO("Jit", "Bloque 0x%08x -> pc=0x%08x", block->guest_address, state.pc);
        iterations++;
    }

    if (iterations >= kMaxIterations) {
        PAS_LOG_ERROR("Jit", "Limite iteraciones (%zu)", kMaxIterations);
    } else {
        PAS_LOG_INFO("Jit", "Completado tras %zu transiciones", iterations);
    }
}

const CachedBlock* Jit::TranslateBlock(uint32_t guest_address,
                                        const uint8_t* guest_memory_base,
                                        size_t guest_memory_size) {
    if (guest_address >= guest_memory_size) {
        PAS_LOG_ERROR("Jit", "Direccion fuera de rango: 0x%08x", guest_address);
        return nullptr;
    }

    x86::X86Instruction instructions[64];
    size_t count = 0;
    uint32_t cursor = guest_address;

    while (count < 64 && cursor < guest_memory_size) {
        x86::X86Instruction inst;
        if (!decoder_.DecodeOne(guest_memory_base + cursor, guest_memory_size - cursor,
                                 cursor, inst)) {
            break;
        }
        instructions[count++] = inst;
        cursor += inst.length;
        if (inst.length == 0) break;

        bool ends_block = (inst.opcode == x86::Opcode::Ret || inst.opcode == x86::Opcode::Call ||
                            inst.opcode == x86::Opcode::Jmp || inst.opcode == x86::Opcode::Jcc);
        if (ends_block) break;
    }

    if (count == 0) {
        PAS_LOG_ERROR("Jit", "Ninguna instruccion en 0x%08x", guest_address);
        return nullptr;
    }

    ir::Block ir_block = ir_builder_.BuildBlock(instructions, count);
    ir_builder_.ApplyPatches(ir_block);

    if (ir_block.instructions.empty()) {
        PAS_LOG_ERROR("Jit", "IrBuilder vacio en 0x%08x", guest_address);
        return nullptr;
    }

    uint8_t arm64_buffer[2048];
    arm64::Emitter emitter(arm64_buffer, sizeof(arm64_buffer));

    Arm64CodeGen codegen(guest_mem_base_, guest_mem_base_guest_);
    if (!codegen.Generate(ir_block, emitter)) {
        PAS_LOG_ERROR("Jit", "Arm64CodeGen fallo en 0x%08x", guest_address);
        return nullptr;
    }
    if (emitter.HadEncodingError() || emitter.Overflowed()) {
        PAS_LOG_ERROR("Jit", "Emisor error en 0x%08x", guest_address);
        return nullptr;
    }

    const CachedBlock* block = code_cache_.Insert(guest_address, arm64_buffer,
                                                   emitter.BytesWritten());
    if (!block) {
        PAS_LOG_ERROR("Jit", "CodeCache.Insert fallo en 0x%08x", guest_address);
        return nullptr;
    }

    PAS_LOG_INFO("Jit", "Traducido: guest=0x%08x, %zu x86 -> %zu IR -> %zu bytes ARM64",
                guest_address, count, ir_block.instructions.size(), emitter.BytesWritten());
    return block;
}

} // namespace pas::cpu::jit
