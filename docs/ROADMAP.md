# Roadmap

## Primer objetivo elegido: Sega Lindbergh

Evaluado contra los criterios pedidos (facilidad de investigación + documentación + simplicidad + debugging + reutilización + probabilidad real de funcionar):

| Criterio | Lindbergh | RingWide/RingEdge (Windows) | Taito Type X |
|---|---|---|---|
| Documentación pública | **Muy alta** — `lindbergh-loader` es una reimplementación completa y documentada de la placa | Media — specs de hardware públicas, pero cero trabajo público de compatibilidad software equivalente | Media-baja, muy fragmentada por revisión |
| Simplicidad | **Alta** — sin Windows, sin Win32, x86 de 32 bits, runtime gráfico único y conocido (Cg 3.1) | Baja — requiere Win32 desde el primer juego | Baja — Windows XP/7/10 según revisión, dongles USB |
| Debugging | **Alta** — se puede correr `lindbergh-loader` real en un PC x86 Linux para comparar comportamiento byte a byte con nuestra reimplementación en Switch, aislando si un bug es del JIT/traductor o del propio shim | Media — no hay equivalente directo de "referencia que corre en Linux x86" | Baja |
| Reutilización | **Alta** — el propio `lindbergh-loader` documenta exactamente qué interceptar, así que no hace falta ingeniería inversa desde cero de la placa | Ninguna referencia equivalente | Ninguna referencia equivalente pública tan completa |
| Probabilidad real de funcionar en Switch | **Alta relativa** — CPU 32-bit simple, sin AVX, gráficos OpenGL de función fija+Cg (subconjunto acotado), sin capa Win32 | Baja en el corto plazo — depende de una capa Win32 que no existe todavía | Baja en el corto plazo, mismo motivo |

**No se elige por ser el juego más famoso** (el encargo lo pide explícitamente) — se elige porque es, con diferencia, el sistema con menor superficie de riesgo desconocido: la mayoría de las incógnitas de este proyecto (traducción de CPU, backend gráfico, memoria ejecutable en Horizon) ya son suficiente riesgo por sí solas; no tiene sentido apilar además el riesgo de una capa Win32 completa en el primer intento.

### Primer título candidato dentro de Lindbergh

**[DESCONOCIDO — pendiente de decidir con datos reales]**: el roadmap no fija todavía un título concreto (p. ej. "The House of the Dead 4" vs otro de la lista de `lindbergh-loader/docs/supported.md`) porque esa elección debe hacerse mirando la lista de shaders Cg reales y la complejidad de motor de cada título candidato — información que requiere tener el cargador ELF y el extractor de shaders funcionando primero. Fijar un título de antemano sin esos datos sería inventar una decisión, en contra de la sección 21 del encargo.

## MVP en tres niveles

### Nivel 1 — Primer frame

Componentes estrictamente necesarios para que un ejecutable Lindbergh llegue a emitir su primer frame por deko3d:

1. ✅ Cargador ELF x86 de 32 bits (`os/elf_loader/`) — parseo de cabecera/program headers/section headers, carga de segmentos PT_LOAD, tabla de símbolos dinámicos y reubicaciones `R_386_RELATIVE`/`R_386_JMP_SLOT`/`R_386_GLOB_DAT`/`R_386_32`/`R_386_PC32` **implementado y con tests**. Cubierto por dos suites: `tests/test_elf_loader.cpp` (ELF32 sintético construido a mano) y `tests/test_elf_loader_real_binary.cpp` (ELF32 **real**, generado en tiempo de build con `gcc -m32` a partir de `tests/fixtures/real_elf32_sample.c` — dinámicamente enlazado, con PLT/GOT y reubicaciones reales de glibc; se salta con un aviso si el host no tiene soporte multilib de 32 bits en vez de romper el build). **No se dispone de un binario real de Lindbergh** para probar esto — son juegos comerciales con copyright, dumps de placas arcade reales, y obtenerlos por cualquier vía que no sea el hardware legítimamente poseído por quien desarrolla esto no es algo que este proyecto persiga; el fixture compilado es el sustituto legal más cercano hasta que exista un binario real con el que probar. Pendiente cuando llegue ese binario real: confirmar que el subconjunto de tipos de reubicación cubiertos es suficiente (Lindbergh usa un compilador/libc bastante más antiguos que los de este fixture).
2. libc-shim mínima (`os/`) — malloc/free, I/O de fichero básico, `gettimeofday`. **Sin implementar todavía** (`os/syscall/libc_shim.h` solo tiene el registro genérico por nombre, sin ninguna función real registrada).
3. Decodificador x86 (subconjunto real usado por el binario) + IR + backend ARM64 (`cpu/`). **Los cuatro componentes del pipeline (decoder → IR builder → codegen ARM64 → code cache) están implementados y conectados de extremo a extremo** (`Jit::TranslateBlock`, ver `docs/JIT.md`) para el subconjunto MOV/Grupo1(ADD-SUB-AND-OR-XOR-CMP)/RET con operandos de registro e inmediato — validado no solo por codificación sino por **ejecución real**: el ARM64 generado corre bajo `qemu-aarch64` y se compara contra el x86 original corriendo nativamente, para 11 valores de entrada distintos incluyendo `INT_MIN`/`INT_MAX` (`tests/test_ir_end_to_end.cpp`, `tests/test_jit_integration.cpp` — este último a través de la clase `Jit` real, no piezas sueltas). El backend ARM64 en sí está además contrastado contra `switch_ppc_jit_arm64.h` (coinciden bit a bit). **Sigue faltando**: cualquier operando de memoria (LEA, PUSH/POP reales, MOV con memoria — la mayoría de instrucciones de un binario real las usan, ver el ejemplo de `main()` en `docs/CPU_TRANSLATION.md`), saltos condicionales (Jcc/JMP), SSE/SSE2/x87 (imprescindibles para Lindbergh), y el bucle de dispatch de `Jit::RunFrom` (que hoy traduce un bloque y se detiene, no salta a él ni encadena bloques — ver `docs/JIT.md`, tabla de estado).
4. Memoria ejecutable Horizon-safe (`platform/switch/`, patrón ya validado en Super3-NX). Escrito contra la API pública de `jitCreate`/`jitTransitionToWritable`/`jitTransitionToExecutable` (`platform/switch/jit_memory.cpp`), **sin verificar todavía contra una compilación real de devkitA64** (ver nota de honestidad en ese fichero).
5. Shim de eeprom/sram (`arcade/devices/`) — muchos juegos comprueban esto antes incluso de intentar dibujar nada. **Sin implementar** — pendiente el layout real de `amSysDataRecord`.
6. Traductor GL/Cg → deko3d, aunque sea con shaders precompilados a mano para las primeras llamadas observadas (`graphics/`). **Sin implementar.**
7. ✅ Parser de `GameProfile`/`PlatformProfile` (`profiles/json_lite.h` + `profiles/game_profile.cpp`/`platform_profile.cpp`) — parser JSON propio (sin dependencias externas, ver `third_party/README.md`) con tests que cargan los JSON reales de `profiles/`. No estaba en la lista original de este nivel pero resultó ser un bloqueador de facto para poder probar nada end-to-end, así que se adelantó.

