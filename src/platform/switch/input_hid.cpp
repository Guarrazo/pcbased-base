#include "platform/switch/input_hid.h"
#include "core/log.h"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace pas::platform::switch_ {

SwitchHidInputBackend::SwitchHidInputBackend() {
#ifdef __SWITCH__
    // TODO(Nivel 2, docs/ROADMAP.md): padConfigureInput / padInitializeDefault
    // segun la version de libnx instalada -- no implementado en este esqueleto.
    PAS_LOG_WARN("SwitchHidInputBackend", "Inicializacion de HidNpad no implementada todavia");
#endif
}

void SwitchHidInputBackend::Poll() {
    // TODO: padUpdate() + lectura de estado real. No implementado.
}

input::RawInputState SwitchHidInputBackend::GetState() const {
    return state_; // vacio hasta que Poll() este implementado
}

} // namespace pas::platform::switch_
