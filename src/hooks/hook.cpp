#include "hooks/hook.h"
#include "core/log.h"

namespace pas::hooks {

HookRegistry& HookRegistry::Instance() {
    static HookRegistry instance;
    return instance;
}

void HookRegistry::Register(const std::string& symbol, HookFn fn) {
    PAS_LOG_INFO("HookRegistry", "Registrando hook para '%s'", symbol.c_str());
    hooks_[symbol] = std::move(fn);
}

bool HookRegistry::Has(const std::string& symbol) const {
    return hooks_.find(symbol) != hooks_.end();
}

void HookRegistry::Invoke(const std::string& symbol) const {
    auto it = hooks_.find(symbol);
    if (it == hooks_.end()) {
        PAS_LOG_ERROR("HookRegistry", "Invoke sobre hook inexistente: '%s'", symbol.c_str());
        return;
    }
    it->second();
}

} // namespace pas::hooks
