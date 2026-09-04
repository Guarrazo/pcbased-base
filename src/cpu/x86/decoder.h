#pragma once
#include <cstdint>
#include <cstddef>

// Decodificador de x86 de 32 bits (subconjunto real usado por binarios
// Lindbergh: base x86 + SSE/SSE2 + x87. Ver docs/CPU_TRANSLATION.md para el
// alcance exacto y por que NO se cubre x86-64/AVX en el MVP).
//
// El decodificador NO ejecuta ni traduce nada por si mismo: produce una
// secuencia de X86Instruction que translator/ir_builder.h convierte a IR.
// Esta separacion es intencional (ver docs/JIT.md, pipeline) para que el
// mismo decodificador sirva tanto al JIT como a un futuro modo interprete
// de validacion diferencial (patron ya usado en Super3-NX con
// switch_ppc_jit_diff.cpp/h).
//
// ALCANCE ACTUAL (ver docs/ROADMAP.md): el subconjunto de opcodes cubierto
// se eligio decodificando la salida REAL de un compilador (GCC -m32,
// tests/fixtures/real_elf32_sample.c -- ver docs/ROADMAP.md sobre por que
// no hay un binario real de Lindbergh disponible todavia) en vez de
// pre-poblar "todo x86" de forma especulativa. Cubre: PUSH/POP r32,
// PUSH r/m32 (0xFF /6), MOV (registro<->registro/memoria, inmediato, moffs
// absoluto), LEA, Grupo 1 con imm8/imm32 (ADD/OR/ADC/SBB/AND/SUB/XOR/CMP
// r/m32,imm8|imm32), CALL rel32, RET, LEAVE. NO cubre todavia: SSE/SSE2/x87 (necesarios para Lindbergh
// pero no ejercitados por este fixture generico), saltos condicionales
// (Jcc/JMP -- el fixture no tiene ninguno; se añaden en la siguiente
// iteracion con su propio fixture que sí tenga control de flujo), ni ModRM
// con SIB completo (el fixture solo usa direccionamiento base+disp, sin
// indice escalado).

namespace pas::cpu::x86 {

// Codificacion de registro tal y como aparece en ModRM/opcode+reg (0-7),
// igual para 8/16/32 bits -- el tamaño lo indica por separado el operando,
// no el numero de registro.
enum class Reg : uint8_t { Eax = 0, Ecx = 1, Edx = 2, Ebx = 3, Esp = 4, Ebp = 5, Esi = 6, Edi = 7 };

enum class Opcode {
    Unknown = 0,
    // TODO: subconjunto real de x86 de 32 bits. Se rellena incrementalmente
    // segun lo que aparezca decodificando binarios reales (ver
    // docs/ROADMAP.md, Nivel 1) -- NO se pre-puebla con "todo x86" de forma
    // especulativa.
    Mov, Lea, Push, Pop, Add, Sub, Or, Adc, Sbb, And, Xor, Cmp,
    Jmp, Jcc, Call, Ret, Leave, Nop,
    // SSE/SSE2 minimo esperado en codigo Pentium 4 de la epoca:
    Movss, Movsd, Addps, Mulps,
    // x87 (codigo legacy que puede seguir presente):
    Fld, Fstp, Fadd, Fmul,
};

// Tamaño de operando en bytes (1/2/4) -- x86 de 32 bits, sin prefijo REX
// (no aplica, no es x86-64) ni necesidad de 8 bytes en el MVP.
enum class OperandSize : uint8_t { Byte = 1, Word = 2, Dword = 4 };

struct X86Operand {
    enum class Kind { None, Register, Memory, Immediate } kind = Kind::None;
    OperandSize size = OperandSize::Dword;

    Reg reg = Reg::Eax;        // si Kind::Register

    // Si Kind::Memory: direccion efectiva = (has_base ? reg[base_reg] : 0)
    //                                      + (has_index ? reg[index_reg]*scale : 0)
    //                                      + disp
    // has_base=false Y has_index=false representa direccionamiento absoluto
    // (moffs, p.ej. `mov eax, ds:0x804c01c`).
    bool has_base = false;
    bool has_index = false;
    Reg base_reg = Reg::Eax;
    Reg index_reg = Reg::Eax;
    uint8_t scale = 1;          // 1,2,4,8 (SIB) -- no usado por el fixture actual
    int32_t disp = 0;

    int64_t immediate = 0;      // si Kind::Immediate (con signo ya aplicado
                                 // si el propio x86 sign-extiende el
                                 // inmediato, p.ej. Grupo1 con imm8)
};

struct X86Instruction {
    Opcode opcode = Opcode::Unknown;
    uint32_t address = 0;      // direccion virtual x86 de esta instruccion
    uint8_t  length = 0;       // bytes que ocupa (para avanzar el puntero)
    X86Operand operands[3];
    uint8_t operand_count = 0;

    // Para Jcc: codigo de condicion x86 crudo (los 4 bits bajos del opcode
    // 0x70-0x7F / 0x0F 0x80-0x8F) -- interpretado por ir_builder.h, no aqui.
    uint8_t condition_code = 0;

    // Flags que esta instruccion consulta/define (ZF, CF, SF, OF, PF, AF).
    // Necesario para el analisis de propagacion de flags del optimizador
    // (docs/JIT.md) -- se calcula en tiempo de decodificacion, no de emision.
    uint8_t reads_flags = 0;
    uint8_t writes_flags = 0;
};

class Decoder {
public:
    // Decodifica UNA instruccion a partir de 'code' (hasta 'max_len' bytes
    // disponibles, para no leer fuera de la seccion mapeada). Devuelve false
    // si el opcode no esta soportado todavia -- el llamador (jit.cpp) debe
    // registrar esto como fallo explicito, nunca ignorarlo en silencio (ver
    // docs/ROADMAP.md, "Superficie de libc-shim subestimada" aplica igual
    // aqui: opcodes no cubiertos deben aparecer en el log, no fallar mudos).
    bool DecodeOne(const uint8_t* code, size_t max_len, uint32_t virtual_address,
                   X86Instruction& out);
};

} // namespace pas::cpu::x86
