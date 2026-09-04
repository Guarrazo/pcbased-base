#pragma once
#include <cstdint>
#include <vector>

// IR intermedia propia, inspirada en el diseño arquitectónico de FEX (no en
// su código): decodificar -> IR -> optimizar -> backend ARM64, en vez de
// traducción directa instrucción-a-instrucción como hace Box64.
//
// Por qué existe esta capa (ver docs/CPU_TRANSLATION.md y docs/PATCHING.md):
//   1. Es el punto de enganche único del Patch Engine: un patch se expresa
//      como transformación sobre la IR de un bloque, y sobrevive a cambios
//      en cómo se genera el ARM64 final.
//   2. Normaliza la irregularidad de x86 (prefijos, modos de direccionamiento)
//      antes de llegar al backend, que así solo tiene que conocer un
//      conjunto pequeño y regular de nodos.
//
// Deliberadamente pequeña para el MVP: no se modela ni una fracción de lo
// que modelan Box64/FEX, solo lo que hace falta para el subconjunto x86
// real usado por el target Lindbergh (ver docs/CPU_TRANSLATION.md, alcance).

namespace pas::cpu::ir {

enum class OpCode {
    Nop,
    LoadImm,                    // materializa una constante (Instruction::immediate) en dst
    LoadReg, StoreReg,          // registro x86 virtual <-> valor IR (que registro: Instruction::reg_index)
    LoadMem, StoreMem,          // acceso a memoria del proceso emulado
    Add, Sub, Mul,
    And, Or, Xor, Not,
    ShiftLeft, ShiftRight,
    CompareAndSetFlags,         // nodo que modela el calculo de ZF/CF/SF/OF/PF/AF
    Branch, BranchConditional,
    Call, Return,
    // Puente explicito hacia funciones nativas ARM64 (usado por syscall-shim,
    // Cg-shim, GL-shim -- ver docs/WINDOWS_COMPATIBILITY.md, docs/GRAPHICS.md):
    CallNative,
};

struct Value {
    uint32_t id = 0;   // identificador SSA-like dentro del bloque
};

struct Instruction {
    OpCode op = OpCode::Nop;
    Value dst;
    Value src[3];
    uint64_t immediate = 0;
    uint32_t native_target = 0; // indice de funcion nativa si op == CallNative
    uint8_t reg_index = 0;      // que registro x86 (0-7, orden ModRM -- ver
                                 // cpu::x86::Reg) si op == LoadReg/StoreReg

    // Rellenado por el Patch Engine (docs/PATCHING.md) cuando un GameProfile
    // declara un hook/patch que afecta a esta instruccion concreta de IR.
    bool patched = false;
};

struct Block {
    uint32_t guest_address = 0;   // direccion x86 de origen de este bloque
    uint32_t next_guest_address = 0xFFFFFFFF;  // direccion x86 siguiente (0xFFFFFFFF = terminar)
    std::vector<Instruction> instructions;

    // Bits de flags realmente consultados por instrucciones posteriores
    // (propagacion de flags al estilo Kildall, ver docs/JIT.md) -- se
    // calcula en la fase de optimizacion, no en la de construccion inicial.
    uint8_t flags_live_out = 0;
};

} // namespace pas::cpu::ir
