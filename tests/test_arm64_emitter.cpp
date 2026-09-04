// Test de host para cpu::arm64::Emitter -- cada valor "esperado" de este
// fichero se obtuvo ensamblando la instruccion equivalente con un
// ensamblador ARM64 real (`aarch64-linux-gnu-as` de binutils) y leyendo el
// word resultante con `objdump -d`, NO derivando la formula a mano sin
// contraste. Ver el comentario extenso en src/cpu/arm64/emitter.h.
//
// Ejemplo de como se obtuvo cada valor (reproducible en cualquier maquina
// con binutils-aarch64-linux-gnu instalado):
//   echo "ldr x0, [x1, #16]" | aarch64-linux-gnu-as -o /tmp/t.o -
//   aarch64-linux-gnu-objdump -d /tmp/t.o
#include "cpu/arm64/emitter.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace pas::cpu::arm64;

namespace {

uint32_t WordAt(const std::vector<uint8_t>& buf, size_t index) {
    uint32_t w;
    std::memcpy(&w, buf.data() + index * 4, 4);
    return w;
}

} // namespace

static void TestLdrStrUnsignedOffset64Bit() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitLdrImm(0, 1, 16, 8);   // ldr x0, [x1, #16]  -> f9400820
    e.EmitStrImm(0, 1, 16, 8);   // str x0, [x1, #16]  -> f9000820
    e.EmitLdrImm(0, 1, 32760, 8); // ldr x0, [x1, #32760] -> f97ffc20 (imm12 maximo)
    assert(!e.HadEncodingError());
    assert(!e.Overflowed());
    assert(WordAt(buf, 0) == 0xF9400820u);
    assert(WordAt(buf, 1) == 0xF9000820u);
    assert(WordAt(buf, 2) == 0xF97FFC20u);
    std::printf("OK: TestLdrStrUnsignedOffset64Bit\n");
}

static void TestLdrStrUnsignedOffset32Bit() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitLdrImm(2, 3, 8, 4);   // ldr w2, [x3, #8] -> b9400862
    e.EmitStrImm(2, 3, 8, 4);   // str w2, [x3, #8] -> b9000862
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xB9400862u);
    assert(WordAt(buf, 1) == 0xB9000862u);
    std::printf("OK: TestLdrStrUnsignedOffset32Bit\n");
}

static void TestLdurSturNegativeOffset() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitLdrImm(0, 1, -256, 8); // ldur x0, [x1, #-256] -> f8500020
    e.EmitLdrImm(0, 1, -8, 8);   // ldur x0, [x1, #-8]   -> f85f8020
    e.EmitLdrImm(0, 1, -1, 8);   // ldur x0, [x1, #-1]   -> f85ff020
    e.EmitStrImm(0, 1, -256, 8); // stur x0, [x1, #-256] -> f8100020
    e.EmitLdrImm(2, 3, -8, 4);   // ldur w2, [x3, #-8]   -> b85f8062
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xF8500020u);
    assert(WordAt(buf, 1) == 0xF85F8020u);
    assert(WordAt(buf, 2) == 0xF85FF020u);
    assert(WordAt(buf, 3) == 0xF8100020u);
    assert(WordAt(buf, 4) == 0xB85F8062u);
    std::printf("OK: TestLdurSturNegativeOffset\n");
}

static void TestLdpStp() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitLdp(0, 1, 2, 32);   // ldp x0, x1, [x2, #32] -> a9420440
    e.EmitStp(0, 1, 2, 32);   // stp x0, x1, [x2, #32] -> a9020440
    e.EmitLdp(0, 0, 0, -8);   // ldp x0, x0, [x0, #-8] -> a97f8000
    e.EmitLdp(0, 0, 0, -512); // ldp x0, x0, [x0, #-512] -> a9600000
    e.EmitLdp(0, 0, 0, 504);  // ldp x0, x0, [x0, #504]  -> a95f8000
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xA9420440u);
    assert(WordAt(buf, 1) == 0xA9020440u);
    assert(WordAt(buf, 2) == 0xA97F8000u);
    assert(WordAt(buf, 3) == 0xA9600000u);
    assert(WordAt(buf, 4) == 0xA95F8000u);
    std::printf("OK: TestLdpStp\n");
}

static void TestBlr() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitBlr(0);  // blr x0  -> d63f0000
    e.EmitBlr(5);  // blr x5  -> d63f00a0
    e.EmitBlr(30); // blr x30 -> d63f03c0
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xD63F0000u);
    assert(WordAt(buf, 1) == 0xD63F00A0u);
    assert(WordAt(buf, 2) == 0xD63F03C0u);
    std::printf("OK: TestBlr\n");
}

