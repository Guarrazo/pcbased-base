#pragma once
#include <cstdint>
#include <cstddef>

// JVS virtual (docs/ARCADE_HARDWARE.md): implementa, en software, el
// protocolo que el juego espera de una I/O board JVS real -- sin bus RS485
// físico, porque tanto el "cliente" (juego traducido) como el "servidor"
// (esta clase) corren en el mismo proceso Switch.
//
// El origen real de los datos (botones/sticks del propio Switch) llega vía
// IInputBackend (src/input/input_backend.h), inyectado desde
// platform/switch/. Este fichero no sabe nada de HidNpad directamente.

namespace pas::input { class IInputBackend; }

namespace pas::arcade {

struct JvsButtonState {
    uint32_t buttons = 0;   // bitmask, orden definido por el DeviceProfile
    uint16_t analog_axes[8] = {0}; // 0-1023, escala tipica JVS de 10 bits
    uint8_t coin_count = 0;
};

class VirtualJvsIoBoard {
public:
    explicit VirtualJvsIoBoard(input::IInputBackend& input_backend);

    // Llamado por el shim de "acceso a dispositivo JVS" instalado sobre
    // /dev/ttyS0 (ver os/filesystem/virtual_fs.h) cuando el binario emulado
    // pide el estado de entradas. Aplica el mapeo declarado en
    // GameProfile.controller_mapping (docs/GAME_PROFILES.md).
    JvsButtonState PollState();

    void InsertCoin(uint8_t count = 1);

private:
    input::IInputBackend& input_backend_;
    uint8_t pending_coins_ = 0;
};

} // namespace pas::arcade
