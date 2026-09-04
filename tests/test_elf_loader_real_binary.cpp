// Test de host para os::ElfLoader contra un ELF32 x86 REAL, generado por un
// compilador de verdad (ver tests/fixtures/real_elf32_sample.c y el
// CMakeLists.txt de este directorio) -- complementa a
// test_elf_loader.cpp, que usa un ELF construido a mano. Aquí se valida
// que el loader funciona con las convenciones reales de un ELF32 dinámico
// producido por GCC/glibc (PLT/GOT reales, tipos de reubicación
// R_386_JMP_SLOT/R_386_GLOB_DAT tal y como los emite el enlazador de
// verdad), no solo con lo que yo mismo construí a mano para el otro test.
//
// Este test se SALTA (no falla) si el entorno no tiene soporte multilib de
// 32 bits -- ver PAS_REAL_ELF32_SAMPLE en tests/CMakeLists.txt.
#include "os/elf_loader/elf_loader.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include <algorithm>

void RunElfLoaderRealBinaryTests() {
#ifdef PAS_REAL_ELF32_SAMPLE
    const std::string path = PAS_REAL_ELF32_SAMPLE;
    if (path.empty()) {
        std::printf("SKIP: RunElfLoaderRealBinaryTests (sin soporte multilib de 32 bits "
                    "en este host -- ver tests/CMakeLists.txt)\n");
        return;
    }

    std::ifstream file(path, std::ios::binary);
    assert(file.is_open());
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    assert(!data.empty());

    pas::os::ElfLoader loader;
    auto result = loader.Load(data.data(), data.size());
    assert(result.has_value());

    // No fijamos el valor exacto de base_address/entry_point (dependen de
    // la version de gcc/glibc del host que compilo el fixture) -- lo que
    // se valida es que el parseo y las reubicaciones de un ELF32 dinamico
    // REAL no rompen el loader y que los simbolos externos esperables
    // aparecen como no resueltos, exactamente como deben (nadie los ha
    // registrado todavia en LibcShim).
    assert(result->guest_memory.size() > 0);
    assert(result->entry_point >= result->base_address);

    auto has_unresolved = [&](const char* name) {
        return std::any_of(result->unresolved_symbols.begin(), result->unresolved_symbols.end(),
                            [&](const pas::os::UnresolvedSymbol& s) { return s.name == name; });
    };
    assert(has_unresolved("malloc"));
    assert(has_unresolved("free"));
    assert(has_unresolved("printf"));

    std::printf("OK: RunElfLoaderRealBinaryTests (base=0x%08x, entry=0x%08x, %zu bytes, "
                "%zu simbolos sin resolver)\n",
                result->base_address, result->entry_point, result->guest_memory.size(),
                result->unresolved_symbols.size());
#else
    std::printf("SKIP: RunElfLoaderRealBinaryTests (PAS_REAL_ELF32_SAMPLE no definido)\n");
#endif
}