static void TestBAndBCond() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitB(8);                       // b .+8  -> 14000002
    e.EmitB(16);                      // b .+16 -> 14000004
    e.EmitB(-8);                      // b .-8  -> 17fffffe
    e.EmitBCond(Emitter::Cond::Eq, 16); // b.eq .+16 -> 54000080
    e.EmitBCond(Emitter::Cond::Ne, 16); // b.ne .+16 -> 54000081
    e.EmitBCond(Emitter::Cond::Lt, 16); // b.lt .+16 -> 5400008b
    e.EmitBCond(Emitter::Cond::Cs, -16);// b.cs .-16 -> 54ffff82
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0x14000002u);
    assert(WordAt(buf, 1) == 0x14000004u);
    assert(WordAt(buf, 2) == 0x17FFFFFEu);
    assert(WordAt(buf, 3) == 0x54000080u);
    assert(WordAt(buf, 4) == 0x54000081u);
    assert(WordAt(buf, 5) == 0x5400008Bu);
    assert(WordAt(buf, 6) == 0x54FFFF82u);
    std::printf("OK: TestBAndBCond\n");
}

static void TestAddSub() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitAdd(0, 2, 3);   // add x0, x2, x3  -> 8b030040
    e.EmitSub(0, 2, 3);   // sub x0, x2, x3  -> cb030040
    e.EmitAdd(0, 2, 30);  // add x0, x2, x30 -> 8b1e0040
    e.EmitAdd(1, 5, 30);  // add x1, x5, x30 -> 8b1e00a1
    e.EmitAdd(0, 1, 2, false); // add w0, w1, w2 -> 0b020020
    e.EmitSub(0, 1, 2, false); // sub w0, w1, w2 -> 4b020020
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0x8B030040u);
    assert(WordAt(buf, 1) == 0xCB030040u);
    assert(WordAt(buf, 2) == 0x8B1E0040u);
    assert(WordAt(buf, 3) == 0x8B1E00A1u);
    assert(WordAt(buf, 4) == 0x0B020020u);
    assert(WordAt(buf, 5) == 0x4B020020u);
    std::printf("OK: TestAddSub\n");
}

static void TestRet() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitRet();     // ret    -> d65f03c0
    e.EmitRet(30);   // ret x30-> d65f03c0
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xD65F03C0u);
    assert(WordAt(buf, 1) == 0xD65F03C0u);
    std::printf("OK: TestRet\n");
}

static void TestNopAndBrk() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitNop();          // nop -> d503201f
    e.EmitBrk(0xDEAD);    // brk #0xdead -> d43bd5a0
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xD503201Fu);
    assert(WordAt(buf, 1) == 0xD43BD5A0u);
    std::printf("OK: TestNopAndBrk\n");
}

// --- Instrucciones anadidas tras contrastar con switch_ppc_jit_arm64.h ---
// Valores de referencia obtenidos igual que arriba, con aarch64-linux-gnu-as.

static void TestMovzMovkMovn() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitMovz(0, 0x1234, 0, true);        // movz x0, #0x1234        -> d2824680
    e.EmitMovz(0, 0x1234, 1, true);        // movz x0, #0x1234,lsl#16 -> d2a24680
    e.EmitMovz(1, 0xABCD, 0, false);       // movz w1, #0xabcd        -> 529579a1
    e.EmitMovk(0, 0x5678, 2, true);        // movk x0, #0x5678,lsl#32 -> f2cacf00
    e.EmitMovn(2, 0, 0, true);             // movn x2, #0             -> 92800002
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xD2824680u);
    assert(WordAt(buf, 1) == 0xD2A24680u);
    assert(WordAt(buf, 2) == 0x529579A1u);
    assert(WordAt(buf, 3) == 0xF2CACF00u);
    assert(WordAt(buf, 4) == 0x92800002u);
    std::printf("OK: TestMovzMovkMovn\n");
}

static void TestAddSubCmpImmediate() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitAddImm(0, 1, 100, true, false);   // add x0, x1, #100  -> 91019020
    e.EmitAddImm(0, 1, 100, true, true);    // adds x0, x1, #100 -> b1019020
    e.EmitSubImm(2, 3, 50, false, false);   // sub w2, w3, #50   -> 5100c862
    e.EmitSubImm(2, 3, 50, false, true);    // subs w2, w3, #50  -> 7100c862
    e.EmitCmpImm(5, 10, true);              // cmp x5, #10       -> f10028bf
    e.EmitCmpImm(5, 10, false);             // cmp w5, #10       -> 710028bf
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0x91019020u);
    assert(WordAt(buf, 1) == 0xB1019020u);
    assert(WordAt(buf, 2) == 0x5100C862u);
    assert(WordAt(buf, 3) == 0x7100C862u);
    assert(WordAt(buf, 4) == 0xF10028BFu);
    assert(WordAt(buf, 5) == 0x710028BFu);
    std::printf("OK: TestAddSubCmpImmediate\n");
}

static void TestLogicalRegister() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitAndReg(0, 1, 2, true);  // and x0, x1, x2 -> 8a020020
    e.EmitOrrReg(0, 1, 2, true);  // orr x0, x1, x2 -> aa020020
    e.EmitEorReg(0, 1, 2, true);  // eor x0, x1, x2 -> ca020020
    e.EmitAndReg(0, 1, 2, false); // and w0, w1, w2 -> 0a020020
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0x8A020020u);
    assert(WordAt(buf, 1) == 0xAA020020u);
    assert(WordAt(buf, 2) == 0xCA020020u);
    assert(WordAt(buf, 3) == 0x0A020020u);
    std::printf("OK: TestLogicalRegister\n");
}

