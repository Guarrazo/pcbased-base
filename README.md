# pc-arcade-switch

Compatibility layer para ejecutar juegos arcade PC-based (empezando por **Sega Lindbergh**) en Nintendo Switch homebrew, sin emular un PC completo.

Este repositorio es el resultado de una investigación técnica dirigida (ver `docs/`) antes de escribir una sola línea de traductor. **Lee `docs/STATE_OF_THE_ART.md` y `docs/ARCHITECTURE.md` antes de tocar código** — explican por qué se descartó Win32/Wine para el MVP, por qué Lindbergh y no un sistema Windows, y qué partes del proyecto Super3-NX (JIT PowerPC→ARM64) se reutilizan directamente.

## Documentación

| Documento | Contenido |
|---|---|
| `docs/STATE_OF_THE_ART.md` | Qué existe hoy: Box64, FEX, TeknoParrot, lindbergh-loader, DXVK, Wine — con distinción explícita entre hechos confirmados e inferencias |
| `docs/ARCHITECTURE.md` | Matriz de plataformas arcade, decisión Opción A/B/C, diagrama de flujo de ejecución, respuestas a las preguntas centrales del proyecto |
| `docs/SWITCH_PLATFORM.md` | Hardware y límites reales de Nintendo Switch/Horizon OS aplicados a este proyecto |
| `docs/CPU_TRANSLATION.md` | Por qué DBT propio (no Box64/FEX portados) y qué se reutiliza de Super3-NX |
| `docs/WINDOWS_COMPATIBILITY.md` | Por qué el MVP no necesita Win32, y cuándo sí hará falta |
| `docs/GRAPHICS.md` | OpenGL+Cg → deko3d, riesgo abierto de compilación de shaders |
| `docs/ARCADE_HARDWARE.md` | JVS virtual, eeprom/sram sintéticos |
| `docs/INPUT_OUTPUT.md` | Input, display, audio, red, filesystem |
| `docs/PATCHING.md` | Motor de patches sobre IR |
| `docs/GAME_PROFILES.md` | Formato de `GameProfile`/`PlatformProfile` |
| `docs/MEMORY_MODEL.md` | TSO x86 vs modelo débil ARM64, atomics, TLS |
| `docs/JIT.md` | Pipeline del JIT, qué se reutiliza de `switch_ppc_jit_arm64.h` |
| `docs/ROADMAP.md` | Elección de Lindbergh como primer objetivo, MVP en 3 niveles, riesgos |

## Estado actual

Esqueleto compilable, sin lógica de traducción implementada todavía (eso es la siguiente fase, no cubierta por este primer commit). Ver `docs/ROADMAP.md` → "MVP en tres niveles" para el orden de trabajo.

## Build

Requiere devkitPro con devkitA64 y libnx instalados, con `DEVKITPRO` apuntando a la instalación (igual que tus otros proyectos Switch).

```bash
export DEVKITPRO=/opt/devkitpro   # ajusta a tu instalación
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
make
```

Genera `pc-arcade-switch.nro`.

## Build de host (para desarrollo, sin Switch)

```bash
mkdir build-host && cd build-host
cmake .. -DPAS_BUILD_TESTS=ON
make
./tests/pas_tests
```

Dependencias opcionales que activan tests adicionales (se saltan con un aviso si no están, no rompen el build):

| Paquete (Debian/Ubuntu) | Activa |
|---|---|
| `gcc-multilib`, `libc6-dev-i386` | `test_elf_loader_real_binary`, y el arnés de ejecución x86-32 nativa (`test_ir_end_to_end`, `test_jit_integration`) — Linux ejecuta binarios ia32 estáticos directamente en un host x86-64, sin emulación |
| `binutils-aarch64-linux-gnu` | Ninguno directamente en CI, pero es como se derivaron y verificaron los encodings del emisor ARM64 (ver `docs/JIT.md`) |
| `gcc-aarch64-linux-gnu`, `qemu-user` | `test_arm64_dynamic_exec`, `test_ir_end_to_end`, `test_jit_integration` — compilan y **ejecutan de verdad** (bajo `qemu-aarch64`) el código máquina generado, comprobando el resultado real de la CPU, no solo la codificación (ver `docs/JIT.md`) |

## Estructura

```text
src/
 ├── core/           logging, config, registro de módulos — independiente de Switch
 ├── cpu/            decodificador x86, IR, backend ARM64
 ├── os/             cargador ELF, libc-shim, syscall-shim
 ├── graphics/       traductor GL/Cg + backend deko3d
 ├── arcade/         JVS virtual, dispositivos (eeprom/sram)
 ├── input/          abstracción de entrada
 ├── patch/          motor de patches sobre IR
 ├── hooks/          infraestructura de hooks de símbolos
 ├── profiles/       GameProfile / PlatformProfile
 └── platform/switch/  todo lo específico de Horizon OS/libnx (entry point, jitCreate, HidNpad, deko3d init)
profiles/            GameProfile de ejemplo (Lindbergh)
tests/                pruebas unitarias de host (no requieren hardware Switch)
tools/                herramientas de extracción/preparación de assets (placeholder)
third_party/          dependencias externas a vendorizar (ver third_party/README.md)
```

Ver `docs/ARCHITECTURE.md` §3 para la justificación de esta separación `core`/`platform`.
