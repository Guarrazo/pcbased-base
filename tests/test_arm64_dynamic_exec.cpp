// Test de EJECUCION DINAMICA para cpu::arm64::Emitter -- a diferencia de
// tests/test_arm64_emitter.cpp (que solo comprueba que cada instruccion se
// codifica con los bits correctos), este test genera secuencias completas
// de instrucciones y las EJECUTA de verdad en un CPU ARM64 (emulado por
// qemu-aarch64 en el sandbox de desarrollo, pero el codigo maquina es
// identico al que ejecutaria hardware ARM64 real) -- comprobando que el
// RESULTADO calculado es el esperado, no solo que cada word individual
// coincide con una referencia estatica.
//
// Esto detecta una clase de bug que test_arm64_emitter.cpp no puede: una
// secuencia de instrucciones correctas por separado pero que hace algo
// semanticamente distinto de lo previsto (p.ej. orden de operandos
// invertido en una resta, un registro reutilizado por error).
//
// Se salta (no falla) si el entorno no tiene gcc-aarch64-linux-gnu y
// qemu-user instalados -- ver tests/CMakeLists.txt.
#include "cpu/arm64/emitter.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <fstream>
#include <array>
#include <unistd.h>

using namespace pas::cpu::arm64;

namespace {

#if defined(PAS_ARM64_HARNESS) && defined(PAS_QEMU_AARCH64)

// Escribe 'code' a un fichero temporal, lo ejecuta bajo qemu-aarch64 con
// x0=input, y devuelve el valor impreso (X0 al retornar) como int64_t.
// Aborta el proceso de test (via assert) si algo del pipeline de
// ejecucion falla -- un fallo aqui es un fallo real del emisor, no un
// problema de entorno (la disponibilidad del entorno ya se comprobo antes
// de compilar este test, ver arriba).
int64_t RunOnRealArm64(const std::vector<uint8_t>& code, int64_t input) {
    std::string tmp_path = "/tmp/pas_arm64_dynexec_XXXXXX";
    std::vector<char> tmp_buf(tmp_path.begin(), tmp_path.end());
    tmp_buf.push_back('\0');
    int fd = mkstemp(tmp_buf.data());
    assert(fd >= 0);
    close(fd);
    std::string path(tmp_buf.data());

    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(code.data()),
                   static_cast<std::streamsize>(code.size()));
    }

    std::string cmd = std::string(PAS_QEMU_AARCH64) + " " + PAS_ARM64_HARNESS + " " +
                       path + " " + std::to_string(input);
    FILE* pipe = popen(cmd.c_str(), "r");
    assert(pipe != nullptr);
    char line[256] = {0};
    char* got = std::fgets(line, sizeof(line), pipe);
    int rc = pclose(pipe);
    std::remove(path.c_str());

    assert(got != nullptr);
    assert(rc == 0);
    return std::strtoll(line, nullptr, 10);
}

#define PAS_HAVE_ARM64_HARNESS 1
#else
#define PAS_HAVE_ARM64_HARNESS 0
#endif

} // namespace

static void TestArithmeticSequenceExecutesCorrectly() {
#if PAS_HAVE_ARM64_HARNESS
    // Genera: dado x0 de entrada, calcula (x0 + 5) - 2, luego AND con 0xF,
    // y devuelve el resultado -- ejercita MOVZ/ADD-imm/SUB-imm/AND-reg/RET
    // en una secuencia real, no instrucciones sueltas.
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitAddImm(0, 0, 5, true);   // x0 = x0 + 5
    e.EmitSubImm(0, 0, 2, true);   // x0 = x0 - 2
    e.EmitMovz(1, 0xF, 0, true);   // x1 = 0xF
    e.EmitAndReg(0, 0, 1, true);   // x0 = x0 & x1
    e.EmitRet();
    assert(!e.HadEncodingError());
    assert(!e.Overflowed());
    buf.resize(e.BytesWritten());

    int64_t result = RunOnRealArm64(buf, 10); // (10+5-2)=13, 13 & 0xF = 13
    assert(result == 13);
    std::printf("OK: TestArithmeticSequenceExecutesCorrectly (resultado real de CPU ARM64: %lld)\n",
                (long long)result);
#else
    std::printf("SKIP: TestArithmeticSequenceExecutesCorrectly (sin gcc-aarch64-linux-gnu/qemu-user)\n");
#endif
}

