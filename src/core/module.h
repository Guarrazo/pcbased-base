#pragma once
#include <string>
#include <vector>

// Registro generico de "CompatibilityModule" (seccion 11 del encargo original):
// cada pieza de compatibilidad (libc-shim, cg_3_1, gl_1_x, win32_min en el
// roadmap...) se registra aqui por nombre. Un GameProfile solo referencia
// nombres de modulo, nunca instancia logica de juego directamente.
// Ver docs/GAME_PROFILES.md.

namespace pas::core {

class ICompatibilityModule {
public:
    virtual ~ICompatibilityModule() = default;
    virtual const char* Name() const = 0;
    // Se llama una vez, tras resolver que el GameProfile activo lo necesita.
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
};

class ModuleRegistry {
public:
    static ModuleRegistry& Instance();

    void Register(ICompatibilityModule* module);
    ICompatibilityModule* Find(const std::string& name) const;
    const std::vector<ICompatibilityModule*>& All() const { return modules_; }

private:
    std::vector<ICompatibilityModule*> modules_;
};

} // namespace pas::core
