// Test de host para cpu::x86::Decoder -- los bytes de este test son
// EXACTAMENTE los que produjo un compilador real (GCC -m32 -O0 -fno-pie,
// ver tests/fixtures/real_elf32_sample.c) para su función main(), tal y
// como los mostró `objdump -d -M intel` sobre el binario compilado en este
// mismo entorno. No son bytes inventados ni un ELF sintético -- es la
// misma metodología de "cross-validar contra una herramienta real" que se
// usó para el emisor ARM64 (ver docs/JIT.md), aplicada ahora al
// decodificador.
//
// Reproducible: `gcc -m32 -O0 -fno-pie -no-pie real_elf32_sample.c -o t &&
// objdump -d -M intel t`.
//
// Nota de alcance: esta es una instantánea de bytes concretos, no una
// integración contra el ELF real compilado en tiempo de build (ver
// test_elf_loader_real_binary.cpp para esa otra pieza) -- así este test
// funciona igual en cualquier máquina, tenga o no soporte multilib de 32
// bits instalado.
#include "cpu/x86/decoder.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstdint>

using namespace pas::cpu::x86;

namespace {

// Bytes exactos de main() en el fixture compilado, direccion base 0x08049193
// (ver comentario de cabecera para como se obtuvieron).
const std::vector<uint8_t> kMainBytes = {
    0x8d, 0x4c, 0x24, 0x04,             // lea ecx,[esp+0x4]
    0x83, 0xe4, 0xf0,                   // and esp,0xfffffff0
    0xff, 0x71, 0xfc,                   // push DWORD PTR [ecx-0x4]
    0x55,                               // push ebp
    0x89, 0xe5,                         // mov ebp,esp
    0x51,                               // push ecx
    0x83, 0xec, 0x14,                   // sub esp,0x14
    0x83, 0xec, 0x0c,                   // sub esp,0xc
    0x6a, 0x40,                         // push 0x40
    0xe8, 0xb2, 0xfe, 0xff, 0xff,       // call 8049060 <malloc@plt>
    0x83, 0xc4, 0x10,                   // add esp,0x10
    0x89, 0x45, 0xf4,                   // mov [ebp-0xc],eax
    0x8b, 0x45, 0xf4,                   // mov eax,[ebp-0xc]
    0xc7, 0x00, 0x68, 0x6f, 0x6c, 0x61, // mov DWORD PTR [eax],0x616c6f68
    0xc7, 0x40, 0x04, 0x20, 0x64, 0x65, 0x73, // mov [eax+0x4],0x73656420
    0xc7, 0x40, 0x08, 0x64, 0x65, 0x20, 0x78, // mov [eax+0x8],0x78206564
    0xc7, 0x40, 0x0c, 0x38, 0x36, 0x2d, 0x33, // mov [eax+0xc],0x332d3638
    0xc7, 0x40, 0x10, 0x32, 0x20, 0x72, 0x65, // mov [eax+0x10],0x65722032
    0xc7, 0x40, 0x13, 0x65, 0x61, 0x6c, 0x00, // mov [eax+0x13],0x6c6165
    0x83, 0xec, 0x08,                   // sub esp,0x8
    0x6a, 0x03,                         // push 0x3
    0x6a, 0x02,                         // push 0x2
    0xe8, 0x9a, 0xff, 0xff, 0xff,       // call 8049186 <add>
    0x83, 0xc4, 0x10,                   // add esp,0x10
    0xa3, 0x1c, 0xc0, 0x04, 0x08,       // mov ds:0x804c01c,eax
    0xa1, 0x1c, 0xc0, 0x04, 0x08,       // mov eax,ds:0x804c01c
    0x83, 0xec, 0x04,                   // sub esp,0x4
    0x50,                               // push eax
    0xff, 0x75, 0xf4,                   // push DWORD PTR [ebp-0xc]
    0x68, 0x08, 0xa0, 0x04, 0x08,       // push 0x804a008
    0xe8, 0x36, 0xfe, 0xff, 0xff,       // call 8049040 <printf@plt>
    0x83, 0xc4, 0x10,                   // add esp,0x10
    0x83, 0xec, 0x0c,                   // sub esp,0xc
    0xff, 0x75, 0xf4,                   // push DWORD PTR [ebp-0xc]
    0xe8, 0x38, 0xfe, 0xff, 0xff,       // call 8049050 <free@plt>
    0x83, 0xc4, 0x10,                   // add esp,0x10
    0xb8, 0x00, 0x00, 0x00, 0x00,       // mov eax,0x0
    0x8b, 0x4d, 0xfc,                   // mov ecx,[ebp-0x4]
    0xc9,                               // leave
    0x8d, 0x61, 0xfc,                   // lea esp,[ecx-0x4]
    0xc3,                               // ret
};

constexpr uint32_t kBaseAddress = 0x08049193;

std::vector<X86Instruction> DecodeAll() {
    Decoder decoder;
    std::vector<X86Instruction> result;
    uint32_t addr = kBaseAddress;
    size_t offset = 0;
    while (offset < kMainBytes.size()) {
        X86Instruction inst;
        bool ok = decoder.DecodeOne(kMainBytes.data() + offset, kMainBytes.size() - offset,
                                     addr, inst);
        assert(ok);
        result.push_back(inst);
        offset += inst.length;
        addr += inst.length;
    }
    return result;
}

} // namespace

