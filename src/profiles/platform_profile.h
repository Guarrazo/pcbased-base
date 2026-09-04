#pragma once
#include <string>
#include <vector>

// PlatformProfile: lo comun a TODOS los juegos de una misma placa arcade
// (docs/GAME_PROFILES.md). Para el MVP solo existe "lindbergh" -- ver
// profiles/lindbergh.platform.json en la raiz del repo.

namespace pas::profiles {

struct PlatformProfile {
    std::string id;               // "lindbergh"
    std::string cpu_architecture; // "x86_32"
    std::vector<std::string> default_cpu_features;
    std::string default_graphics_api;
    std::vector<std::string> default_required_apis; // modulos que TODOS los
                                                       // juegos de esta placa
                                                       // necesitan (libc_shim,
                                                       // cg_3_1, gl_1_x)
    std::vector<std::string> device_paths_to_intercept; // /dev/lbb, /dev/i2c/0, ...
};

class PlatformProfileLoader {
public:
    bool LoadFromFile(const std::string& path, PlatformProfile& out);
};

} // namespace pas::profiles
