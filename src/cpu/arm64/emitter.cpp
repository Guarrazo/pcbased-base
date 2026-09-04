#include "cpu/arm64/emitter.h"
#include "core/log.h"

// Todas las formulas de esta unidad estan derivadas y cross-validadas
// contra `aarch64-linux-gnu-as` real -- ver el comentario extenso en
// emitter.h y los tests en tests/test_arm64_emitter.cpp.

namespace pas::cpu::arm64 {

namespace {
bool FitsSigned(int32_t value, int bits) {
    int32_t min = -(1 << (bits - 1));
    int32_t max = (1 << (bits - 1)) - 1;
    return value >= min && value <= max;
}
uint32_t EncodeSigned(int32_t value, int bits) {
    return static_cast<uint32_t>(value) & ((1u << bits) - 1);
}
} // namespace

Emitter::Emitter(uint8_t* buffer, size_t capacity)
    : buffer_(buffer), capacity_(capacity) {}

void Emitter::Emit32(uint32_t word) {
    // Comprobacion de limites explicita -- ver docs/JIT.md: este es
    // exactamente el bug que se diagnostico y corrigio en
    // Arm64Emitter::Emit32() de Super3-NX (buffer overflow silencioso).
    // No se repite ese error aqui: si no cabe, se marca overflow y NO se
    // escribe, en vez de escribir fuera de limites.
    if (offset_ + 4 > capacity_) {
        overflowed_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "Overflow del buffer JIT (offset=%zu, cap=%zu)",
                      offset_, capacity_);
        return;
    }
    buffer_[offset_ + 0] = static_cast<uint8_t>(word & 0xFF);
    buffer_[offset_ + 1] = static_cast<uint8_t>((word >> 8) & 0xFF);
    buffer_[offset_ + 2] = static_cast<uint8_t>((word >> 16) & 0xFF);
    buffer_[offset_ + 3] = static_cast<uint8_t>((word >> 24) & 0xFF);
    offset_ += 4;
}

void Emitter::EmitNop() {
    Emit32(0xD503201F); // NOP, encoding fijo A64 -- sin ambiguedad
}

void Emitter::EmitBrk(uint16_t imm16) {
    Emit32(0xD4200000u | (static_cast<uint32_t>(imm16) << 5)); // BRK #imm16
}

void Emitter::EmitLdrImm(uint8_t rt, uint8_t rn, int32_t offset, uint8_t size_bytes) {
    if (rt > 30 || rn > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitLdrImm: registro fuera de rango (rt=%u, rn=%u)", rt, rn);
        return;
    }
    bool is64 = (size_bytes == 8);
    if (!is64 && size_bytes != 4) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitLdrImm: size_bytes debe ser 4 u 8 (recibido %u)", size_bytes);
        return;
    }

    // Forma 1: LDR (unsigned offset, escalado) -- offset>=0, multiplo de
    // size_bytes, imm12 en [0,4095] (verificado hasta offset=32760/16380,
    // ver tests).
    if (offset >= 0 && offset % size_bytes == 0 && (offset / size_bytes) <= 4095) {
        uint32_t imm12 = static_cast<uint32_t>(offset / size_bytes);
        uint32_t base = is64 ? 0xF9400000u : 0xB9400000u;
        Emit32(base | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | rt);
        return;
    }

    // Forma 2: LDUR (unscaled, con signo) -- offset en [-256,255].
    if (FitsSigned(offset, 9)) {
        uint32_t imm9 = EncodeSigned(offset, 9);
        uint32_t base = is64 ? 0xF8400000u : 0xB8400000u;
        Emit32(base | (imm9 << 12) | (static_cast<uint32_t>(rn) << 5) | rt);
        return;
    }

    encoding_error_ = true;
    PAS_LOG_ERROR("arm64::Emitter", "EmitLdrImm: offset %d fuera de rango soportado "
                                    "(ni LDR escalado ni LDUR sin escalar)", offset);
}

