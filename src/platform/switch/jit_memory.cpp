#include "platform/switch/jit_memory.h"
#include "core/log.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

// Ver la nota de honestidad en jit_memory.h: API jit* de libnx tal y como
// se documenta publicamente (jitCreate/jitTransitionToWritable/
// jitTransitionToExecutable/jitGetRwAddr/jitGetRxAddr/jitClose) -- mismo
// patron ya validado en Super3-NX (docs/JIT.md). No verificado contra una
// compilacion real en este entorno.

namespace pas::platform::switch_ {

#ifdef __SWITCH__
struct JitImpl { Jit jit; };
#else
struct JitImpl {}; // build de host (tests/) sin libnx -- no funcional, solo
                    // para que el codigo independiente de plataforma pueda
                    // compilarse/testearse fuera de Switch si hace falta.
#endif

SwitchExecutableMemory::SwitchExecutableMemory(size_t size) : capacity_(size) {
    impl_ = new JitImpl();
#ifdef __SWITCH__
    auto* impl = static_cast<JitImpl*>(impl_);
    Result rc = jitCreate(&impl->jit, size);
    if (R_FAILED(rc)) {
        PAS_LOG_ERROR("SwitchExecutableMemory", "jitCreate fallo (rc=0x%x, size=%zu)", rc, size);
        valid_ = false;
        return;
    }
    valid_ = true;
#else
    PAS_LOG_ERROR("SwitchExecutableMemory", "Build de host: jitCreate no disponible");
    valid_ = false;
#endif
}

SwitchExecutableMemory::~SwitchExecutableMemory() {
#ifdef __SWITCH__
    if (valid_) {
        jitClose(&static_cast<JitImpl*>(impl_)->jit);
    }
#endif
    delete static_cast<JitImpl*>(impl_);
}

uint8_t* SwitchExecutableMemory::BeginWrite() {
#ifdef __SWITCH__
    if (!valid_) return nullptr;
    auto* impl = static_cast<JitImpl*>(impl_);
    Result rc = jitTransitionToWritable(&impl->jit);
    if (R_FAILED(rc)) {
        PAS_LOG_ERROR("SwitchExecutableMemory", "jitTransitionToWritable fallo (rc=0x%x)", rc);
        return nullptr;
    }
    return static_cast<uint8_t*>(jitGetRwAddr(&impl->jit));
#else
    return nullptr;
#endif
}

void SwitchExecutableMemory::EndWrite() {
#ifdef __SWITCH__
    if (!valid_) return;
    auto* impl = static_cast<JitImpl*>(impl_);
    Result rc = jitTransitionToExecutable(&impl->jit);
    if (R_FAILED(rc)) {
        PAS_LOG_ERROR("SwitchExecutableMemory", "jitTransitionToExecutable fallo (rc=0x%x)", rc);
    }
#endif
}

uint8_t* SwitchExecutableMemory::ExecutableBase() {
#ifdef __SWITCH__
    if (!valid_) return nullptr;
    return static_cast<uint8_t*>(jitGetRxAddr(&static_cast<JitImpl*>(impl_)->jit));
#else
    return nullptr;
#endif
}

} // namespace pas::platform::switch_