static void TestDecodesEveryInstructionWithoutError() {
    auto instructions = DecodeAll();
    // 41 instrucciones en total en el fixture (contadas a mano sobre el
    // objdump de cabecera).
    assert(instructions.size() == 41);
    std::printf("OK: TestDecodesEveryInstructionWithoutError (%zu instrucciones)\n",
                instructions.size());
}

static void TestTotalLengthMatchesRealFunctionSize() {
    auto instructions = DecodeAll();
    uint32_t total = 0;
    for (auto& i : instructions) total += i.length;
    // Si el decodificador se desincroniza en algun punto (longitud mal
    // calculada), esto no cuadraria con el tamano real de kMainBytes --
    // es la misma clase de comprobacion "de cuadre" que usa un
    // desensamblador real para detectar decodificacion incorrecta.
    assert(total == kMainBytes.size());
    std::printf("OK: TestTotalLengthMatchesRealFunctionSize (%u bytes)\n", total);
}

static void TestLeaWithSibAddressing() {
    // lea ecx,[esp+0x4] -- ejercita el path de SIB (rm==4) con base=esp,
    // sin indice (index==4 en el SIB significa "sin indice").
    auto instructions = DecodeAll();
    const auto& i0 = instructions[0];
    assert(i0.opcode == Opcode::Lea);
    assert(i0.operands[0].kind == X86Operand::Kind::Register);
    assert(i0.operands[0].reg == Reg::Ecx);
    assert(i0.operands[1].kind == X86Operand::Kind::Memory);
    assert(i0.operands[1].has_base);
    assert(i0.operands[1].base_reg == Reg::Esp);
    assert(!i0.operands[1].has_index);
    assert(i0.operands[1].disp == 4);
    assert(i0.length == 4);
    std::printf("OK: TestLeaWithSibAddressing\n");
}

static void TestGroup1AndImmediateSignExtension() {
    // and esp,0xfffffff0 -- Grupo1 (0x83) con imm8=0xf0, que debe quedar
    // sign-extendido a -16 (0xfffffff0 en 32 bits).
    auto instructions = DecodeAll();
    const auto& i1 = instructions[1];
    assert(i1.opcode == Opcode::And);
    assert(i1.operands[0].kind == X86Operand::Kind::Register);
    assert(i1.operands[0].reg == Reg::Esp);
    assert(i1.operands[1].kind == X86Operand::Kind::Immediate);
    assert(i1.operands[1].immediate == -16);
    std::printf("OK: TestGroup1AndImmediateSignExtension\n");
}

static void TestPushMemoryOperandGroup5() {
    // push DWORD PTR [ecx-0x4] -- 0xFF /6, con disp8 negativo.
    auto instructions = DecodeAll();
    const auto& i2 = instructions[2];
    assert(i2.opcode == Opcode::Push);
    assert(i2.operands[0].kind == X86Operand::Kind::Memory);
    assert(i2.operands[0].has_base);
    assert(i2.operands[0].base_reg == Reg::Ecx);
    assert(i2.operands[0].disp == -4);
    std::printf("OK: TestPushMemoryOperandGroup5\n");
}

static void TestCallTargetResolvedToAbsoluteAddress() {
    // call 8049060 <malloc@plt> -- el decoder debe resolver rel32 a una
    // direccion absoluta correcta (misma que reporta objdump).
    auto instructions = DecodeAll();
    bool found = false;
    for (const auto& i : instructions) {
        if (i.opcode == Opcode::Call && i.operands[0].immediate == 0x08049060) {
            found = true;
            break;
        }
    }
    assert(found);
    std::printf("OK: TestCallTargetResolvedToAbsoluteAddress\n");
}

static void TestMovMoffsAbsoluteAddressing() {
    // mov ds:0x804c01c,eax (0xA3) y mov eax,ds:0x804c01c (0xA1) --
    // direccionamiento absoluto, sin ModRM, sin base ni indice.
    auto instructions = DecodeAll();
    bool found_store = false, found_load = false;
    for (const auto& i : instructions) {
        if (i.opcode != Opcode::Mov || i.operand_count != 2) continue;
        for (int k = 0; k < 2; ++k) {
            const auto& op = i.operands[k];
            if (op.kind == X86Operand::Kind::Memory && !op.has_base && !op.has_index &&
                op.disp == 0x0804c01c) {
                if (k == 0) found_store = true; else found_load = true;
            }
        }
    }
    assert(found_store);
    assert(found_load);
    std::printf("OK: TestMovMoffsAbsoluteAddressing\n");
}

static void TestLeaveAndRet() {
    auto instructions = DecodeAll();
    assert(instructions[instructions.size() - 2].opcode == Opcode::Lea); // lea esp,[ecx-0x4]
    assert(instructions.back().opcode == Opcode::Ret);
    // El leave esta 2 instrucciones antes del ret (leave, lea, ret).
    assert(instructions[instructions.size() - 3].opcode == Opcode::Leave);
    std::printf("OK: TestLeaveAndRet\n");
}

void RunX86DecoderTests() {
    TestDecodesEveryInstructionWithoutError();
    TestTotalLengthMatchesRealFunctionSize();
    TestLeaWithSibAddressing();
    TestGroup1AndImmediateSignExtension();
    TestPushMemoryOperandGroup5();
    TestCallTargetResolvedToAbsoluteAddress();
    TestMovMoffsAbsoluteAddressing();
    TestLeaveAndRet();
    std::printf("Todos los tests de x86_decoder pasaron.\n");
}
