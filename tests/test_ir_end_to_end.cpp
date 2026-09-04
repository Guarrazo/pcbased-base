// Test diferencial de extremo a extremo: decode x86 -> IrBuilder -> IR ->
// Arm64CodeGen -> ARM64, y compara EJECUTANDO AMBAS VERSIONES:
//   - el x86 ORIGINAL, de forma nativa en el host x86-64 (Linux soporta
//     ejecutar binarios ia32 estaticos directamente, sin emulacion)
//   - la traduccion ARM64, bajo qemu-aarch64
// para el mismo conjunto de valores de entrada. Si ambas coinciden para
// todos los valores probados, es la validacion mas fuerte posible de todo
// el pipeline de traduccion junto (decoder + IR + codegen), no solo de
// una pieza aislada.
//
// Los bytes x86 son los mismos de tests/test_x86_decoder.cpp, generados
// por GCC real (ver tests/fixtures/real_elf32_sample.c y el comentario de
// cabecera de test_x86_decoder.cpp para como se obtuvieron) -- en este
// caso de una funcion aparte compilada con __attribute__((regparm(1)))
// para forzar que el argumento llegue en EAX y el codigo resultante sea
// puramente de registros (sin operandos de memoria, que IrBuilder/
// Arm64CodeGen no soportan todavia, ver docs/ROADMAP.md):
//
//   __attribute__((regparm(1))) int compute(int a) {
//       int x = a + 5; x = x - 2; x = x & 0xF; return x;
//   }
//
// GCC -O2 -m32 lo optimiza a: `add eax,0x3` / `and eax,0xf` / `ret`
// (pliega 5-2 en la constante 3). Reproducible con
// `gcc -m32 -O2 -fno-pie -c fichero.c && objdump -d -M intel`.
//
// Se salta (no falla) si el entorno no tiene TODAS las herramientas
// necesarias (gcc -m32, gcc-aarch64-linux-gnu, qemu-user) -- ver
// tests/CMakeLists.txt.
#include "cpu/x86/decoder.h"
#include "cpu/translator/ir_builder.h"
#include "cpu/jit/arm64_codegen.h"
#include "cpu/arm64/emitter.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <unistd.h>

using namespace pas::cpu;

namespace {

#if defined(PAS_X86_HARNESS) && defined(PAS_ARM64_HARNESS) && defined(PAS_QEMU_AARCH64)
#define PAS_HAVE_DIFF_TEST_HARNESSES 1

// Bytes x86 reales: add eax,0x3 ; and eax,0xf ; ret (ver comentario de
// cabecera). Los dos primeros son los que se decodifican/traducen; el
// 'ret' final solo se usa para la ejecucion nativa (el traductor produce
// su propio RET a partir del IR, ver mas abajo).
const std::vector<uint8_t> kAddAndInstructions = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f};
const std::vector<uint8_t> kAddAndWithRet = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f, 0xc3};

std::string WriteTempFile(const std::vector<uint8_t>& bytes) {
    std::string tmpl = "/tmp/pas_diff_test_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    assert(fd >= 0);
    close(fd);
    std::string path(buf.data());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return path;
}

int RunViaHarness(const std::string& harness_cmd_prefix, const std::string& bin_path, int64_t input) {
    std::string cmd = harness_cmd_prefix + " " + bin_path + " " + std::to_string(input);
    FILE* pipe = popen(cmd.c_str(), "r");
    assert(pipe != nullptr);
    char line[256] = {0};
    char* got = std::fgets(line, sizeof(line), pipe);
    int rc = pclose(pipe);
    assert(got != nullptr);
    assert(rc == 0);
    return std::atoi(line);
}

