#pragma once
#include "cpu/jit/code_cache.h"
#include <cstddef>

// UNICA implementacion de IExecutableMemory de todo el proyecto -- y el
// UNICO fichero que debe llamar a jitCreate/jitTransitionToWritable/
// jitTransitionToExecutable directamente (docs/SWITCH_PLATFORM.md,
// docs/JIT.md: patron ya validado en Super3-NX, incluye el canario
// BRK #0xDEAD que usa arm64::Emitter para detectar overflow).
//
// NOTA DE HONESTIDAD: este header/su .cpp estan escritos para compilar
// contra libnx (Jit* de <switch.h>), pero no se ha podido verificar la
// compilacion contra una instalacion real de devkitPro en el entorno donde
// se genero este esqueleto (sin acceso de red a apt.devkitpro.org). Revisa
// los nombres exactos de la API jit* contra tu SDK instalado antes de
// asumir que compila sin ajustes.

namespace pas::platform::switch_ {

class SwitchExecutableMemory : public cpu::jit::IExecutableMemory {
public:
    // size debe ser el tamano dimensionado por GameProfile.code_cache_size_mb
    // (docs/GAME_PROFILES.md) -- no crece dinamicamente, ver docs/SWITCH_PLATFORM.md,
    // tabla de limites practicos.
    explicit SwitchExecutableMemory(size_t size);
    ~SwitchExecutableMemory() override;

    uint8_t* BeginWrite() override;
    void EndWrite() override;
    uint8_t* ExecutableBase() override;
    size_t Capacity() const override { return capacity_; }

private:
    size_t capacity_;
    bool valid_ = false;
    // El handle real de libnx (Jit jit_) se declara en el .cpp para no
    // arrastrar <switch.h> a este header (que cpu/jit/code_cache.h -- y por
    // tanto todo el modulo cpu, independiente de plataforma -- NO debe
    // incluir jamas).
    void* impl_ = nullptr;
};

} // namespace pas::platform::switch_
