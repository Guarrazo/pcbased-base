#include "patch/patch_engine.h"
#include "core/log.h"
#include <algorithm>
#include <cstring>

namespace pas::patch {

void PatchEngine::LoadBytePatches(const std::vector<BytePatch>& patches) {
    byte_patches_ = patches;
    PAS_LOG_INFO("PatchEngine", "Cargados %zu byte patches", byte_patches_.size());
}

void PatchEngine::LoadSymbolHooks(const std::vector<SymbolHook>& hooks) {
    symbol_hooks_ = hooks;
    PAS_LOG_INFO("PatchEngine", "Cargados %zu symbol hooks", symbol_hooks_.size());
}

bool PatchEngine::ApplyBytePatches(uint8_t* guest_memory, size_t guest_memory_size) {
    bool all_ok = true;
    for (const auto& p : byte_patches_) {
        if (p.offset + p.expect.size() > guest_memory_size) {
            PAS_LOG_ERROR("PatchEngine", "Patch fuera de rango en offset 0x%x", p.offset);
            all_ok = false;
            continue;
        }
        if (std::memcmp(guest_memory + p.offset, p.expect.data(), p.expect.size()) != 0) {
            PAS_LOG_ERROR("PatchEngine",
                          "Patch rechazado en offset 0x%x: bytes originales no coinciden "
                          "(binario distinto al esperado?)", p.offset);
            all_ok = false;
            continue;
        }
        std::memcpy(guest_memory + p.offset, p.replace.data(), p.replace.size());
        PAS_LOG_INFO("PatchEngine", "Patch aplicado en offset 0x%x", p.offset);
    }
    return all_ok;
}

bool PatchEngine::HasHookForSymbol(const std::string& symbol) const {
    return std::any_of(symbol_hooks_.begin(), symbol_hooks_.end(),
        [&](const SymbolHook& h) { return h.symbol == symbol; });
}

} // namespace pas::patch
