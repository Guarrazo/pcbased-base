# Game Profiles

## Objetivo

Evitar por completo `if (game == X)` disperso por el código (sección 11 del encargo). Un `GameProfile` es un documento de datos (JSON — ver `profiles/`) más un `PlatformProfile` del que hereda por defecto (p. ej. todos los juegos Lindbergh comparten el mismo `PlatformProfile` con el mapeo de dispositivos base, y cada `GameProfile` solo declara las diferencias).

## Esquema (ver `src/profiles/game_profile.h` para la versión C++)

```jsonc
{
  "id": "hotd4",
  "display_name": "The House of the Dead 4",
  "platform": "lindbergh",           // referencia a un PlatformProfile
  "executable": "hotd4.elf",
  "architecture": "x86_32",
  "cpu_features": ["sse", "sse2", "x87"],
  "graphics_api": "opengl_cg",
  "required_apis": ["libc_shim", "cg_3_1", "gl_1_x"],
  "arcade_io": {
    "device_profile": "lindbergh_gun_2p"
  },
  "shader_bundle": "hotd4_shaders.dksh_pack",  // salida de la extracción offline (ver GRAPHICS.md)
  "patches": [],                      // vacío hasta que se identifique alguno
  "hooks": [],
  "memory_patches": [],
  "controller_mapping": "lindbergh_gun_default",
  "display": { "internal_resolution": [1280, 720], "aspect": "4:3" },
  "network": { "mode": "disabled" },
  "timing": { "rdtsc_scale": "auto" },
  "filesystem": { "root": "sdmc:/arcade/hotd4/" },
  "dll_overrides": [],                // reservado para el roadmap Windows
  "compatibility_flags": [],
  "code_cache_size_mb": 64,
  "launch_parameters": []
}
```

## Reglas de diseño

- Un `PlatformProfile` (`src/profiles/platform_profile.h`) captura todo lo común a una placa arcade: qué shims de dispositivo instalar, qué subconjunto de libc-shim cargar, la CPU base a decodificar. `lindbergh.platform.json` es el primero y único para el MVP.
- El `PatchSet` y los `hooks` de un `GameProfile` son **datos**, no código: se interpretan por el `PatchEngine` genérico (`docs/PATCHING.md`), nunca compilados condicionalmente por juego.
- Todo motor (`CompatibilityModule`) se registra por nombre (`libc_shim`, `cg_3_1`, `gl_1_x`, `win32_min` para el roadmap) y un `GameProfile` solo declara **cuáles necesita**, nunca implementa lógica propia.
- Cambios que alteren el comportamiento de un perfil ya publicado deben incrementar un campo `revision` (mismo patrón que usa TeknoParrot en sus `GameProfile` XML, confirmado en su documentación de contribución) para no romper instalaciones existentes.

## Qué NO resuelve el sistema de perfiles

No decide *cómo* se traduce CPU, ni *cómo* se traduce gráficos — eso es responsabilidad de los módulos genéricos de `core/`. El perfil solo **selecciona y parametriza** módulos ya existentes.