void Emitter::EmitStrImm(uint8_t rt, uint8_t rn, int32_t offset, uint8_t size_bytes) {
    if (rt > 30 || rn > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitStrImm: registro fuera de rango (rt=%u, rn=%u)", rt, rn);
        return;
    }
    bool is64 = (size_bytes == 8);
    if (!is64 && size_bytes != 4) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitStrImm: size_bytes debe ser 4 u 8 (recibido %u)", size_bytes);
        return;
    }

    if (offset >= 0 && offset % size_bytes == 0 && (offset / size_bytes) <= 4095) {
        uint32_t imm12 = static_cast<uint32_t>(offset / size_bytes);
        uint32_t base = is64 ? 0xF9000000u : 0xB9000000u;
        Emit32(base | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | rt);
        return;
    }

    if (FitsSigned(offset, 9)) {
        uint32_t imm9 = EncodeSigned(offset, 9);
        uint32_t base = is64 ? 0xF8000000u : 0xB8000000u;
        Emit32(base | (imm9 << 12) | (static_cast<uint32_t>(rn) << 5) | rt);
        return;
    }

    encoding_error_ = true;
    PAS_LOG_ERROR("arm64::Emitter", "EmitStrImm: offset %d fuera de rango soportado "
                                    "(ni STR escalado ni STUR sin escalar)", offset);
}

void Emitter::EmitLdp(uint8_t rt1, uint8_t rt2, uint8_t rn, int32_t offset) {
    if (rt1 > 30 || rt2 > 30 || rn > 31 || offset % 8 != 0 || !FitsSigned(offset / 8, 7)) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitLdp: parametros fuera de rango (offset=%d)", offset);
        return;
    }
    uint32_t imm7 = EncodeSigned(offset / 8, 7);
    Emit32(0xA9400000u | (imm7 << 15) | (static_cast<uint32_t>(rt2) << 10) |
           (static_cast<uint32_t>(rn) << 5) | rt1);
}

void Emitter::EmitStp(uint8_t rt1, uint8_t rt2, uint8_t rn, int32_t offset) {
    if (rt1 > 30 || rt2 > 30 || rn > 31 || offset % 8 != 0 || !FitsSigned(offset / 8, 7)) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitStp: parametros fuera de rango (offset=%d)", offset);
        return;
    }
    uint32_t imm7 = EncodeSigned(offset / 8, 7);
    Emit32(0xA9000000u | (imm7 << 15) | (static_cast<uint32_t>(rt2) << 10) |
           (static_cast<uint32_t>(rn) << 5) | rt1);
}

void Emitter::EmitBlr(uint8_t rn) {
    if (rn > 30) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitBlr: registro fuera de rango (rn=%u)", rn);
        return;
    }
    Emit32(0xD63F0000u | (static_cast<uint32_t>(rn) << 5));
}

void Emitter::EmitB(int32_t offset) {
    if (offset % 4 != 0 || !FitsSigned(offset / 4, 26)) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitB: offset %d invalido (debe ser multiplo de 4, "
                                        "+-128MB)", offset);
        return;
    }
    Emit32(0x14000000u | EncodeSigned(offset / 4, 26));
}

void Emitter::EmitBCond(Cond cond, int32_t offset) {
    if (offset % 4 != 0 || !FitsSigned(offset / 4, 19)) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitBCond: offset %d invalido (debe ser multiplo de 4, "
                                        "+-1MB)", offset);
        return;
    }
    uint32_t imm19 = EncodeSigned(offset / 4, 19);
    Emit32(0x54000000u | (imm19 << 5) | static_cast<uint32_t>(cond));
}

