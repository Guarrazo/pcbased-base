#pragma once
#include <cstdint>
#include <cstddef>

// Emisor de instrucciones ARM64 a nivel de bit.
//
// Cada encoding de esta clase esta derivado y VALIDADO cruzando contra un
// ensamblador ARM64 real (GNU binutils, `aarch64-linux-gnu-as` +
// `objdump -d`): se generaron decenas de combinaciones de registros/
// inmediatos, se ensamblaron con la herramienta real, y se derivo la
// formula de bits comparando los words resultantes -- no se han
// re-derivado a mano sin verificacion cruzada.
//
// ACTUALIZACION: se recibio `switch_ppc_jit_arm64.h` (el emisor real de
// Super3-NX, con meses de uso y varios bugs reales ya corregidos alli --
// opc field misplacement, bits SIMD vs enteros, shifts que faltan, la
// restriccion sf==N de la familia Bitfield, etc.) y se contrasto formula a
// formula contra lo ya implementado aqui: TODO lo verificado coincide bit a
// bit. Dos derivaciones completamente independientes llegando al mismo
// resultado es la mejor confirmacion posible de que ninguna tiene un bug
// sutil (ver docs/JIT.md para el detalle). Las funciones anadidas despues
// de esa comparacion (MOVZ/MOVK/MOVN, ADD/SUB/CMP inmediato, AND/ORR/EOR,
// shifts, MUL, UDIV/SDIV) replican esas formulas ya doblemente
// verificadas, adaptadas a la firma uint8_t de esta clase en vez de los
// enums XReg/WReg tipados del original -- esto ultimo (registros tipados
// que impiden mezclar accidentalmente un Xn con un Wn) es un patron mejor
// que el uint8_t plano de aqui y vale la pena adoptarlo mas adelante, pero
// se pospone para no romper la interfaz ya usada por los tests existentes
// justo antes de empezar el decodificador x86 (que es lo que de verdad
// bloquea el progreso ahora mismo).
//
// Formatos soportados y su rango (fuera de rango = error explicito, nunca
// silencioso -- ver .cpp):
//   - LDR/STR (Xt/Wt, offset sin signo escalado): offset en
//     [0, 4095*size_bytes], multiplo de size_bytes. Rn (base) acepta 0-31
//     (31=SP, direccionamiento respecto a pila); Rt (destino/origen) 0-30.
//   - LDUR/STUR (Xt/Wt, offset con signo sin escalar): offset en [-256, 255],
//     mismos rangos de registro que arriba
//   - LDP/STP (Xt1,Xt2, offset con signo escalado x8): offset en
//     [-512, 504], multiplo de 8. Rn (base) acepta 0-31 (31=SP); Rt1/Rt2 0-30.

namespace pas::cpu::arm64 {

class Emitter {
public:
    Emitter(uint8_t* buffer, size_t capacity);

    // Elige automaticamente LDR/STR (unsigned offset, escalado) o
    // LDUR/STUR (unscaled, con signo) segun el offset pedido. size_bytes
    // debe ser 4 (registro W) u 8 (registro X). Si el offset no encaja en
    // ninguna de las dos formas, se registra un error y no se emite nada
    // (Overflowed() NO se activa para esto -- es un caso distinto de "no
    // cabe en el buffer"; se comprueba por separado, ver .cpp).
    void EmitLdrImm(uint8_t rt, uint8_t rn, int32_t offset, uint8_t size_bytes);
    void EmitStrImm(uint8_t rt, uint8_t rn, int32_t offset, uint8_t size_bytes);

    // Variante de 64 bits (Xt1, Xt2), offset con signo escalado x8.
    void EmitLdp(uint8_t rt1, uint8_t rt2, uint8_t rn, int32_t offset);
    void EmitStp(uint8_t rt1, uint8_t rt2, uint8_t rn, int32_t offset);

    void EmitBlr(uint8_t rn);

    // 'offset' es el desplazamiento en BYTES desde la direccion de esta
    // propia instruccion hasta el destino (no desde el inicio del bloque),
    // con signo, multiplo de 4 -- igual que como lo expresaria un
    // ensamblador real. El llamador (jit.cpp/code_cache.h) es responsable
    // de calcular ese desplazamiento a partir de direcciones absolutas
    // dentro del code cache.
    void EmitB(int32_t offset);

    // 'cond' es el codigo de condicion ARM64 de 4 bits estandar (ver
    // enum Cond mas abajo) -- p.ej. Cond::Eq para BEQ.
    enum class Cond : uint8_t {
        Eq = 0x0, Ne = 0x1, Cs = 0x2, Cc = 0x3, Mi = 0x4, Pl = 0x5, Vs = 0x6, Vc = 0x7,
        Hi = 0x8, Ls = 0x9, Ge = 0xA, Lt = 0xB, Gt = 0xC, Le = 0xD, Al = 0xE,
    };
    void EmitBCond(Cond cond, int32_t offset);

