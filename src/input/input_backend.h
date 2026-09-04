#pragma once
#include <cstdint>

// Abstraccion de entrada independiente de plataforma (docs/INPUT_OUTPUT.md).
// La implementacion real vive en platform/switch/input_hid.cpp (HidNpad via
// libnx). VirtualJvsIoBoard (src/arcade/jvs/) consume esta interfaz, nunca
// libnx directamente -- misma separacion core/platform de siempre.

namespace pas::input {

struct RawInputState {
    uint64_t buttons = 0;       // bitmask cruda del backend (HidNpadButton en Switch)
    float stick_left_x = 0.f, stick_left_y = 0.f;
    float stick_right_x = 0.f, stick_right_y = 0.f;
};

class IInputBackend {
public:
    virtual ~IInputBackend() = default;
    virtual void Poll() = 0;
    virtual RawInputState GetState() const = 0;
};

} // namespace pas::input
