#include "platform/switch/graphics_deko3d.h"
#include "core/log.h"

// Ninguna llamada real a deko3d en este esqueleto todavia -- ver
// docs/GRAPHICS.md y docs/ROADMAP.md, Nivel 1. Se deja como TODO explicito
// en cada metodo, reutilizando la inicializacion ya validada en
// DLSS-Switch como punto de partida cuando se implemente.

namespace pas::platform::switch_ {

bool Deko3dGraphicsBackend::Init(uint32_t width, uint32_t height) {
    PAS_LOG_WARN("Deko3dGraphicsBackend", "Init(%ux%u) no implementado todavia -- "
                                          "reutilizar inicializacion de DLSS-Switch",
                 width, height);
    return false;
}

void Deko3dGraphicsBackend::Shutdown() {}

graphics::TextureHandle Deko3dGraphicsBackend::CreateTexture(
    uint32_t, uint32_t, graphics::TextureFormat, const uint8_t*) {
    PAS_LOG_WARN("Deko3dGraphicsBackend", "CreateTexture no implementado todavia");
    return {};
}

void Deko3dGraphicsBackend::DestroyTexture(graphics::TextureHandle) {}

graphics::ShaderHandle Deko3dGraphicsBackend::LoadPrecompiledShader(const uint8_t*, size_t) {
    PAS_LOG_WARN("Deko3dGraphicsBackend", "LoadPrecompiledShader no implementado todavia -- "
                                          "ver docs/GRAPHICS.md, riesgo de compilacion de shaders");
    return {};
}

graphics::BufferHandle Deko3dGraphicsBackend::CreateVertexBuffer(const uint8_t*, size_t) {
    PAS_LOG_WARN("Deko3dGraphicsBackend", "CreateVertexBuffer no implementado todavia");
    return {};
}

void Deko3dGraphicsBackend::BindTexture(uint32_t, graphics::TextureHandle) {}
void Deko3dGraphicsBackend::BindShader(graphics::ShaderHandle) {}
void Deko3dGraphicsBackend::Draw(graphics::PrimitiveTopology, graphics::BufferHandle, uint32_t) {}
void Deko3dGraphicsBackend::BeginFrame() {}
void Deko3dGraphicsBackend::EndFrame() {}

} // namespace pas::platform::switch_
