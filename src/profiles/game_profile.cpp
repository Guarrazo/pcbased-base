#include "profiles/game_profile.h"
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

std::vector<patch::BytePatch> ParseBytePatches(const json::Value* node) {
    std::vector<patch::BytePatch> result;
    if (!node) return result;
    for (const auto& item : node->AsArray()) {
        patch::BytePatch p;
        // offset/expect/replace se aceptan en hex ("0x..") o decimal --
        // docs/PATCHING.md muestra el formato hex, que es lo natural al
        // editar patches a mano.
        p.offset = static_cast<uint32_t>(std::strtoul(
            item.Get("offset") ? item.Get("offset")->AsString("0").c_str() : "0", nullptr, 0));

        auto hex_to_bytes = [](const std::string& hex) {
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i + 1 < hex.size(); i += 2) {
                bytes.push_back(static_cast<uint8_t>(std::strtoul(hex.substr(i, 2).c_str(), nullptr, 16)));
            }
            return bytes;
        };
        if (auto* expect = item.Get("expect")) p.expect = hex_to_bytes(expect->AsString());
        if (auto* replace = item.Get("replace")) p.replace = hex_to_bytes(replace->AsString());
        result.push_back(std::move(p));
    }
    return result;
}

std::vector<patch::SymbolHook> ParseSymbolHooks(const json::Value* node) {
    std::vector<patch::SymbolHook> result;
    if (!node) return result;
    for (const auto& item : node->AsArray()) {
        patch::SymbolHook h;
        h.symbol = item.Get("symbol") ? item.Get("symbol")->AsString() : "";
        std::string action = item.Get("action") ? item.Get("action")->AsString() : "stub_return_ok";
        if (action == "stub_return_fail") h.action = patch::SymbolHook::Action::StubReturnFail;
        else if (action == "call_custom") h.action = patch::SymbolHook::Action::CallCustom;
        else h.action = patch::SymbolHook::Action::StubReturnOk;
        result.push_back(std::move(h));
    }
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

bool GameProfileLoader::LoadFromFile(const std::string& path, GameProfile& out) {
    std::string text;
    if (!ReadFileToString(path, text)) {
        PAS_LOG_ERROR("GameProfileLoader", "No se pudo abrir '%s'", path.c_str());
        return false;
    }

    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, parse_error)) {
        PAS_LOG_ERROR("GameProfileLoader", "JSON invalido en '%s': %s", path.c_str(),
                      parse_error.c_str());
        return false;
    }

    if (root.type() != json::Type::Object) {
        PAS_LOG_ERROR("GameProfileLoader", "'%s': la raiz debe ser un objeto JSON", path.c_str());
        return false;
    }

    // Campos obligatorios segun docs/GAME_PROFILES.md -- si faltan, se
    // rechaza el perfil entero en vez de continuar con un GameProfile a
    // medias (mismo principio que "expect" en BytePatch: mejor fallar
    // ruidosamente que arrancar con datos incompletos).
    const char* required_fields[] = {"id", "platform", "executable", "architecture"};
    for (const char* field : required_fields) {
        if (!root.Get(field)) {
            PAS_LOG_ERROR("GameProfileLoader", "'%s': falta el campo obligatorio '%s'",
                          path.c_str(), field);
            return false;
        }
    }

    GameProfile profile;
    profile.id = root.Get("id")->AsString();
    profile.display_name = root.Get("display_name") ? root.Get("display_name")->AsString() : profile.id;
    profile.platform_id = root.Get("platform")->AsString();
    profile.executable = root.Get("executable")->AsString();
    profile.architecture = root.Get("architecture")->AsString();
    profile.cpu_features = StringArray(root.Get("cpu_features"));
    profile.graphics_api = root.Get("graphics_api") ? root.Get("graphics_api")->AsString() : "";
    profile.required_apis = StringArray(root.Get("required_apis"));

    if (auto* arcade_io = root.Get("arcade_io")) {
        if (auto* dp = arcade_io->Get("device_profile")) profile.device_profile = dp->AsString();
    }
    profile.shader_bundle = root.Get("shader_bundle") ? root.Get("shader_bundle")->AsString() : "";

    profile.patches = ParseBytePatches(root.Get("patches"));
    profile.hooks = ParseSymbolHooks(root.Get("hooks"));

    profile.controller_mapping = root.Get("controller_mapping")
        ? root.Get("controller_mapping")->AsString() : "";

    if (auto* display = root.Get("display")) {
        if (auto* res = display->Get("internal_resolution")) {
            const auto& arr = res->AsArray();
            if (arr.size() == 2) {
                profile.display.internal_width = static_cast<uint32_t>(arr[0].AsInt(1280));
                profile.display.internal_height = static_cast<uint32_t>(arr[1].AsInt(720));
            }
        }
        if (auto* aspect = display->Get("aspect")) profile.display.aspect = aspect->AsString();
    }

    if (auto* network = root.Get("network")) {
        if (auto* mode = network->Get("mode")) profile.network.mode = mode->AsString();
    }

    if (auto* fs = root.Get("filesystem")) {
        if (auto* r = fs->Get("root")) profile.filesystem.root = r->AsString();
    }
    if (profile.filesystem.root.empty()) {
        PAS_LOG_ERROR("GameProfileLoader", "'%s': falta 'filesystem.root' -- obligatorio "
                                           "(docs/INPUT_OUTPUT.md, aislamiento por juego)",
                      path.c_str());
        return false;
    }

    profile.compatibility_flags = StringArray(root.Get("compatibility_flags"));
    profile.code_cache_size_mb = root.Get("code_cache_size_mb")
        ? static_cast<uint32_t>(root.Get("code_cache_size_mb")->AsInt(64)) : 64;
    profile.launch_parameters = StringArray(root.Get("launch_parameters"));
    profile.revision = root.Get("revision")
        ? static_cast<uint32_t>(root.Get("revision")->AsInt(1)) : 1;

    PAS_LOG_INFO("GameProfileLoader", "Perfil '%s' cargado (plataforma '%s', ejecutable '%s')",
                profile.id.c_str(), profile.platform_id.c_str(), profile.executable.c_str());

    out = std::move(profile);
    return true;
}

} // namespace pas::profiles
