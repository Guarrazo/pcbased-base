#include "arcade/jvs/virtual_jvs.h"
#include "input/input_backend.h"
#include "core/log.h"

namespace pas::arcade {

VirtualJvsIoBoard::VirtualJvsIoBoard(input::IInputBackend& input_backend)
    : input_backend_(input_backend) {}

JvsButtonState VirtualJvsIoBoard::PollState() {
    // TODO(Nivel 2, docs/ROADMAP.md): leer input_backend_.Poll() y aplicar
    // el mapeo del GameProfile activo. No implementado en este esqueleto.
    JvsButtonState state;
    state.coin_count = pending_coins_;
    pending_coins_ = 0;
    return state;
}

void VirtualJvsIoBoard::InsertCoin(uint8_t count) {
    pending_coins_ += count;
    PAS_LOG_INFO("VirtualJvsIoBoard", "Moneda insertada (total pendiente: %u)", pending_coins_);
}

} // namespace pas::arcade
