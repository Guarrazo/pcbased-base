#include "cpu/jit/jit.h"
#include "core/log.h"

namespace pas::cpu::jit {

Jit::Jit(std::unique_ptr<IExecutableMemory> executable_memory)
    : code_cache_(std::move(executable_memory)) {}

void Jit::RunFrom(uint32_t guest_entry_point, const uint8_t* guest_memory_base,
                   size_t guest_memory_size) {
    PAS_LOG_INFO("Jit", "RunFrom(entry=0x%08x)", guest_entry_point);

    // Bucle de dispatch: traduce a demanda, ejecuta, captura next PC, repite.
    // Convención: cada bloque ARM64 traducido tiene firma uint32_t(*)(void) y
    // devuelve en W0 la siguiente dirección guest a ejecutar (o 0xFFFFFFFF
    // para indicar "terminar", ver Arm64CodeGen::Generate más abajo).
    //
    // Este dispatch es el camino "lento" -- cada transición entre bloques
    // vuelve a C++. Block linking (saltar directamente bloque→bloque sin
    // volver aquí) es optimización futura (docs/JIT.md), no correctitud.
    
    uint32_t current_pc = guest_entry_point;
    const uint32_t kExitSentinel = 0xFFFFFFFF;
    constexpr size_t kMaxIterations = 1000000; // límite de seguridad contra loops infinitos
    size_t iterations = 0;

    while (current_pc != kExitSentinel && iterations < kMaxIterations) {
        const CachedBlock* block = code_cache_.Find(current_pc);
        if (!block) {
            block = TranslateBlock(current_pc, guest_memory_base, guest_memory_size);
            if (!block) {
                PAS_LOG_ERROR("Jit", "Fallo al traducir bloque en 0x%08x, terminando ejecución",
                              current_pc);
                return;
            }
        }

        uint8_t* code = code_cache_.ExecutableAddress(*block);
        if (!code) {
            PAS_LOG_ERROR("Jit", "ExecutableAddress devolvió nullptr para bloque en 0x%08x",
                          current_pc);
            return;
        }

        // Invocar el bloque traducido como función: uint32_t (*)(void).
        // La convención ARM64 AAPCS64 garantiza que W0 (parte baja de X0) es
        // el valor de retorno para tipos de 32 bits.
        using BlockFn = uint32_t (*)(void);
        auto fn = reinterpret_cast<BlockFn>(code);
        
        uint32_t next_pc = fn();
        
        if (next_pc == kExitSentinel) {
            PAS_LOG_INFO("Jit", "Bloque devolvió sentinel de salida, terminando normalmente");
            return;
        }
        
        PAS_LOG_INFO("Jit", "Ejecutado bloque 0x%08x -> siguiente: 0x%08x", current_pc, next_pc);
        current_pc = next_pc;
        iterations++;
    }

    if (iterations >= kMaxIterations) {
        PAS_LOG_ERROR("Jit", "Alcanzado límite de iteraciones (%zu), posible loop infinito", 
                      kMaxIterations);
    } else {
        PAS_LOG_INFO("Jit", "Ejecución completada tras %zu transiciones de bloque", iterations);
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

    // Decodifica hasta un limite de bloque simple (docs/JIT.md: "empezar
    // conservador"), TERMINANDO el bloque en la primera instruccion de
    // control de flujo (Ret/Call/Jmp/Jcc) -- un bloque no debe seguir
    // decodificando mas alla de eso, es la definicion misma de "bloque
    // basico". Se detiene tambien en el primer fallo de decodificacion en
    // vez de continuar con datos parciales.
    while (count < 64 && cursor < guest_memory_size) {
        x86::X86Instruction inst;
        if (!decoder_.DecodeOne(guest_memory_base + cursor, guest_memory_size - cursor,
                                 cursor, inst)) {
            break;
        }
        instructions[count++] = inst;
        cursor += inst.length;
        if (inst.length == 0) break; // evita bucle infinito si length no se rellena

        bool ends_block = (inst.opcode == x86::Opcode::Ret || inst.opcode == x86::Opcode::Call ||
                            inst.opcode == x86::Opcode::Jmp || inst.opcode == x86::Opcode::Jcc);
        if (ends_block) break;
    }

    if (count == 0) {
        PAS_LOG_ERROR("Jit", "Ninguna instruccion decodificada en 0x%08x", guest_address);
        return nullptr;
    }

    ir::Block ir_block = ir_builder_.BuildBlock(instructions, count);
    ir_builder_.ApplyPatches(ir_block);

    if (ir_block.instructions.empty()) {
        PAS_LOG_ERROR("Jit", "IrBuilder no genero ninguna instruccion para el bloque "
                             "en 0x%08x (opcode x86 no soportado todavia, ver el log "
                             "de IrBuilder mas arriba)", guest_address);
        return nullptr;
    }

    // Buffer de trabajo para la emision -- tamano generoso fijo por ahora
    // (docs/JIT.md: bloques pequenos al empezar). Si algun bloque real
    // necesitara mas, esto fallaria de forma visible (Overflowed()), no
    // silenciosa.
    uint8_t arm64_buffer[1024];
    arm64::Emitter emitter(arm64_buffer, sizeof(arm64_buffer));

    Arm64CodeGen codegen; // estado por-bloque, ver jit.h
    if (!codegen.Generate(ir_block, emitter)) {
        PAS_LOG_ERROR("Jit", "Arm64CodeGen fallo para el bloque en 0x%08x", guest_address);
        return nullptr;
    }
    if (emitter.HadEncodingError() || emitter.Overflowed()) {
        PAS_LOG_ERROR("Jit", "El emisor ARM64 reporto un error para el bloque en 0x%08x",
                      guest_address);
        return nullptr;
    }

    const CachedBlock* block = code_cache_.Insert(guest_address, arm64_buffer,
                                                   emitter.BytesWritten());
    if (!block) {
        PAS_LOG_ERROR("Jit", "CodeCache.Insert fallo para el bloque en 0x%08x "
                             "(sin espacio?)", guest_address);
        return nullptr;
    }

    PAS_LOG_INFO("Jit", "Bloque traducido: guest=0x%08x, %zu instrucciones x86 -> "
                        "%zu instrucciones IR -> %zu bytes ARM64",
                guest_address, count, ir_block.instructions.size(), emitter.BytesWritten());
    return block;
}

} // namespace pas::cpu::jit
