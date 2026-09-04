#pragma once
#include "input/input_backend.h"

// Implementacion real de IInputBackend via HidNpad de libnx (docs/INPUT_OUTPUT.md).
// Unico fichero (junto con jit_memory.cpp y graphics_deko3d.cpp) que debe
// tocar APIs de libnx directamente para su area respectiva.

namespace pas::platform::switch_ {

class SwitchHidInputBackend : public input::IInputBackend {
public:
    SwitchHidInputBackend();
    void Poll() override;
    input::RawInputState GetState() const override;

private:
    input::RawInputState state_;
};

} // namespace pas::platform::switch_
