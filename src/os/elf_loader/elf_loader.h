#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <optional>

// Cargador de ejecutables ELF x86 de 32 bits (target Lindbergh -- ver
// docs/WINDOWS_COMPATIBILITY.md: NO hay cargador PE en el MVP, eso es
// roadmap posterior para sistemas Windows).
//
// Responsabilidades:
//   - Parsear cabecera ELF32, program headers (PT_LOAD) y section headers.
//   - Reservar el espacio de direcciones "de invitado" (guest) donde vive
//     el binario x86 (este loader usa un std::vector<uint8_t> normal; la
//     memoria del PROCESO emulado NO tiene por que ser memoria ejecutable
//     -- eso es responsabilidad exclusiva de cpu/jit/code_cache.h, ver
//     docs/JIT.md).
//   - Parsear la tabla de simbolos dinamicos (.dynsym/.dynstr) y aplicar
//     las reubicaciones (.rel.dyn/.rel.plt) segun el subconjunto de tipos
//     R_386_* realmente usado por binarios de esta epoca (GCC de
//     mediados-2000, sin PIE -- ver docs/CPU_TRANSLATION.md).
//   - Resolver simbolos importados NO contra .so reales sino contra la
//     tabla de sistemas registrada (libc-shim, Cg-shim, GL-shim -- ver
//     src/os/syscall/libc_shim.h) segun lo que declare el PlatformProfile
//     activo (docs/GAME_PROFILES.md). Lo que no se resuelva se reporta en
//     unresolved_symbols, NUNCA se ignora en silencio.

namespace pas::os {

struct UnresolvedSymbol {
    std::string name;
    uint32_t got_or_plt_address = 0; // direccion (en el espacio guest) que
                                       // hay que rellenar si el simbolo se
                                       // resuelve mas tarde
};

struct LoadedElf {
    uint32_t entry_point = 0;
    std::vector<uint8_t> guest_memory; // buffer contiguo que representa el
                                        // espacio de direcciones del
                                        // proceso emulado, indexado por
                                        // (direccion_virtual - base_address)
    uint32_t base_address = 0;         // direccion virtual del primer byte
                                        // de guest_memory (el vaddr minimo
                                        // entre los PT_LOAD)

    std::vector<UnresolvedSymbol> unresolved_symbols;

    // Traduce una direccion virtual x86 a un puntero dentro de guest_memory,
    // o nullptr si esta fuera de rango. Usado por cpu::jit::Jit (ver
    // docs/JIT.md) para leer los bytes x86 a decodificar.
    uint8_t* AddressToPointer(uint32_t virtual_address);
};

class ElfLoader {
public:
    // Firma de "resolver": dado un nombre de simbolo, devuelve la direccion
    // (en el espacio guest, normalmente un stub sintetico, no una direccion
    // real x86) a usar para R_386_JMP_SLOT/R_386_GLOB_DAT, o nullopt si
    // nadie lo resuelve (queda en unresolved_symbols). Inyectado por quien
    // llama a Load(), para no acoplar este loader a LibcShim directamente
    // (misma separacion que en el resto del proyecto, docs/ARCHITECTURE.md).
    using SymbolResolver = std::optional<uint32_t> (*)(const std::string& name, void* user_data);

    // 'image' es el contenido completo del fichero ELF (leido previamente
    // desde la raiz de filesystem declarada en el GameProfile, ver
    // docs/GAME_PROFILES.md, campo "filesystem.root").
    std::optional<LoadedElf> Load(const uint8_t* image, size_t image_size,
                                   SymbolResolver resolver = nullptr, void* resolver_data = nullptr);
};

} // namespace pas::os
