#include "cpu/x86/decoder.h"
#include "core/log.h"
#include <cstring>

// Implementacion escrita y validada contra la salida REAL de un compilador
// (ver tests/fixtures/real_elf32_sample.c y docs/ROADMAP.md sobre por que
// no hay un binario real de Lindbergh disponible todavia). El test
// tests/test_x86_decoder.cpp decodifica los bytes reales de la funcion
// main() de ese fixture y compara, instruccion a instruccion, contra la
// salida de `objdump -d -M intel` -- misma metodologia de "cross-validar
// contra una herramienta real" que se uso para el emisor ARM64
// (docs/JIT.md).
//
// NO cubre todavia (ver decoder.h para el detalle completo del alcance):
// prefijo 0x66 (operandos de 16 bits), SSE/SSE2/x87, saltos condicionales,
// ModRM con mod=3 sobre Grupo1 no probado aqui (aunque el codigo lo
// soporta, no ha sido ejercitado por el fixture real todavia).

namespace pas::cpu::x86 {

namespace {

struct Cursor {
    const uint8_t* code;
    size_t max_len;
    size_t pos = 0;

    bool HasBytes(size_t n) const { return pos + n <= max_len; }
    uint8_t ReadU8() { return code[pos++]; }
    int8_t ReadS8() { return static_cast<int8_t>(code[pos++]); }
    uint32_t ReadU32() {
        uint32_t v;
        std::memcpy(&v, code + pos, 4);
        pos += 4;
        return v;
    }
    int32_t ReadS32() { return static_cast<int32_t>(ReadU32()); }
};

// Decodifica ModRM (+ SIB si aplica) según el manual Intel, para
// direccionamiento de 32 bits sin prefijo de direcciones. Devuelve el
// operando "rm" (registro si mod==3, memoria en otro caso) y el campo
// "reg" (usado como registro fuente/destino o como selector de operación
// en instrucciones de grupo, según quien llame). No consume el propio
// byte ModRM -- eso lo hace el llamador antes de invocar esto, para poder
// leer el campo reg antes de decidir qué hacer.
bool DecodeModRmOperand(Cursor& cur, uint8_t modrm, X86Operand& rm_operand) {
    uint8_t mod = (modrm >> 6) & 0x3;
    uint8_t rm = modrm & 0x7;

    if (mod == 3) {
        rm_operand.kind = X86Operand::Kind::Register;
        rm_operand.reg = static_cast<Reg>(rm);
        return true;
    }

    rm_operand.kind = X86Operand::Kind::Memory;
    rm_operand.has_base = true;
    rm_operand.has_index = false;
    rm_operand.scale = 1;
    rm_operand.disp = 0;

    if (rm == 4) {
        // SIB sigue.
        if (!cur.HasBytes(1)) return false;
        uint8_t sib = cur.ReadU8();
        uint8_t ss = (sib >> 6) & 0x3;
        uint8_t index = (sib >> 3) & 0x7;
        uint8_t base = sib & 0x7;

        if (index != 4) { // ESP no puede ser indice -- "sin indice" si index==4
            rm_operand.has_index = true;
            rm_operand.index_reg = static_cast<Reg>(index);
            rm_operand.scale = static_cast<uint8_t>(1u << ss);
        }

        if (base == 5 && mod == 0) {
            // Sin base, disp32 absoluto.
            rm_operand.has_base = false;
            if (!cur.HasBytes(4)) return false;
            rm_operand.disp = cur.ReadS32();
        } else {
            rm_operand.base_reg = static_cast<Reg>(base);
        }
    } else if (mod == 0 && rm == 5) {
        // Sin base, disp32 absoluto (direccionamiento [disp32] directo).
        rm_operand.has_base = false;
        if (!cur.HasBytes(4)) return false;
        rm_operand.disp = cur.ReadS32();
    } else {
        rm_operand.base_reg = static_cast<Reg>(rm);
    }

    if (mod == 1) {
        if (!cur.HasBytes(1)) return false;
        rm_operand.disp += cur.ReadS8(); // += por si SIB ya puso un disp32 (no puede pasar con mod=1, pero es mas robusto)
    } else if (mod == 2) {
        if (!cur.HasBytes(4)) return false;
        rm_operand.disp += cur.ReadS32();
    }

    return true;
}

X86Operand RegOperand(Reg r) {
    X86Operand op;
    op.kind = X86Operand::Kind::Register;
    op.reg = r;
    return op;
}

X86Operand ImmOperand(int64_t value) {
    X86Operand op;
    op.kind = X86Operand::Kind::Immediate;
    op.immediate = value;
    return op;
}

// Grupo 1 (0x80/0x81/0x83): el campo reg de ModRM selecciona la operación.
Opcode Group1Opcode(uint8_t reg_field) {
    switch (reg_field) {
        case 0: return Opcode::Add;
        case 1: return Opcode::Or;
        case 2: return Opcode::Adc;
        case 3: return Opcode::Sbb;
        case 4: return Opcode::And;
        case 5: return Opcode::Sub;
        case 6: return Opcode::Xor;
        case 7: return Opcode::Cmp;
        default: return Opcode::Unknown;
    }
}

} // namespace

bool Decoder::DecodeOne(const uint8_t* code, size_t max_len, uint32_t virtual_address,
                         X86Instruction& out) {
    out = X86Instruction{};
    out.address = virtual_address;

    if (max_len == 0) {
        PAS_LOG_ERROR("x86::Decoder", "DecodeOne: 0 bytes disponibles en 0x%08x", virtual_address);
        return false;
    }

    Cursor cur{code, max_len};
    uint8_t opcode_byte = cur.ReadU8();

    // --- PUSH r32 (0x50-0x57) ---
    if (opcode_byte >= 0x50 && opcode_byte <= 0x57) {
        out.opcode = Opcode::Push;
        out.operands[0] = RegOperand(static_cast<Reg>(opcode_byte - 0x50));
        out.operand_count = 1;
        out.length = static_cast<uint8_t>(cur.pos);
        return true;
    }

    // --- POP r32 (0x58-0x5F) ---
    if (opcode_byte >= 0x58 && opcode_byte <= 0x5F) {
        out.opcode = Opcode::Pop;
        out.operands[0] = RegOperand(static_cast<Reg>(opcode_byte - 0x58));
        out.operand_count = 1;
        out.length = static_cast<uint8_t>(cur.pos);
        return true;
    }

    // --- MOV r32, imm32 (0xB8-0xBF) ---
    if (opcode_byte >= 0xB8 && opcode_byte <= 0xBF) {
        if (!cur.HasBytes(4)) { PAS_LOG_ERROR("x86::Decoder", "MOV r32,imm32 truncado"); return false; }
        out.opcode = Opcode::Mov;
        out.operands[0] = RegOperand(static_cast<Reg>(opcode_byte - 0xB8));
        out.operands[1] = ImmOperand(static_cast<uint32_t>(cur.ReadS32()));
        out.operand_count = 2;
        out.length = static_cast<uint8_t>(cur.pos);
        return true;
    }

    // --- PUSH imm8 sign-extended (0x6A) ---
    if (opcode_byte == 0x6A) {
        if (!cur.HasBytes(1)) return false;
        out.opcode = Opcode::Push;
        out.operands[0] = ImmOperand(cur.ReadS8());
        out.operand_count = 1;
        out.length = static_cast<uint8_t>(cur.pos);
        return true;
    }

    // --- PUSH imm32 (0x68) ---
    if (opcode_byte == 0x68) {
        if (!cur.HasBytes(4)) return false;
        out.opcode = Opcode::Push;
        out.operands[0] = ImmOperand(cur.ReadS32());
        out.operand_count = 1;
        out.length = static_cast<uint8_t>(cur.pos);
        return true;
    }

    // --- CALL rel32 (0xE8) ---
    if (opcode_byte == 0xE8) {
        if (!cur.HasBytes(4)) return false;
        int32_t rel = cur.ReadS32();
        out.opcode = Opcode::Call;
        // Direccion absoluta de destino = direccion de la SIGUIENTE
        // instruccion (ya con los 5 bytes de esta consumidos) + rel32 --
        // se resuelve aqui, no en el IR builder, porque es aritmetica
        // fija de x86 (rip-relative-like) independiente de la traduccion.
        uint32_t next_addr = virtual_address + 5;
        out.operands[0] = ImmOperand(static_cast<uint32_t>(next_addr + static_cast<uint32_t>(rel)));
        out.operand_count = 1;
        out.length = static_cast<uint8_t>(cur.pos);
        return true;
    }

    // --- RET (0xC3), LEAVE (0xC9) ---
    if (opcode_byte == 0xC3) { out.opcode = Opcode::Ret; out.length = static_cast<uint8_t>(cur.pos); return true; }
    if (opcode_byte == 0xC9) { out.opcode = Opcode::Leave; out.length = static_cast<uint8_t>(cur.pos); return true; }

    // --- MOV EAX, moffs32 / MOV moffs32, EAX (0xA1 / 0xA3) ---
    if (opcode_byte == 0xA1 || opcode_byte == 0xA3) {
        if (!cur.HasBytes(4)) return false;
        int32_t addr = cur.ReadS32();
        X86Operand mem;
        mem.kind = X86Operand::Kind::Memory;
        mem.has_base = false;
        mem.has_index = false;
        mem.disp = addr;
        out.opcode = Opcode::Mov;
        if (opcode_byte == 0xA1) {
            out.operands[0] = RegOperand(Reg::Eax);
            out.operands[1] = mem;
        } else {
            out.operands[0] = mem;
            out.operands[1] = RegOperand(Reg::Eax);
        }
        out.operand_count = 2;
        out.length = static_cast<uint8_t>(cur.pos);
        return true;
    }

    // --- Instrucciones con ModRM ---
    if (opcode_byte == 0x89 || opcode_byte == 0x8B || opcode_byte == 0x8D ||
        opcode_byte == 0x83 || opcode_byte == 0x81 || opcode_byte == 0xC7 ||
        opcode_byte == 0xFF) {

        if (!cur.HasBytes(1)) return false;
        uint8_t modrm = cur.ReadU8();
        uint8_t reg_field = (modrm >> 3) & 0x7;

        X86Operand rm_operand;
        if (!DecodeModRmOperand(cur, modrm, rm_operand)) {
            PAS_LOG_ERROR("x86::Decoder", "ModRM/SIB truncado en 0x%08x", virtual_address);
            return false;
        }

        if (opcode_byte == 0x89) { // MOV r/m32, r32
            out.opcode = Opcode::Mov;
            out.operands[0] = rm_operand;
            out.operands[1] = RegOperand(static_cast<Reg>(reg_field));
            out.operand_count = 2;
        } else if (opcode_byte == 0x8B) { // MOV r32, r/m32
            out.opcode = Opcode::Mov;
            out.operands[0] = RegOperand(static_cast<Reg>(reg_field));
            out.operands[1] = rm_operand;
            out.operand_count = 2;
        } else if (opcode_byte == 0x8D) { // LEA r32, m
            if (rm_operand.kind != X86Operand::Kind::Memory) {
                PAS_LOG_ERROR("x86::Decoder", "LEA con operando registro (invalido) en 0x%08x",
                              virtual_address);
                return false;
            }
            out.opcode = Opcode::Lea;
            out.operands[0] = RegOperand(static_cast<Reg>(reg_field));
            out.operands[1] = rm_operand;
            out.operand_count = 2;
        } else if (opcode_byte == 0x83) { // Grupo1 r/m32, imm8 (sign-extended)
            if (!cur.HasBytes(1)) return false;
            out.opcode = Group1Opcode(reg_field);
            out.operands[0] = rm_operand;
            out.operands[1] = ImmOperand(cur.ReadS8());
            out.operand_count = 2;
        } else if (opcode_byte == 0x81) { // Grupo1 r/m32, imm32
            if (!cur.HasBytes(4)) return false;
            out.opcode = Group1Opcode(reg_field);
            out.operands[0] = rm_operand;
            out.operands[1] = ImmOperand(cur.ReadS32());
            out.operand_count = 2;
        } else if (opcode_byte == 0xC7) { // MOV r/m32, imm32 (reg debe ser 0)
            if (reg_field != 0) {
                PAS_LOG_ERROR("x86::Decoder", "0xC7 con reg!=0 (no es MOV) en 0x%08x -- "
                                              "no soportado", virtual_address);
                return false;
            }
            if (!cur.HasBytes(4)) return false;
            out.opcode = Opcode::Mov;
            out.operands[0] = rm_operand;
            out.operands[1] = ImmOperand(cur.ReadS32());
            out.operand_count = 2;
        } else if (opcode_byte == 0xFF) { // Grupo 5 -- solo /6 (PUSH r/m32) soportado
            if (reg_field != 6) {
                PAS_LOG_ERROR("x86::Decoder", "0xFF con reg=%u no soportado todavia (solo "
                                              "/6 PUSH r/m32) en 0x%08x", reg_field, virtual_address);
                return false;
            }
            out.opcode = Opcode::Push;
            out.operands[0] = rm_operand;
            out.operand_count = 1;
        }

        out.length = static_cast<uint8_t>(cur.pos);
        return true;
    }

    PAS_LOG_ERROR("x86::Decoder", "Opcode 0x%02x no soportado todavia en 0x%08x (ver "
                                  "decoder.h para el alcance actual)", opcode_byte, virtual_address);
    return false;
}

} // namespace pas::cpu::x86