// Traduce kAddAndInstructions (decode -> IR -> ARM64), envolviendo el
// resultado con un adaptador de convencion de llamada para que se pueda
// invocar como int64_t(*)(int64_t) desde arm64_exec_harness (que espera
// el resultado en X0 tras RET) -- ver docs/JIT.md, "Convencion de
// registros": el traductor real fija EAX a W20, no a W0, asi que hace
// falta copiar el valor de entrada a W20 antes y el resultado de vuelta a
// W0 despues. Este adaptador es un artefacto de ESTE TEST, no del JIT
// real (el dispatcher real leeria W20 directamente, no necesitaria X0).
std::vector<uint8_t> TranslateToArm64() {
    x86::Decoder decoder;
    std::vector<x86::X86Instruction> instructions;
    size_t offset = 0;
    uint32_t addr = 0;
    while (offset < kAddAndInstructions.size()) {
        x86::X86Instruction inst;
        bool ok = decoder.DecodeOne(kAddAndInstructions.data() + offset,
                                     kAddAndInstructions.size() - offset, addr, inst);
        assert(ok);
        instructions.push_back(inst);
        offset += inst.length;
        addr += inst.length;
    }
    assert(instructions.size() == 2); // add, and

    translator::IrBuilder ir_builder;
    ir::Block block = ir_builder.BuildBlock(instructions.data(), instructions.size());
    assert(block.instructions.size() > 0);

    std::vector<uint8_t> buf(256);
    arm64::Emitter emitter(buf.data(), buf.size());

    // Adaptador: W20 (EAX) <- W0 (entrada del harness)
    emitter.EmitOrrReg(/*rd=*/jit::Arm64CodeGen::MappedRegister(0) /*Eax->20*/,
                        /*rn=*/31, /*rm=*/0, /*is64=*/false);

    jit::Arm64CodeGen codegen;
    bool ok = codegen.Generate(block, emitter);
    assert(ok);

    // Adaptador: W0 (salida esperada por el harness) <- W20 (EAX)
    emitter.EmitOrrReg(/*rd=*/0, /*rn=*/31, /*rm=*/jit::Arm64CodeGen::MappedRegister(0), /*is64=*/false);
    emitter.EmitRet();

    assert(!emitter.HadEncodingError());
    assert(!emitter.Overflowed());
    buf.resize(emitter.BytesWritten());
    return buf;
}

#endif // PAS_HAVE_DIFF_TEST_HARNESSES

} // namespace

static void TestTranslatedArm64MatchesNativeX86ForSeveralInputs() {
#if PAS_HAVE_DIFF_TEST_HARNESSES
    std::vector<uint8_t> arm64_code = TranslateToArm64();
    std::string arm64_bin_path = WriteTempFile(arm64_code);
    std::string x86_bin_path = WriteTempFile(kAddAndWithRet);

    std::string x86_cmd = PAS_X86_HARNESS;
    std::string arm64_cmd = std::string(PAS_QEMU_AARCH64) + " " + PAS_ARM64_HARNESS;

    // Incluye casos borde: 0, negativos, y valores que fuerzan acarreo/
    // desbordamiento en la suma antes del AND -- exactamente los casos
    // donde un bug de traduccion (p.ej. signo mal propagado) se notaria.
    const int test_inputs[] = {0, 1, 5, 10, -1, -5, 100, 255, -100, 2147483647, -2147483648};

    for (int input : test_inputs) {
        int native_result = RunViaHarness(x86_cmd, x86_bin_path, input);
        int translated_result = RunViaHarness(arm64_cmd, arm64_bin_path, input);
        if (native_result != translated_result) {
            std::printf("DISCREPANCIA para entrada %d: nativo x86=%d, traducido ARM64=%d\n",
                        input, native_result, translated_result);
        }
        assert(native_result == translated_result);
    }

    std::remove(arm64_bin_path.c_str());
    std::remove(x86_bin_path.c_str());

    std::printf("OK: TestTranslatedArm64MatchesNativeX86ForSeveralInputs (%zu valores, "
                "x86 nativo == ARM64 traducido en todos)\n", std::size(test_inputs));
#else
    std::printf("SKIP: TestTranslatedArm64MatchesNativeX86ForSeveralInputs (faltan "
                "gcc -m32, gcc-aarch64-linux-gnu o qemu-user)\n");
#endif
}

void RunIrEndToEndTests() {
    TestTranslatedArm64MatchesNativeX86ForSeveralInputs();
    std::printf("Todos los tests de ir_end_to_end pasaron (o se saltaron si no habia entorno).\n");
}
