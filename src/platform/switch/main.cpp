// Entry point de pc-arcade-switch (docs/ARCHITECTURE.md §4, "Flujo de ejecucion").
//
// Este fichero es deliberadamente el UNICO lugar del proyecto (junto con
// jit_memory.cpp/input_hid.cpp/graphics_deko3d.cpp) que incluye <switch.h>.
// Todo lo demas (cpu/, os/, graphics/, arcade/, patch/, profiles/...) es
// independiente de plataforma -- ver docs/ARCHITECTURE.md §3.
//
// NOTA DE HONESTIDAD: no se ha podido compilar este fichero contra una
// instalacion real de devkitA64/libnx en el entorno donde se generó este
// esqueleto (sin acceso de red a los paquetes de devkitPro). Antes de dar
// por bueno que compila sin cambios, constrúyelo con tu toolchain local
// (ver README.md, sección Build) y ajusta lo que haga falta -- es
// razonablemente fiel a la API pública de libnx tal y como la documentan
// los ejemplos oficiales de devkitPro y como ya la usas en Super3-NX /
// DLSS-Switch, pero no ha pasado por un compilador real todavía.

#ifdef __SWITCH__
#include <switch.h>
#endif

#include "core/log.h"
#include "core/config.h"
#include "core/module.h"
#include "platform/switch/jit_memory.h"
#include "platform/switch/input_hid.h"
#include "platform/switch/graphics_deko3d.h"
#include "cpu/jit/jit.h"
#include "os/elf_loader/elf_loader.h"
#include "profiles/game_profile.h"
#include "profiles/platform_profile.h"

#include <memory>
#include <cstdio>

namespace {

#ifdef __SWITCH__
void SdmcLogSink(pas::core::LogLevel level, const char* tag, const char* message) {
    static const char* kLevelNames[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR"};
    // TODO(Nivel 1): abrir/mantener un fichero en sdmc:/arcade/logs/ en vez
    // de imprimir por consola -- consola homebrew no siempre es visible
    // salvo con nxlink. Placeholder con printf (visible via nxlink) por
    // ahora, para tener un esqueleto compilable y util para depurar.
    std::printf("[%s][%s] %s\n", kLevelNames[static_cast<int>(level)], tag, message);
}
#endif

} // namespace

#ifdef __SWITCH__
extern "C" void userAppInit() {
    // TODO: romfsInit()/fsdevMountSdmc() si hace falta segun como se
    // distribuya el .nro final -- no implementado en este esqueleto.
}

extern "C" void userAppExit() {
}
#endif

int main(int /*argc*/, char** /*argv*/) {
#ifdef __SWITCH__
    consoleInit(nullptr); // consola de depuracion basica -- suficiente para
                           // ver el log del esqueleto en un emulador/hardware
                           // real con nxlink; se sustituye mas adelante
    pas::core::SetLogSink(SdmcLogSink);
#endif

    PAS_LOG_INFO("main", "pc-arcade-switch -- esqueleto inicial, ver docs/ROADMAP.md");

    pas::core::Config::Instance().LoadFromFile("sdmc:/arcade/config.ini");

    // --- Carga de perfiles (docs/GAME_PROFILES.md) ---------------------
    pas::profiles::PlatformProfile platform_profile;
    pas::profiles::PlatformProfileLoader platform_loader;
    if (!platform_loader.LoadFromFile("sdmc:/arcade/platforms/lindbergh.platform.json",
                                       platform_profile)) {
        PAS_LOG_ERROR("main", "No se pudo cargar el PlatformProfile 'lindbergh' -- "
                              "GameProfileLoader/PlatformProfileLoader no estan "
                              "implementados todavia (docs/ROADMAP.md, Nivel 1)");
    }

    pas::profiles::GameProfile game_profile;
    pas::profiles::GameProfileLoader game_loader;
    if (!game_loader.LoadFromFile("sdmc:/arcade/profiles/example.json", game_profile)) {
        PAS_LOG_ERROR("main", "No se pudo cargar ningun GameProfile -- deteniendo aqui, "
                              "es el comportamiento esperado de este esqueleto (nada mas "
                              "abajo esta implementado todavia)");
#ifdef __SWITCH__
        consoleUpdate(nullptr);
        // TODO: bucle de espera de boton para poder leer el log en pantalla
        // antes de salir -- no implementado en este esqueleto.
        consoleExit(nullptr);
#endif
        return 1;
    }

    // --- A partir de aqui, el flujo real (docs/ARCHITECTURE.md §4) -----
    // No alcanzable todavia porque los loaders de perfiles no estan
    // implementados -- se deja el codigo tal y como deberia quedar para
    // que quede claro el orden de inicializacion previsto.

    auto executable_memory = std::make_unique<pas::platform::switch_::SwitchExecutableMemory>(
        game_profile.code_cache_size_mb * 1024u * 1024u);

    pas::cpu::jit::Jit jit(std::move(executable_memory));

    pas::platform::switch_::SwitchHidInputBackend input_backend;
    pas::platform::switch_::Deko3dGraphicsBackend graphics_backend;
    graphics_backend.Init(game_profile.display.internal_width,
                          game_profile.display.internal_height);

    pas::os::ElfLoader elf_loader;
    // TODO: leer game_profile.executable desde game_profile.filesystem.root,
    // pasar los bytes a elf_loader.Load(), y si tiene exito, jit.RunFrom(...)
    // con el entry point resultante. No implementado en este esqueleto.

    PAS_LOG_WARN("main", "Fin del esqueleto -- flujo de carga real no implementado todavia");

#ifdef __SWITCH__
    consoleUpdate(nullptr);
    consoleExit(nullptr);
#endif
    return 0;
}
