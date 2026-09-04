#pragma once
#include <string>
#include <unordered_map>

// Configuracion global de la aplicacion (no confundir con GameProfile, que es
// por-juego: ver src/profiles/game_profile.h). Esto cubre opciones de runtime
// como nivel de log, ruta base de perfiles, cap de reloj, etc.
// Ver docs/GAME_PROFILES.md para la diferencia entre esto y un GameProfile.

namespace pas::core {

class Config {
public:
    static Config& Instance();

    // Carga config.ini desde la ruta dada (formato clave=valor simple).
    bool LoadFromFile(const std::string& path);

    std::string GetString(const std::string& key, const std::string& fallback) const;
    int GetInt(const std::string& key, int fallback) const;
    bool GetBool(const std::string& key, bool fallback) const;

private:
    std::unordered_map<std::string, std::string> values_;
};

} // namespace pas::core