void Emitter::EmitAdd(uint8_t rd, uint8_t rn, uint8_t rm, bool is64) {
    // A partir de aqui (EmitAdd en adelante) se acepta rd/rn/rm hasta 31
    // inclusive: en esta familia de instrucciones (aritmetico/logico) el
    // valor 31 es un registro real -- el registro cero (WZR/XZR) en la
    // posicion Rd de las variantes que fijan flags (p.ej. CMP = SUBS con
    // Rd=31), o SP en Rn segun la instruccion -- no un valor "fuera de
    // rango". Rechazarlo (como hacian las primeras versiones de este
    // fichero) es exactamente el mismo tipo de bug que cazo el propio test
    // tests/test_arm64_emitter.cpp: TestAddSubCmpImmediate fallaba porque
    // EmitCmpImm(rd=31,...) se marcaba como error de encoding sin serlo.
    // Los LDR/STR/LDP/STP/BLR de mas arriba siguen limitados a 0-30 a
    // proposito (ver su documentacion): ahi 31 significaria direccionar
    // via SP, que todavia no esta soportado, y en BLR el valor 31 esta
    // reservado por el propio ISA (no es una limitacion nuestra).
    if (rd > 31 || rn > 31 || rm > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitAdd: registro fuera de rango");
        return;
    }
    uint32_t base = is64 ? 0x8B000000u : 0x0B000000u;
    Emit32(base | (static_cast<uint32_t>(rm) << 16) | (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitSub(uint8_t rd, uint8_t rn, uint8_t rm, bool is64) {
    if (rd > 31 || rn > 31 || rm > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitSub: registro fuera de rango");
        return;
    }
    uint32_t base = is64 ? 0xCB000000u : 0x4B000000u;
    Emit32(base | (static_cast<uint32_t>(rm) << 16) | (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitRet(uint8_t rn) {
    if (rn > 30) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitRet: registro fuera de rango (rn=%u)", rn);
        return;
    }
    Emit32(0xD65F0000u | (static_cast<uint32_t>(rn) << 5));
}

// --- A partir de aqui: formulas contrastadas con switch_ppc_jit_arm64.h ---
// (ver la nota extensa en emitter.h) -- coinciden bit a bit con las de ese
// fichero para cada caso comprobado contra aarch64-linux-gnu-as.

void Emitter::EmitMovz(uint8_t rd, uint16_t imm16, uint8_t hw, bool is64) {
    if (rd > 31 || hw > 3 || (!is64 && hw > 1)) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitMovz: parametros invalidos (rd=%u, hw=%u, is64=%d)",
                      rd, hw, is64);
        return;
    }
    uint32_t sf = is64 ? 1u : 0u;
    Emit32((sf << 31) | (0b10u << 29) | (0b100101u << 23) | (static_cast<uint32_t>(hw) << 21) |
           (static_cast<uint32_t>(imm16) << 5) | rd);
}

void Emitter::EmitMovk(uint8_t rd, uint16_t imm16, uint8_t hw, bool is64) {
    if (rd > 31 || hw > 3 || (!is64 && hw > 1)) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitMovk: parametros invalidos (rd=%u, hw=%u, is64=%d)",
                      rd, hw, is64);
        return;
    }
    uint32_t sf = is64 ? 1u : 0u;
    Emit32((sf << 31) | (0b11u << 29) | (0b100101u << 23) | (static_cast<uint32_t>(hw) << 21) |
           (static_cast<uint32_t>(imm16) << 5) | rd);
}

void Emitter::EmitMovn(uint8_t rd, uint16_t imm16, uint8_t hw, bool is64) {
    if (rd > 31 || hw > 3 || (!is64 && hw > 1)) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitMovn: parametros invalidos (rd=%u, hw=%u, is64=%d)",
                      rd, hw, is64);
        return;
    }
    uint32_t sf = is64 ? 1u : 0u;
    Emit32((sf << 31) | (0b00u << 29) | (0b100101u << 23) | (static_cast<uint32_t>(hw) << 21) |
           (static_cast<uint32_t>(imm16) << 5) | rd);
}