static void TestMultiplyAndDivideExecuteCorrectly() {
#if PAS_HAVE_ARM64_HARNESS
    // x0_out = (x0_in * 6) / 4  -- ejercita MUL y UDIV con un valor real.
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitMovz(1, 6, 0, true);   // x1 = 6
    e.EmitMul(0, 0, 1, true);    // x0 = x0 * 6
    e.EmitMovz(1, 4, 0, true);   // x1 = 4
    e.EmitUdiv(0, 0, 1, true);   // x0 = x0 / 4
    e.EmitRet();
    assert(!e.HadEncodingError());
    buf.resize(e.BytesWritten());

    int64_t result = RunOnRealArm64(buf, 10); // (10*6)/4 = 15
    assert(result == 15);
    std::printf("OK: TestMultiplyAndDivideExecuteCorrectly (resultado real de CPU ARM64: %lld)\n",
                (long long)result);
#else
    std::printf("SKIP: TestMultiplyAndDivideExecuteCorrectly (sin gcc-aarch64-linux-gnu/qemu-user)\n");
#endif
}

static void TestMemoryLoadStoreRoundTrip() {
#if PAS_HAVE_ARM64_HARNESS
    // Guarda x0 en la pila (STP con x29/SP implicito no disponible aqui
    // sin prologo -- se usa un LDR/STR directo sobre una direccion fija en
    // el propio buffer de codigo, reutilizando el puntero de codigo como
    // "memoria de trabajo" para no complicar el arnes con gestion de pila).
    // Ejercita STR/LDR con offset negativo (LDUR/STUR) y positivo (LDR/STR).
    //
    // Estrategia: el arnes pasa x0 como entrada; usamos SP (garantizado
    // valido y con margen por la ABI de Linux) para hacer STR x0,[sp,#-16]!
    // -- pero eso es pre-indexado, que este emisor no soporta todavia. En
    // vez de eso, probamos LDUR/STUR sobre [sp, #-8] (unscaled, offset
    // negativo pequeno, sin escritura previa al puntero de pila) seguido
    // de una lectura -- valida el roundtrip completo store->load real
    // sobre memoria de verdad, no solo la codificacion.
    std::vector<uint8_t> buf(64);
    Emitter e(buf.data(), buf.size());
    e.EmitStrImm(0, 31, -8, 8);  // stur x0, [sp, #-8]  (sp = x31 en contexto de direccionamiento)
    e.EmitMovz(0, 0, 0, true);   // x0 = 0 (para asegurar que lo que se lea despues viene de memoria, no del registro)
    e.EmitLdrImm(0, 31, -8, 8);  // ldur x0, [sp, #-8]
    e.EmitRet();
    assert(!e.HadEncodingError());
    buf.resize(e.BytesWritten());

    int64_t result = RunOnRealArm64(buf, 0x1234);
    assert(result == 0x1234);
    std::printf("OK: TestMemoryLoadStoreRoundTrip (resultado real de CPU ARM64: 0x%llx)\n",
                (long long)result);
#else
    std::printf("SKIP: TestMemoryLoadStoreRoundTrip (sin gcc-aarch64-linux-gnu/qemu-user)\n");
#endif
}

void RunArm64DynamicExecTests() {
    TestArithmeticSequenceExecutesCorrectly();
    TestMultiplyAndDivideExecuteCorrectly();
    TestMemoryLoadStoreRoundTrip();
    std::printf("Todos los tests de arm64_dynamic_exec pasaron (o se saltaron si no habia entorno).\n");
}
