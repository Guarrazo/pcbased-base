#pragma once
#include "cpu/x86/decoder.h"
#include "cpu/translator/ir.h"

// Convierte una secuencia de X86Instruction (salida del decoder) en un
// cpu::ir::Block. Aqui es donde el Patch Engine (docs/PATCHING.md) inserta
// sus transformaciones -- ver ApplyPatches() abajo, llamado SIEMPRE tras
// construir el bloque base, nunca mezclado con la decodificacion.

namespace pas::cpu::translator {

class IrBuilder {
public:
    // Construye un bloque IR a partir de instrucciones x86 ya decodificadas,
    // hasta el primer salto/llamada/retorno o el limite de tamano de bloque
    // configurado (ver docs/JIT.md, "Tamano y linkado de bloques").
    ir::Block BuildBlock(const x86::X86Instruction* instructions, size_t count);

    // Aplica los patches/hooks del GameProfile activo que afecten a este
    // bloque (por direccion x86). Se llama desde jit.cpp tras BuildBlock(),
    // nunca desde dentro de BuildBlock() -- mantiene la construccion base
    // independiente de que perfil este activo.
    void ApplyPatches(ir::Block& block);
};

} // namespace pas::cpu::translator
