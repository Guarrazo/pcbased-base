# Input / Output

## Input

Ver `docs/ARCADE_HARDWARE.md` para el mapeo JVS → HID de Switch, que es la parte específica de arcade. Este documento cubre la capa de abstracción genérica:

- `src/input/` define una interfaz `InputSource` independiente de Switch (útil si algún día se añade soporte de I/O físico real vía USB-host, sin tocar el resto del pipeline).
- El mapeo botón-físico ↔ input-JVS-esperado vive en el `GameProfile`/`PlatformProfile` (`controller_mapping`), nunca hardcodeado.
- Sticks analógicos: JVS expone ejes analógicos de 8/10 bits según placa; libnx expone floats normalizados — la conversión de escala/rango vive en `arcade/jvs/`, no en `input/`.

## Display

- Resolución interna configurable por perfil (`display.internal_resolution`) — Lindbergh soporta múltiples resoluciones VGA/HD; no hay que asumir 1280x720 para todos los títulos, es un valor por perfil con ese como default razonable dado el hardware de Switch.
- Aspect ratio: muchos juegos Lindbergh de la época son 4:3; el backend deko3d debe soportar letterboxing sin reescalar el contenido del juego de forma distorsionada.
- Dual-monitor (soportado por hardware Lindbergh real): fuera de alcance, no aplica a un dispositivo de una sola pantalla — se documenta como descartado explícitamente, no como olvido.

## Audio

**Explícitamente fuera del MVP inicial** (no listado como bloqueante para "primer frame" ni "gameplay" en `docs/ROADMAP.md`) — se aborda después de validar CPU+gráficos+input, con su propio documento de investigación cuando llegue el momento (formato de audio real usado por los juegos Lindbergh no investigado todavía — **[DESCONOCIDO]**).

## Networking

Los juegos Lindbergh con soporte ALL.Net asumen conectividad a servidores Sega que ya no existen — se declara `network.mode: "disabled"` por perfil por defecto, y el `GameProfile` debe declarar explícitamente si un título requiere red simulada (servidor local falso) para arrancar, caso por caso, no de antemano.

## Filesystem

Cada `GameProfile` declara una raíz (`filesystem.root`) bajo `sdmc:/arcade/<juego>/` — el cargador ELF y la libc-shim resuelven cualquier acceso a fichero del juego relativo a esa raíz, nunca a rutas absolutas del sistema real (mismo principio de aislamiento que ya usa `lindbergh-loader` al ejecutar cada juego desde su propio directorio).
