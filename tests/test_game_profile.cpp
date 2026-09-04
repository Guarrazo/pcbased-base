// Test de host para GameProfileLoader/PlatformProfileLoader -- carga los
// JSON de ejemplo reales que vive en profiles/ (docs/GAME_PROFILES.md), no
// texto inventado en el test, para detectar si el esquema documentado y el
// parser real divergen.
#include "profiles/game_profile.h"
#include "profiles/platform_profile.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
// Los tests de host se ejecutan desde build-host/tests/ -- la raiz del
// repo esta dos niveles por encima. Se permite sobreescribir por variable
// de entorno para no asumir un cwd concreto en otros entornos de CI.
std::string RepoRoot() {
#ifdef PAS_REPO_ROOT
    return PAS_REPO_ROOT; // definido por CMake (tests/CMakeLists.txt) con
                          // la ruta absoluta -- evita depender del cwd
                          // desde el que se ejecute pas_tests
#else
    const char* env = std::getenv("PAS_REPO_ROOT");
    return env ? env : "../..";
#endif
}
}

static void TestLoadsRealLindberghPlatformProfile() {
    pas::profiles::PlatformProfile profile;
    pas::profiles::PlatformProfileLoader loader;
    std::string path = RepoRoot() + "/profiles/lindbergh.platform.json";
    bool ok = loader.LoadFromFile(path, profile);
    assert(ok);
    assert(profile.id == "lindbergh");
    assert(profile.cpu_architecture == "x86_32");
    assert(profile.default_required_apis.size() == 3);
    assert(profile.device_paths_to_intercept.size() == 4);
    bool found_lbb = false;
    for (const auto& p : profile.device_paths_to_intercept) if (p == "/dev/lbb") found_lbb = true;
    assert(found_lbb);
    std::printf("OK: TestLoadsRealLindberghPlatformProfile\n");
}

static void TestLoadsRealExampleGameProfile() {
    pas::profiles::GameProfile profile;
    pas::profiles::GameProfileLoader loader;
    std::string path = RepoRoot() + "/profiles/example.json";
    bool ok = loader.LoadFromFile(path, profile);
    assert(ok);
    assert(profile.platform_id == "lindbergh");
    assert(profile.executable == "game.elf");
    assert(profile.architecture == "x86_32");
    assert(profile.graphics_api == "opengl_cg");
    assert(profile.display.internal_width == 1280);
    assert(profile.display.internal_height == 720);
    assert(profile.filesystem.root == "sdmc:/arcade/example_lindbergh_title/");
    assert(profile.code_cache_size_mb == 64);
    std::printf("OK: TestLoadsRealExampleGameProfile\n");
}

static void TestRejectsProfileMissingRequiredField() {
    pas::profiles::GameProfile profile;
    pas::profiles::GameProfileLoader loader;
    // Fichero temporal sin 'executable' -- debe rechazarse explicitamente
    // (docs/GAME_PROFILES.md: nunca arrancar con datos incompletos).
    std::string tmp_path = "/tmp/pas_test_incomplete_profile.json";
    FILE* f = std::fopen(tmp_path.c_str(), "w");
    assert(f != nullptr);
    std::fputs(R"({"id": "x", "platform": "lindbergh", "architecture": "x86_32"})", f);
    std::fclose(f);

    bool ok = loader.LoadFromFile(tmp_path, profile);
    assert(!ok);
    std::printf("OK: TestRejectsProfileMissingRequiredField\n");
}

void RunGameProfileTests() {
    TestLoadsRealLindberghPlatformProfile();
    TestLoadsRealExampleGameProfile();
    TestRejectsProfileMissingRequiredField();
    std::printf("Todos los tests de game_profile pasaron.\n");
}
