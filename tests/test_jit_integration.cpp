// Test de integracion para cpu::jit::Jit -- a diferencia de
// tests/test_ir_end_to_end.cpp (que usa Decoder/IrBuilder/Arm64CodeGen
// sueltos), este test pasa por la clase Jit COMPLETA (RunFrom -> 
// TranslateBlock interno -> CodeCache), con un IExecutableMemory falso
// (sin jitCreate real, igual que tests/test_code_cache.cpp).
#include "cpu/jit/jit.h"
#include "cpu/arm64/emitter.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <memory>
#include <cstdlib>

#if defined(PAS_ARM64_HARNESS) && defined(PAS_QEMU_AARCH64)
#include <fstream>
#include <unistd.h>
#define PAS_HAVE_JIT_DYNEXEC 1
#endif

namespace {

class FakeExecutableMemory : public pas::cpu::jit::IExecutableMemory {
public:
    explicit FakeExecutableMemory(size_t size) : buffer_(size, 0) {}
    uint8_t* BeginWrite() override { return buffer_.data(); }
    void EndWrite() override {}
    uint8_t* ExecutableBase() override { return buffer_.data(); }
    size_t Capacity() const override { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
};

} // namespace

static void TestRunFromTranslatesAndCachesBlock() {
    // add eax,0x3 ; and eax,0xf ; ret -- mismo fixture que
    // tests/test_ir_end_to_end.cpp (ver ese fichero para procedencia).
    uint8_t guest_memory[] = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f, 0xc3};

    auto mem = std::make_unique<FakeExecutableMemory>(4096);
    pas::cpu::jit::Jit jit(std::move(mem));

    assert(jit.FindTranslatedBlock(0) == nullptr); // nada traducido aun

    jit.RunFrom(0, guest_memory, sizeof(guest_memory));

    const auto* block = jit.FindTranslatedBlock(0);
    assert(block != nullptr);
    assert(block->guest_address == 0);
    assert(block->code_size > 0);
    assert(block->code_size % 4 == 0); // instrucciones ARM64 de 4 bytes

    uint8_t* code = jit.GetExecutableCode(*block);
    assert(code != nullptr);

    std::printf("OK: TestRunFromTranslatesAndCachesBlock (%zu bytes ARM64 generados)\n",
                block->code_size);

#ifdef PAS_HAVE_JIT_DYNEXEC
    // Validacion adicional: el codigo que genero Jit::RunFrom() a traves
    // de TODA la clase (no las piezas sueltas) se ejecuta de verdad y
    // calcula lo correcto -- mismo adaptador de convencion de llamada que
    // tests/test_ir_end_to_end.cpp (EAX vive en W20, hay que copiar
    // entrada/salida a/desde W0 para el harness). El adaptador se
    // construye con el propio Emitter (NO bytes escritos a mano -- ver
    // docs/JIT.md sobre por que: escribir hex a mano es exactamente la
    // clase de error que este proyecto intenta evitar en todo lo demas).
    //
    // El bloque generado por Jit ya termina en su propio RET (viene del
    // RET x86 traducido) -- la copia de salida a W0 tiene que ir ANTES de
    // ese RET, asi que se generan tres tramos por separado con el mismo
    // Emitter (prologo, bloque menos su ultima instruccion, epilogo+RET)
    // en vez de intentar insertar bytes en medio de un buffer ya emitido.
    assert(block->code_size >= 4); // al menos el RET final
    size_t body_size = block->code_size - 4; // todo menos el RET traducido

    uint8_t wrapper_buf[512];
    pas::cpu::arm64::Emitter wrapper(wrapper_buf, sizeof(wrapper_buf));
    wrapper.EmitOrrReg(/*rd=*/20, /*rn=*/31, /*rm=*/0, /*is64=*/false); // mov w20, w0
    assert(!wrapper.HadEncodingError());
    size_t prologue_size = wrapper.BytesWritten();

    std::vector<uint8_t> final_code;
    final_code.insert(final_code.end(), wrapper_buf, wrapper_buf + prologue_size);
    final_code.insert(final_code.end(), code, code + body_size); // bloque sin su RET

    uint8_t epilogue_buf[512];
    pas::cpu::arm64::Emitter epilogue(epilogue_buf, sizeof(epilogue_buf));
    epilogue.EmitOrrReg(/*rd=*/0, /*rn=*/31, /*rm=*/20, /*is64=*/false); // mov w0, w20
    epilogue.EmitRet();
    assert(!epilogue.HadEncodingError());
    final_code.insert(final_code.end(), epilogue_buf, epilogue_buf + epilogue.BytesWritten());

    std::string tmpl = "/tmp/pas_jit_dynexec_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemp(buf.data());
    assert(fd >= 0);
    close(fd);
    std::string path(buf.data());
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(final_code.data()),
                   static_cast<std::streamsize>(final_code.size()));
    }

    std::string cmd = std::string(PAS_QEMU_AARCH64) + " " + PAS_ARM64_HARNESS + " " + path + " 10";
    FILE* pipe = popen(cmd.c_str(), "r");
    assert(pipe != nullptr);
    char line[256] = {0};
    char* got = std::fgets(line, sizeof(line), pipe);
    int rc = pclose(pipe);
    std::remove(path.c_str());
    assert(got != nullptr);
    assert(rc == 0);
    int result = std::atoi(line);
    assert(result == 13); // (10+3)&0xF = 13, mismo calculo que test_ir_end_to_end.cpp

    std::printf("OK: TestRunFromTranslatesAndCachesBlock -- ejecucion real via Jit "
                "completo: %d (esperado 13)\n", result);
#else
    std::printf("SKIP: verificacion de ejecucion dinamica (faltan gcc-aarch64-linux-gnu/qemu-user)\n");
#endif
}

static void TestRunFromReusesTranslatedBlock() {
    uint8_t guest_memory[] = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f, 0xc3};
    auto mem = std::make_unique<FakeExecutableMemory>(4096);
    pas::cpu::jit::Jit jit(std::move(mem));

    jit.RunFrom(0, guest_memory, sizeof(guest_memory));
    const auto* first = jit.FindTranslatedBlock(0);
    assert(first != nullptr);
    size_t offset_before = first->code_offset;

    jit.RunFrom(0, guest_memory, sizeof(guest_memory)); // segunda vez: debe reusar, no retraducir
    const auto* second = jit.FindTranslatedBlock(0);
    assert(second != nullptr);
    assert(second->code_offset == offset_before); // mismo bloque, no uno nuevo

    std::printf("OK: TestRunFromReusesTranslatedBlock\n");
}

void RunJitIntegrationTests() {
    TestRunFromTranslatesAndCachesBlock();
    TestRunFromReusesTranslatedBlock();
    std::printf("Todos los tests de jit (integracion) pasaron.\n");
}