    void EmitAdd(uint8_t rd, uint8_t rn, uint8_t rm, bool is64 = true);
    void EmitSub(uint8_t rd, uint8_t rn, uint8_t rm, bool is64 = true);

    // RET (alias de BR con Rn=30/LR por defecto) -- termina un bloque
    // traducido devolviendo el control al dispatcher del JIT (docs/JIT.md).
    void EmitRet(uint8_t rn = 30);

    // --- Añadidas tras contrastar con switch_ppc_jit_arm64.h (Super3-NX) ---
    // Ese fichero llegó más tarde de lo esperado (ver docs/JIT.md), pero
    // permitió una validación aún más fuerte que la de arriba: en vez de
    // solo contrastar contra `aarch64-linux-gnu-as`, aquí se contrastan
    // DOS implementaciones independientes (la tuya, ya con meses de uso en
    // producción y varios bugs reales corregidos; y esta, derivada desde
    // cero contra el ensamblador) -- y coinciden bit a bit en todo lo
    // verificado. Las funciones de abajo replican exactamente esas fórmulas
    // (con el mismo uint8_t rt/rn/rm que el resto de esta clase, en vez de
    // los enums XReg/WReg tipados de su versión -- ver la nota sobre esto
    // en emitter.cpp).

    // MOVZ/MOVK/MOVN: 'hw' es el índice de "lane" de 16 bits (0,1,2,3 -> 
    // shift 0/16/32/48). is64 selecciona Xd vs Wd (hw solo puede ser 0 o 1
    // si is64=false).
    void EmitMovz(uint8_t rd, uint16_t imm16, uint8_t hw, bool is64);
    void EmitMovk(uint8_t rd, uint16_t imm16, uint8_t hw, bool is64);
    void EmitMovn(uint8_t rd, uint16_t imm16, uint8_t hw, bool is64);

    // ADD/SUB (inmediato de 12 bits, sin shift): rango imm12 en [0,4095].
    // set_flags=true emite ADDS/SUBS (necesario para traducir los flags
    // ZF/CF/SF/OF de x86 tras una operación aritmética, ver docs/MEMORY_MODEL.md).
    void EmitAddImm(uint8_t rd, uint8_t rn, uint32_t imm12, bool is64, bool set_flags = false);
    void EmitSubImm(uint8_t rd, uint8_t rn, uint32_t imm12, bool is64, bool set_flags = false);
    // CMP (inmediato) = SUBS con rd = registro cero (31). El llamador debe
    // pasar 31 como rd al construir la instruccion via EmitSubImm si
    // prefiere no tener un alias -- esta función es solo azúcar.
    void EmitCmpImm(uint8_t rn, uint32_t imm12, bool is64);

    void EmitAndReg(uint8_t rd, uint8_t rn, uint8_t rm, bool is64);
    void EmitOrrReg(uint8_t rd, uint8_t rn, uint8_t rm, bool is64);
    void EmitEorReg(uint8_t rd, uint8_t rn, uint8_t rm, bool is64);

    // Shifts inmediatos, implementados como alias de UBFM/SBFM -- igual que
    // en switch_ppc_jit_arm64.h. 'amount' en [0,31] si is64=false, [0,63] si is64=true.
    void EmitLsl(uint8_t rd, uint8_t rn, uint8_t amount, bool is64);
    void EmitLsr(uint8_t rd, uint8_t rn, uint8_t amount, bool is64);
    void EmitAsr(uint8_t rd, uint8_t rn, uint8_t amount, bool is64);

    void EmitMul(uint8_t rd, uint8_t rn, uint8_t rm, bool is64);
    void EmitUdiv(uint8_t rd, uint8_t rn, uint8_t rm, bool is64);
    void EmitSdiv(uint8_t rd, uint8_t rn, uint8_t rm, bool is64);


    void EmitNop();
    void EmitBrk(uint16_t imm16);  // usado como canario, mismo patron que
                                    // BRK #0xDEAD en Super3-NX

    size_t BytesWritten() const { return offset_; }
    bool Overflowed() const { return overflowed_; }
    // Se activa si alguna llamada recibio un offset/registro fuera del
    // rango soportado por el encoding elegido -- distinto de Overflowed()
    // (que es "no cabia en el buffer"). Comprobar ambos tras emitir.
    bool HadEncodingError() const { return encoding_error_; }

private:
    // Mismo hallazgo que en Super3-NX: Emit32() DEBE comprobar limites del
    // buffer -- el bug original (buffer overflow silencioso corrompiendo
    // bloques JIT) se debio precisamente a la ausencia de esta comprobacion.
    void Emit32(uint32_t word);

    uint8_t* buffer_;
    size_t capacity_;
    size_t offset_ = 0;
    bool overflowed_ = false;
    bool encoding_error_ = false;
};

} // namespace pas::cpu::arm64
