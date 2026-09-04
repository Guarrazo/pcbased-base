#include "os/syscall/libc_shim.h"
#include "core/log.h"

namespace pas::os {

LibcShim& LibcShim::Instance() {
    static LibcShim instance;
    return instance;
}

void LibcShim::Register(const std::string& symbol_name, NativeFn fn) {
    PAS_LOG_INFO("LibcShim", "Registrando simbolo '%s'", symbol_name.c_str());
    functions_[symbol_name] = fn;
}

NativeFn LibcShim::Resolve(const std::string& symbol_name) const {
    auto it = functions_.find(symbol_name);
    return it != functions_.end() ? it->second : nullptr;
}

} // namespace pas::os
