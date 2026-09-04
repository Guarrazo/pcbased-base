#pragma once
#include "graphics/switch/graphics_backend.h"
#include <cstdint>

// Traductor de estado OpenGL 1.x/2.x de funcion fija + NVIDIA Cg -> backend
// generico (docs/GRAPHICS.md). Este es el modulo que se registra en
// ModuleRegistry (core/module.h) como "gl_1_x" / "cg_3_1" segun declare el
// GameProfile activo (docs/GAME_PROFILES.md, campo "required_apis").
//
// Intercepta por NOMBRE DE SIMBOLO durante la resolucion de imports del ELF
// (os/elf_loader/), igual que hace el "thunking" de FEX para librerias
// nativas -- pero aqui el destino nunca es "la libreria nativa del host"
// (no existe GL nativo en Horizon, ver docs/STATE_OF_THE_ART.md), sino
// nuestro propio traductor.

namespace pas::graphics {

class GlShim {
public:
    explicit GlShim(IGraphicsBackend& backend);

    // Registra en LibcShim (os/syscall/libc_shim.h) los thunks gl*/cg* que
    // este modulo sabe traducir. Se llama desde Init() del
    // ICompatibilityModule correspondiente.
    void RegisterSymbols();

private:
    IGraphicsBackend& backend_;

    // Estado de pipeline fijo replicado aqui (matrices, textura activa,
    // modo de blending...) porque OpenGL de funcion fija es implicito -- el
    // juego no pasa este estado explicitamente en cada llamada, hay que
    // llevarlo nosotros para poder construir las llamadas equivalentes a
    // deko3d en el momento del Draw. No implementado en este esqueleto.
};

} // namespace pas::graphics
