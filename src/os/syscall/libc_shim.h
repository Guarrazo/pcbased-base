#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// libc-shim minima para binarios ELF Lindbergh (docs/WINDOWS_COMPATIBILITY.md:
// esto sustituye a Win32 en el MVP porque el target no usa Windows).
//
// Principio de diseño: NO se implementa "toda la libc" de antemano. Cada
// simbolo que el ElfLoader reporte como no resuelto (ver
// os/elf_loader/elf_loader.h, UnresolvedSymbol) se añade aqui uno a uno,
// contra el binario real del titulo elegido -- igual que en Super3-NX el
// subconjunto de PowerPC se determino por lo que el binario real usaba, no
// por el ISA completo.

namespace pas::os {

using NativeFn = void (*)(); // firma real depende de la convencion de
                              // llamada x86 emulada; el thunk de cada
                              // funcion se encarga de leer argumentos del
                              // estado de registros/pila emulado.

class LibcShim {
public:
    static LibcShim& Instance();

    // Registra una implementacion para un simbolo (p.ej. "malloc", "free",
    // "gettimeofday"). Los modulos de compatibilidad (docs/GAME_PROFILES.md,
    // "required_apis") llaman a esto durante su Init().
    void Register(const std::string& symbol_name, NativeFn fn);

    NativeFn Resolve(const std::string& symbol_name) const;

private:
    std::unordered_map<std::string, NativeFn> functions_;
};

} // namespace pas::os
