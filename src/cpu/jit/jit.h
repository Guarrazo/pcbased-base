#pragma once
#include "cpu/x86/decoder.h"
#include "cpu/translator/ir_builder.h"
#include "cpu/jit/arm64_codegen.h"
#include "cpu/jit/code_cache.h"
#include <memory>

// Orquestador del pipeline completo (docs/JIT.md):
//   x86 bytes -> Decoder -> IrBuilder (+ patches) -> arm64::Emitter -> CodeCache
//
// Dispatcher a demanda: NO se traduce el binario por adelantado. Se traduce
// el primer bloque a partir del entry point, se ejecuta, y cuando el flujo
// de control llega a una direccion sin traducir, se traduce esa tambien.

namespace pas::cpu::jit {

class Jit {
public:
    explicit Jit(std::unique_ptr<IExecutableMemory> executable_memory);

    // Traduce (si hace falta) y ejecuta desde la direccion x86 dada. La
    // lectura de memoria del proceso emulado (donde estan los bytes x86
    // originales) se resuelve contra el espacio de direcciones que gestiona
    // os/elf_loader/ -- Jit no sabe de ELF, solo de bytes+direcciones.
    void RunFrom(uint32_t guest_entry_point, const uint8_t* guest_memory_base,
                 size_t guest_memory_size);

    // Accesores de solo lectura sobre el estado de traduccion -- usados por
    // tests/test_jit.cpp para verificar la integracion completa (Jit no
    // expone TranslateBlock() directamente, es un detalle interno, pero SI
    // tiene sentido exponer "que hay ya traducido", tambien util para un
    // futuro bucle de dispatch real).
    const CachedBlock* FindTranslatedBlock(uint32_t guest_address) const {
        return code_cache_.Find(guest_address);
    }
    uint8_t* GetExecutableCode(const CachedBlock& block) const {
        return code_cache_.ExecutableAddress(block);
    }

private:
    // Traduce un unico bloque (hasta salto/llamada/retorno, o el limite de
    // tamano configurado) y lo inserta en la cache. Devuelve el bloque
    // cacheado, o nullptr si la traduccion fallo (opcode no soportado,
    // overflow de cache...). Arm64CodeGen se instancia localmente por
    // bloque (no como miembro de Jit) porque lleva estado por-bloque
    // (asignacion de scratch, ubicacion de Values) que NO debe sobrevivir
    // entre bloques -- ver arm64_codegen.h.
    const CachedBlock* TranslateBlock(uint32_t guest_address,
                                       const uint8_t* guest_memory_base,
                                       size_t guest_memory_size);

    x86::Decoder decoder_;
    translator::IrBuilder ir_builder_;
    CodeCache code_cache_;
};

} // namespace pas::cpu::jit
