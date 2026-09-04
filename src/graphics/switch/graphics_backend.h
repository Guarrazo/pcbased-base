#pragma once
#include <cstdint>
#include <cstddef>

// Interfaz que debe implementar el backend real (deko3d/NVN sobre Maxwell
// GM20B -- ver docs/GRAPHICS.md, "Backend de salida: deko3d, no Vulkan").
// gl_shim/ (independiente de plataforma) traduce llamadas GL/Cg contra ESTA
// interfaz, nunca contra deko3d directamente -- así el traductor de estado
// es reutilizable si algún día hay otro backend, y platform/switch/ es el
// único sitio que conoce deko3d de verdad (docs/ARCHITECTURE.md §3).
//
// La implementación real vive en platform/switch/graphics_deko3d.cpp y usa
// tu experiencia ya existente de DLSS-Switch (deko3d sobre Tegra X1/Maxwell
// GM20B) como punto de partida directo.

namespace pas::graphics {

enum class TextureFormat { RGBA8, RGB565, DXT1, DXT5, Unknown };
enum class PrimitiveTopology { Triangles, TriangleStrip, Lines, Points };

struct TextureHandle { uint32_t id = 0; };
struct ShaderHandle { uint32_t id = 0; };   // referencia a un .dksh precompilado
                                              // (ver docs/GRAPHICS.md, riesgo de
                                              // compilacion de shaders)
struct BufferHandle { uint32_t id = 0; };

class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;

    virtual bool Init(uint32_t width, uint32_t height) = 0;
    virtual void Shutdown() = 0;

    virtual TextureHandle CreateTexture(uint32_t width, uint32_t height,
                                         TextureFormat format, const uint8_t* pixels) = 0;
    virtual void DestroyTexture(TextureHandle handle) = 0;

    // Carga un shader YA PRECOMPILADO a .dksh (ver docs/GRAPHICS.md,
    // "Precompilacion por perfil de juego" -- plan A del riesgo de shaders).
    virtual ShaderHandle LoadPrecompiledShader(const uint8_t* dksh_data, size_t size) = 0;

    virtual BufferHandle CreateVertexBuffer(const uint8_t* data, size_t size) = 0;

    virtual void BindTexture(uint32_t slot, TextureHandle handle) = 0;
    virtual void BindShader(ShaderHandle handle) = 0;
    virtual void Draw(PrimitiveTopology topology, BufferHandle vertices, uint32_t vertex_count) = 0;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0; // presenta el frame (swap)
};

} // namespace pas::graphics
