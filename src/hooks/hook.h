#pragma once
#include <string>
#include <functional>
#include <unordered_map>

// Infraestructura de hooks por nombre de simbolo (docs/PATCHING.md,
// "Function/API hooks"). Distinto de patch::PatchEngine: esto es el
// MECANISMO generico de registrar "cuando se resuelva el simbolo X, usa
// esta implementacion nativa en vez de fallar como no-resuelto"; el
// PatchEngine decide, a partir del GameProfile, QUE hooks activar para un
// juego concreto.

namespace pas::hooks {

using HookFn = std::function<void()>; // ver os/syscall/libc_shim.h para la
                                        // discusion de firma real vs thunk

class HookRegistry {
public:
    static HookRegistry& Instance();

    void Register(const std::string& symbol, HookFn fn);
    bool Has(const std::string& symbol) const;
    void Invoke(const std::string& symbol) const;

private:
    std::unordered_map<std::string, HookFn> hooks_;
};

} // namespace pas::hooks
