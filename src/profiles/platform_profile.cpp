#include "profiles/platform_profile.h"
#include "profiles/json_lite.h"
#include "core/log.h"
#include <fstream>
#include <sstream>

namespace pas::profiles {

namespace {

std::vector<std::string> StringArray(const json::Value* node) {
    std::vector<std::string> result;
    if (!node) return result;
    for (const auto& item : node->AsArray()) result.push_back(item.AsString());
    return result;
}

bool ReadFileToString(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

} // namespace

bool PlatformProfileLoader::LoadFromFile(const std::string& path, PlatformProfile& out) {
    std::string text;
    if (!ReadFileToString(path, text)) {
        PAS_LOG_ERROR("PlatformProfileLoader", "No se pudo abrir '%s'", path.c_str());
        return false;
    }

    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, parse_error)) {
        PAS_LOG_ERROR("PlatformProfileLoader", "JSON invalido en '%s': %s", path.c_str(),
                      parse_error.c_str());
        return false;
    }

    if (!root.Get("id") || !root.Get("cpu_architecture")) {
        PAS_LOG_ERROR("PlatformProfileLoader", "'%s': faltan campos obligatorios "
                                               "('id', 'cpu_architecture')", path.c_str());
        return false;
    }

    PlatformProfile profile;
    profile.id = root.Get("id")->AsString();
    profile.cpu_architecture = root.Get("cpu_architecture")->AsString();
    profile.default_cpu_features = StringArray(root.Get("default_cpu_features"));
    profile.default_graphics_api = root.Get("default_graphics_api")
        ? root.Get("default_graphics_api")->AsString() : "";
    profile.default_required_apis = StringArray(root.Get("default_required_apis"));
    profile.device_paths_to_intercept = StringArray(root.Get("device_paths_to_intercept"));

    PAS_LOG_INFO("PlatformProfileLoader", "PlatformProfile '%s' cargado (%zu rutas de "
                "dispositivo interceptadas)", profile.id.c_str(),
                profile.device_paths_to_intercept.size());

    out = std::move(profile);
    return true;
}

} // namespace pas::profiles
