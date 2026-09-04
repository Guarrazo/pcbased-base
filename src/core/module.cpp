#include "core/module.h"
#include "core/log.h"
#include <algorithm>

namespace pas::core {

ModuleRegistry& ModuleRegistry::Instance() {
    static ModuleRegistry instance;
    return instance;
}

void ModuleRegistry::Register(ICompatibilityModule* module) {
    if (!module) return;
    PAS_LOG_INFO("ModuleRegistry", "Registrando modulo '%s'", module->Name());
    modules_.push_back(module);
}

ICompatibilityModule* ModuleRegistry::Find(const std::string& name) const {
    auto it = std::find_if(modules_.begin(), modules_.end(),
        [&](ICompatibilityModule* m) { return name == m->Name(); });
    return it != modules_.end() ? *it : nullptr;
}

} // namespace pas::core
