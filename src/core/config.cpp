#include "core/config.h"
#include "core/log.h"
#include <fstream>
#include <sstream>

namespace pas::core {

Config& Config::Instance() {
    static Config instance;
    return instance;
}

bool Config::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        PAS_LOG_WARN("Config", "No se pudo abrir %s, usando valores por defecto", path.c_str());
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        values_[key] = value;
    }
    return true;
}

std::string Config::GetString(const std::string& key, const std::string& fallback) const {
    auto it = values_.find(key);
    return it != values_.end() ? it->second : fallback;
}

int Config::GetInt(const std::string& key, int fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    try { return std::stoi(it->second); } catch (...) { return fallback; }
}

bool Config::GetBool(const std::string& key, bool fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    return it->second == "1" || it->second == "true";
}

} // namespace pas::core
