#pragma once
#include "cpu/translator/ir.h"
#include <cstdint>
#include <string>
#include <vector>

// Motor de patches (docs/PATCHING.md): datos declarados en GameProfile,
// aplicados como transformacion sobre IR -- nunca sobre bytes ARM64 finales
// ni sobre bytes x86 crudos antes de decodificar.

namespace pas::patch {

struct BytePatch {
    uint32_t offset = 0;
    std::vector<uint8_t> expect;   // bytes originales esperados -- obligatorio
    std::vector<uint8_t> replace;
};

struct SymbolHook {
    std::string symbol;
    enum class Action { StubReturnOk, StubReturnFail, CallCustom } action;
};

class PatchEngine {
public:
    // Cargados desde el GameProfile activo (docs/GAME_PROFILES.md) al
    // arrancar un titulo -- no hay patches hardcodeados en C++.
    void LoadBytePatches(const std::vector<BytePatch>& patches);
    void LoadSymbolHooks(const std::vector<SymbolHook>& hooks);

    // Aplica byte patches sobre la imagen ELF cargada, ANTES de decodificar
    // (os/elf_loader/ llama a esto tras el parseo, antes de pasar el
    // binario al Jit). Verifica 'expect' contra los bytes reales -- si no
    // coincide, rechaza el patch y lo registra, no lo aplica a ciegas
    // (docs/PATCHING.md).
    bool ApplyBytePatches(uint8_t* guest_memory, size_t guest_memory_size);

    // Consultado por IrBuilder::ApplyPatches() (src/cpu/translator/ir_builder.h)
    // por cada bloque construido.
    bool HasHookForSymbol(const std::string& symbol) const;

private:
    std::vector<BytePatch> byte_patches_;
    std::vector<SymbolHook> symbol_hooks_;
};

} // namespace pas::patch
