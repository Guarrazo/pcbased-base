#pragma once
#include "graphics/switch/graphics_backend.h"

// Implementacion real de IGraphicsBackend via deko3d (docs/GRAPHICS.md).
// Punto de partida directo: tu propio proyecto DLSS-Switch ya inicializa
// deko3d sobre este mismo hardware (Tegra X1/Maxwell GM20B) -- la
// inicializacion de device/swapchain/queue de ahi es reutilizable casi tal
// cual para el Init() de aqui.

namespace pas::platform::switch_ {

class Deko3dGraphicsBackend : public graphics::IGraphicsBackend {
public:
    bool Init(uint32_t width, uint32_t height) override;
    void Shutdown() override;

    graphics::TextureHandle CreateTexture(uint32_t width, uint32_t height,
                                           graphics::TextureFormat format,
                                           const uint8_t* pixels) override;
    void DestroyTexture(graphics::TextureHandle handle) override;

    graphics::ShaderHandle LoadPrecompiledShader(const uint8_t* dksh_data, size_t size) override;
    graphics::BufferHandle CreateVertexBuffer(const uint8_t* data, size_t size) override;

    void BindTexture(uint32_t slot, graphics::TextureHandle handle) override;
    void BindShader(graphics::ShaderHandle handle) override;
    void Draw(graphics::PrimitiveTopology topology, graphics::BufferHandle vertices,
              uint32_t vertex_count) override;

    void BeginFrame() override;
    void EndFrame() override;
};

} // namespace pas::platform::switch_