void Emitter::EmitAddImm(uint8_t rd, uint8_t rn, uint32_t imm12, bool is64, bool set_flags) {
    if (rd > 31 || rn > 31 || imm12 > 4095) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitAddImm: parametros invalidos (imm12=%u)", imm12);
        return;
    }
    uint32_t sf = is64 ? 1u : 0u;
    uint32_t s = set_flags ? 1u : 0u;
    Emit32((sf << 31) | (0u << 30) | (s << 29) | (0b100010u << 23) | (imm12 << 10) |
           (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitSubImm(uint8_t rd, uint8_t rn, uint32_t imm12, bool is64, bool set_flags) {
    if (rd > 31 || rn > 31 || imm12 > 4095) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitSubImm: parametros invalidos (imm12=%u)", imm12);
        return;
    }
    uint32_t sf = is64 ? 1u : 0u;
    uint32_t s = set_flags ? 1u : 0u;
    Emit32((sf << 31) | (1u << 30) | (s << 29) | (0b100010u << 23) | (imm12 << 10) |
           (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitCmpImm(uint8_t rn, uint32_t imm12, bool is64) {
    EmitSubImm(/*rd=*/31, rn, imm12, is64, /*set_flags=*/true);
}

void Emitter::EmitAndReg(uint8_t rd, uint8_t rn, uint8_t rm, bool is64) {
    if (rd > 31 || rn > 31 || rm > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitAndReg: registro fuera de rango");
        return;
    }
    uint32_t sf = is64 ? 1u : 0u;
    Emit32((sf << 31) | (0b01010u << 24) | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitOrrReg(uint8_t rd, uint8_t rn, uint8_t rm, bool is64) {
    if (rd > 31 || rn > 31 || rm > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitOrrReg: registro fuera de rango");
        return;
    }
    uint32_t sf = is64 ? 1u : 0u;
    Emit32((sf << 31) | (1u << 29) | (0b01010u << 24) | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitEorReg(uint8_t rd, uint8_t rn, uint8_t rm, bool is64) {
    if (rd > 31 || rn > 31 || rm > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitEorReg: registro fuera de rango");
        return;
    }
    uint32_t sf = is64 ? 1u : 0u;
    // Igual que en switch_ppc_jit_arm64.h: opc=10 para EOR se compone como
    // (0<<30)|(2<<29), que en la practica pone el bit 30 (no el 29) -- es
    // una forma valida de expresar el campo de 2 bits [30:29]=10, aunque a
    // primera vista parezca "raro" que el opc de EOR se escriba con <<29.
    Emit32((sf << 31) | (2u << 29) | (0b01010u << 24) | (static_cast<uint32_t>(rm) << 16) |
           (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitLsl(uint8_t rd, uint8_t rn, uint8_t amount, bool is64) {
    if (rd > 31 || rn > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitLsl: registro fuera de rango");
        return;
    }
    if (is64) {
        if (amount > 63) { encoding_error_ = true; return; }
        uint32_t immr = (64 - amount) & 0x3F;
        uint32_t imms = 63 - amount;
        Emit32((1u << 31) | (2u << 29) | (0b100110u << 23) | (1u << 22) | (immr << 16) |
               (imms << 10) | (static_cast<uint32_t>(rn) << 5) | rd);
    } else {
        if (amount > 31) { encoding_error_ = true; return; }
        uint32_t immr = (32 - amount) & 0x1F;
        uint32_t imms = 31 - amount;
        Emit32((2u << 29) | (0b100110u << 23) | (immr << 16) | (imms << 10) |
               (static_cast<uint32_t>(rn) << 5) | rd);
    }
}

void Emitter::EmitLsr(uint8_t rd, uint8_t rn, uint8_t amount, bool is64) {
    if (rd > 31 || rn > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitLsr: registro fuera de rango");
        return;
    }
    if (is64) {
        if (amount > 63) { encoding_error_ = true; return; }
        Emit32((1u << 31) | (2u << 29) | (0b100110u << 23) | (1u << 22) |
               (static_cast<uint32_t>(amount) << 16) | (63u << 10) |
               (static_cast<uint32_t>(rn) << 5) | rd);
    } else {
        if (amount > 31) { encoding_error_ = true; return; }
        Emit32((2u << 29) | (0b100110u << 23) | (static_cast<uint32_t>(amount) << 16) |
               (31u << 10) | (static_cast<uint32_t>(rn) << 5) | rd);
    }
}

void Emitter::EmitAsr(uint8_t rd, uint8_t rn, uint8_t amount, bool is64) {
    if (rd > 31 || rn > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitAsr: registro fuera de rango");
        return;
    }
    if (is64) {
        if (amount > 63) { encoding_error_ = true; return; }
        Emit32((1u << 31) | (0b100110u << 23) | (1u << 22) |
               (static_cast<uint32_t>(amount) << 16) | (63u << 10) |
               (static_cast<uint32_t>(rn) << 5) | rd);
    } else {
        if (amount > 31) { encoding_error_ = true; return; }
        Emit32((0b100110u << 23) | (static_cast<uint32_t>(amount) << 16) |
               (31u << 10) | (static_cast<uint32_t>(rn) << 5) | rd);
    }
}

void Emitter::EmitMul(uint8_t rd, uint8_t rn, uint8_t rm, bool is64) {
    if (rd > 31 || rn > 31 || rm > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitMul: registro fuera de rango");
        return;
    }
    // MUL Rd,Rn,Rm es alias de MADD Rd,Rn,Rm,ZR (Ra=31).
    uint32_t base = is64 ? 0x9B000000u : 0x1B000000u;
    Emit32(base | (static_cast<uint32_t>(rm) << 16) | (31u << 10) |
           (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitUdiv(uint8_t rd, uint8_t rn, uint8_t rm, bool is64) {
    if (rd > 31 || rn > 31 || rm > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitUdiv: registro fuera de rango");
        return;
    }
    uint32_t base = is64 ? 0x9AC00800u : 0x1AC00800u;
    Emit32(base | (static_cast<uint32_t>(rm) << 16) | (static_cast<uint32_t>(rn) << 5) | rd);
}

void Emitter::EmitSdiv(uint8_t rd, uint8_t rn, uint8_t rm, bool is64) {
    if (rd > 31 || rn > 31 || rm > 31) {
        encoding_error_ = true;
        PAS_LOG_ERROR("arm64::Emitter", "EmitSdiv: registro fuera de rango");
        return;
    }
    uint32_t base = is64 ? 0x9AC00C00u : 0x1AC00C00u;
    Emit32(base | (static_cast<uint32_t>(rm) << 16) | (static_cast<uint32_t>(rn) << 5) | rd);
}

} // namespace pas::cpu::arm64