static void TestShiftsImmediate() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitLsl(0, 1, 4, true);  // lsl x0, x1, #4 -> d37cec20
    e.EmitLsr(0, 1, 4, true);  // lsr x0, x1, #4 -> d344fc20
    e.EmitAsr(0, 1, 4, true);  // asr x0, x1, #4 -> 9344fc20
    e.EmitLsl(0, 1, 4, false); // lsl w0, w1, #4 -> 531c6c20
    e.EmitLsr(0, 1, 4, false); // lsr w0, w1, #4 -> 53047c20
    e.EmitAsr(0, 1, 4, false); // asr w0, w1, #4 -> 13047c20
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xD37CEC20u);
    assert(WordAt(buf, 1) == 0xD344FC20u);
    assert(WordAt(buf, 2) == 0x9344FC20u);
    assert(WordAt(buf, 3) == 0x531C6C20u);
    assert(WordAt(buf, 4) == 0x53047C20u);
    assert(WordAt(buf, 5) == 0x13047C20u);
    std::printf("OK: TestShiftsImmediate\n");
}

static void TestMulDiv() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitMul(0, 1, 2, true);   // mul x0, x1, x2  -> 9b027c20
    e.EmitMul(0, 1, 2, false);  // mul w0, w1, w2  -> 1b027c20
    e.EmitUdiv(0, 1, 2, true);  // udiv x0, x1, x2 -> 9ac20820
    e.EmitSdiv(0, 1, 2, true);  // sdiv x0, x1, x2 -> 9ac20c20
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0x9B027C20u);
    assert(WordAt(buf, 1) == 0x1B027C20u);
    assert(WordAt(buf, 2) == 0x9AC20820u);
    assert(WordAt(buf, 3) == 0x9AC20C20u);
    std::printf("OK: TestMulDiv\n");
}

static void TestStackPointerAsBaseRegister() {
    // Rn=31 en LDR/STR/LDUR/STUR/LDP/STP significa SP -- valor de
    // codificacion valido, no "fuera de rango" (mismo tipo de correccion
    // que EmitAddImm/EmitCmpImm con Rd=31, ver comentario en emitter.h).
    // Verificado contra aarch64-linux-gnu-as.
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitLdrImm(0, 31, 16, 8);   // ldr x0, [sp, #16]  -> f9400be0
    e.EmitStrImm(0, 31, 16, 8);   // str x0, [sp, #16]  -> f9000be0
    e.EmitLdrImm(0, 31, -8, 8);   // ldur x0, [sp, #-8] -> f85f83e0
    e.EmitStrImm(0, 31, -8, 8);   // stur x0, [sp, #-8] -> f81f83e0
    assert(!e.HadEncodingError());
    assert(WordAt(buf, 0) == 0xF9400BE0u);
    assert(WordAt(buf, 1) == 0xF9000BE0u);
    assert(WordAt(buf, 2) == 0xF85F83E0u);
    assert(WordAt(buf, 3) == 0xF81F83E0u);
    std::printf("OK: TestStackPointerAsBaseRegister\n");
}

static void TestOutOfRangeOffsetIsRejectedNotSilentlyTruncated() {
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitLdrImm(0, 1, 100000, 8); // ni escalado (imm12>4095) ni LDUR (>255)
    assert(e.HadEncodingError());
    assert(e.BytesWritten() == 0); // no debe haber emitido nada corrupto
    std::printf("OK: TestOutOfRangeOffsetIsRejectedNotSilentlyTruncated\n");
}

static void TestBufferOverflowIsDetectedNotWrittenOutOfBounds() {
    std::vector<uint8_t> buf(4, 0xEE); // solo cabe UNA instruccion
    Emitter e(buf.data(), buf.size());
    e.EmitNop();
    assert(!e.Overflowed());
    e.EmitNop(); // esta ya no cabe
    assert(e.Overflowed());
    // El buffer no debe haberse escrito mas alla de su tamano real (no hay
    // forma de comprobar "fuera de limites" desde aqui salvo que ASan lo
    // habria detectado ya al hacer memcpy -- el propio hecho de que este
    // test no crashee con ASan/UBSan activado es parte de la validacion).
    std::printf("OK: TestBufferOverflowIsDetectedNotWrittenOutOfBounds\n");
}

void RunArm64EmitterTests() {
    TestLdrStrUnsignedOffset64Bit();
    TestLdrStrUnsignedOffset32Bit();
    TestLdurSturNegativeOffset();
    TestLdpStp();
    TestBlr();
    TestBAndBCond();
    TestAddSub();
    TestRet();
    TestNopAndBrk();
    TestMovzMovkMovn();
    TestAddSubCmpImmediate();
    TestLogicalRegister();
    TestShiftsImmediate();
    TestMulDiv();
    TestStackPointerAsBaseRegister();
    TestOutOfRangeOffsetIsRejectedNotSilentlyTruncated();
    TestBufferOverflowIsDetectedNotWrittenOutOfBounds();
    std::printf("Todos los tests de arm64_emitter pasaron.\n");
}
