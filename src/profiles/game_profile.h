#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "patch/patch_engine.h"

// GameProfile: ver docs/GAME_PROFILES.md para el esquema JSON completo y su
// justificación. Esta struct es el resultado de parsear ese JSON, no el
// JSON en sí -- game_profile.cpp es lo único que debería saber el formato
// de fichero concreto.
//
// Regla de diseño (sección 11 del encargo original): TODO lo que varía por
// juego vive aquí como dato. Ningún módulo de C++ debe tener un
// `if (game_id == "hotd4")` -- si hace falta una diferencia de
// comportamiento, se añade un campo nuevo a este struct (o a
// PlatformProfile si es compartido entre varios juegos de la misma placa),
// no una rama de código.

namespace pas::profiles {

struct DisplayConfig {
    uint32_t internal_width = 1280;
    uint32_t internal_height = 720;
    std::string aspect = "4:3";
};

struct NetworkConfig {
    std::string mode = "disabled"; // "disabled" | "local_fake_server" (roadmap)
};

struct FilesystemConfig {
    std::string root; // p.ej. "sdmc:/arcade/hotd4/"
};

struct GameProfile {
    std::string id;
    std::string display_name;
    std::string platform_id;      // referencia a un PlatformProfile (ver platform_profile.h)
    std::string executable;
    std::string architecture;     // "x86_32" en el MVP -- ver docs/CPU_TRANSLATION.md
    std::vector<std::string> cpu_features;
    std::string graphics_api;     // "opengl_cg" en el MVP -- ver docs/GRAPHICS.md
    std::vector<std::string> required_apis;

    std::string device_profile;   // ver docs/ARCADE_HARDWARE.md
    std::string shader_bundle;    // ver docs/GRAPHICS.md, precompilacion offline

    std::vector<patch::BytePatch> patches;
    std::vector<patch::SymbolHook> hooks;

    std::string controller_mapping;
    DisplayConfig display;
    NetworkConfig network;
    FilesystemConfig filesystem;

    std::vector<std::string> compatibility_flags; // p.ej. "strict_memory_model", ver docs/MEMORY_MODEL.md
    uint32_t code_cache_size_mb = 64;
    std::vector<std::string> launch_parameters;

    uint32_t revision = 1; // incrementar en cambios que rompan perfiles de usuario existentes
};

class GameProfileLoader {
public:
    // Parsea un fichero JSON con el esquema de docs/GAME_PROFILES.md.
    // NO implementado en este esqueleto (ver game_profile.cpp) -- pendiente
    // de vendorizar una libreria JSON de cabecera unica (ver third_party/README.md).
    bool LoadFromFile(const std::string& path, GameProfile& out);
};

} // namespace pas::profiles