Criterio de éxito: aparece un frame reconocible (aunque sea con gráficos incompletos/incorrectos) en la pantalla de Switch. **Todavía no alcanzado** — de los 7 puntos de arriba, 2 están hechos y probados, 5 siguen pendientes.

### Nivel 2 — Gameplay

Añade sobre el Nivel 1:

7. JVS virtual completo + mapeo de input (`arcade/jvs/`, `input/`)
8. Invalidación de bloques JIT (self-modifying code) si el título lo requiere
9. Extracción y precompilación completa de shaders del título elegido (no solo los observados en el primer frame)
10. Patch engine operativo (aunque el primer título no necesite ningún patch, el motor debe estar integrado, no añadido a posteriori)

Criterio de éxito: se puede jugar una partida completa del título elegido, con input real, sin fallos de traducción que rompan la ejecución.

### Nivel 3 — Experiencia completa

11. Audio
12. Caché de código JIT persistente entre ejecuciones (tiempos de carga aceptables)
13. Perfilado y ajuste de rendimiento dirigido por datos reales (no suposiciones)
14. Cobertura de un segundo y tercer título Lindbergh, para validar que el `GameProfile`/`PlatformProfile` generaliza y no quedó implícitamente acoplado al primer título

## Preguntas que el MVP tiene que responder (no asumidas de antemano)

- ¿Es el rendimiento de un DynaRec x86→ARM64 sobre Cortex-A57 a las frecuencias de Switch siquiera jugable para el motor Lindbergh, o hace falta compilación estática/AOT en vez de JIT puro? **No se sabe hasta medir el Nivel 1.**
- ¿Es viable precompilar shaders Cg offline con `uam` de forma sistemática, o hace falta el compilador Cg→DKSH en tiempo de ejecución? **No se sabe hasta intentarlo con shaders reales del título elegido.**
- ¿Cuánta RAM ocupa realmente el conjunto de trabajo de un título Lindbergh cargado + su caché de código JIT + los recursos gráficos traducidos? **No se sabe hasta medirlo.**

## Riesgos técnicos (sección 21 del encargo)

| Riesgo | Severidad | Mitigación planteada |
|---|---|---|
| Rendimiento del JIT en Cortex-A57 insuficiente | Alta — es el riesgo que puede invalidar el proyecto entero | Medir en el Nivel 1 antes de invertir en Niveles 2-3; tener claro de antemano que si el rendimiento no es viable, el proyecto pivota a un target con menos carga de CPU en vez de insistir |
| Compilación de shaders Cg→deko3d en runtime no viable | Media — afecta alcance, no viabilidad total | Camino de precompilación offline por perfil como plan A (ver `GRAPHICS.md`) |
| Self-modifying code no detectado a tiempo | Media | Mecanismo de invalidación diseñado desde el principio, no añadido después (lección directa de Super3-NX) |
| Superficie de libc-shim subestimada (el binario llama a más funciones de las previstas) | Baja-media | El cargador ELF debe listar símbolos no resueltos de forma explícita en el log, no fallar en silencio, para descubrir el alcance real incrementalmente |
| Térmico en sesiones largas (JIT + render simultáneos) | Media, solo relevante en Nivel 3 | Exponer cap de reloj configurable por perfil, no asumir reloj máximo sostenido indefinidamente |

## Explícitamente descartado por ahora (no "olvidado")

- Namco/Konami/Raw Thrills: sin investigación suficiente para decidir, requieren su propia pasada de investigación antes de entrar en el roadmap.
- Cualquier sistema Windows: bloqueado por la ausencia de capa Win32, que es en sí misma un proyecto grande — se aborda después de validar que el JIT y el traductor gráfico son viables con el target más simple.
