# Arquitectura

## 1. Matriz de plataformas arcade PC-based

| Plataforma | CPU | GPU | OS | API gráfica | I/O | Protección | Red | Particularidades | ¿Viable para Switch? |
|---|---|---|---|---|---|---|---|---|---|
| **Sega Lindbergh** | Pentium 4 3.0GHz / Celeron 2.8GHz (x86, **32 bits**) | NVIDIA GeForce 6/7 | **MontaVista Linux embebido** | OpenGL + NVIDIA Cg 3.1 | JVS | DIP switches + test/service (sin keychip real) | Ethernet, ALL.Net opcional | ELF nativo, ampliamente reverse-engineered (`lindbergh-loader`) | **Sí — elegido como MVP** |
| **Sega RingWide** | Celeron 440 2.0GHz (x86, 32 bits) | ATI Radeon HD 2xxx | Windows Embedded Standard 2009 | Direct3D 9 (probable, no verificado por título) | JVS | Similar a Lindbergh + capas Windows | Ethernet, ALL.Net P-ras | Primer sistema **Windows** de la familia — bajo perfil de HW | Roadmap — requiere Win32 mínimo |
| **Sega RingEdge** | Pentium Dual-Core E2160 1.8GHz (x86-64 capaz, binarios probablemente 32 bits) | GeForce 8800GS | Windows Embedded Standard 2009 | Direct3D 9 | JVS | Similar | Ethernet, ALL.Net P-ras | Requiere Win32 mínimo | Roadmap |
| **Sega RingEdge 2** | Core i3-540 3.07GHz (x86-64) | GeForce GT 545 | Windows Embedded Standard 2009 | Direct3D 9/10 probable | JVS | Similar, retrocompatible con RingEdge | Ethernet, ALL.Net P-ras v2 | Requisitos de RAM/GPU más altos | Roadmap tardío / dudoso en Switch |
| **Sega Nu / RingWide sucesores** | — | — | — | — | JVS | — | — | **[DESCONOCIDO]** no investigado en profundidad, requisitos previsiblemente superiores a RingEdge 2 | Descartado inicialmente por presupuesto de investigación |
| **Taito Type X** | Pentium 4 / Core 2 Duo según revisión | GeForce 6/7 | **Windows XP Embedded** | Direct3D 9 | JVS | Dongle USB en algunos títulos | Variable | Familia muy amplia, requisitos variables por título | Roadmap, evaluar título a título |
| **Taito Type X2/X3** | Core 2 Duo E6400+ | GeForce 7900GS+ | Windows XP | Direct3D 9 | JVS | Dongle USB | Variable | RAM 4GB+, más exigente que Lindbergh | Roadmap tardío |
| **Namco System 357/369** | **[DESCONOCIDO]** — no investigado con detalle en esta pasada | — | Windows (probable) | Direct3D (probable) | JVS | Propietaria Namco | — | Documentación pública más escasa que Sega/Taito | Requiere investigación dedicada antes de decidir |
| **Konami PC-based** | Variable | Variable | Windows (variable por título) | Variable | JVS | Variable | — | Muy heterogéneo, sin estándar único de placa como Lindbergh/Type X | Descartado inicialmente — heterogeneidad alta, mal ratio esfuerzo/cobertura |
| **Raw Thrills / Global VR** | PC estándar de la época, variable | Variable | Windows (variable) | Direct3D variable | Variable, a menudo no-JVS (I/O propietaria) | Variable, a menudo dongle | — | I/O no estandarizada, cada gabinete distinto | Descartado inicialmente |

**Nota de rigor**: las filas de Namco y Konami están marcadas explícitamente como no investigadas en profundidad (sección 21 del encargo, "no inventes") — no se afirma nada sobre su CPU/GPU exactas sin fuente. Antes de planificarlas en el roadmap real haría falta una pasada de investigación dedicada exactamente igual a la hecha aquí para Sega/Taito.

## 2. Opción A vs B vs C — decisión

El encargo plantea tres opciones. La respuesta, con la investigación hecha, es:

- **Opción A (emulación completa de PC)** — descartada categóricamente. Ningún sistema arcade PC-based necesita esto: TeknoParrot en sí mismo demuestra que ni siquiera en PC real hace falta virtualizar el hardware subyacente, solo sustituir la placa arcade. Emular un PC entero en un Tegra X1 es, además, presupuestariamente inviable (ver `SWITCH_PLATFORM.md`).
- **Opción B (compatibility layer)** — es la aproximación correcta en espíritu, pero el diagrama del encargo asume Win32 como paso obligatorio. La investigación (ver `WINDOWS_COMPATIBILITY.md`) muestra que **para el primer objetivo real (Lindbergh) ese paso no existe**: no hay Windows de por medio.
- **Opción C (arquitectura híbrida, con Win32 como una pieza entre otras, no obligatoria)** — **es la elegida**, con una precisión importante: la instancia de "Win32 → compatibility layer" del diagrama del encargo se convierte, en la práctica, en **"OS-shim → compatibility layer"**, donde para Lindbergh el OS-shim es una libc-shim mínima de Linux/ELF, y solo para sistemas Windows posteriores del roadmap ese bloque se convierte en Win32.

```text
Juego (ELF x86 32-bit, target Lindbergh)
      │
      ├── CPU → DBT/JIT propio (docs/CPU_TRANSLATION.md, docs/JIT.md)
      │
      ├── OS-shim → libc-shim ELF (docs/WINDOWS_COMPATIBILITY.md explica cuándo esto pasa a ser Win32)
      │
      ├── Gráficos → OpenGL+Cg → deko3d (docs/GRAPHICS.md)
      │
      ├── Arcade I/O → JVS virtual → HID de Switch (docs/ARCADE_HARDWARE.md)
      │
      └── Patches específicos del juego (docs/PATCHING.md, docs/GAME_PROFILES.md)
```

## 3. Separación Core/Platform

```text
src/
 ├── core/        — logging, config, registro de módulos: NO sabe nada de Switch específicamente
 ├── cpu/         — decodificador x86 + IR + backend ARM64: el backend ARM64 SÍ es específico
 │                  de arquitectura de CPU (no de Switch como consola) — se reutilizaría en otro
 │                  target ARM64 sin cambios; lo que ES específico de Switch es la gestión de
 │                  memoria ejecutable (jitCreate), que vive en platform/switch/
 ├── os/          — cargador ELF, libc-shim, syscall-shim: la lógica de "qué syscalls existen
 │                  y qué hacen" es independiente de Switch; el mapeo final a primitivas reales
 │                  (ficheros, hilos, tiempo) vive en platform/switch/
 ├── graphics/    — traductor de estado GL→genérico + backend deko3d (el backend SÍ es de Switch)
 ├── arcade/      — protocolo JVS + dispositivos virtuales: genérico
 ├── input/       — abstracción de entrada: el backend real (HidNpad) es de Switch
 ├── audio/       — [no cubierto por el MVP inicial, placeholder]
 ├── patch/       — motor de patches sobre IR: totalmente genérico
 ├── hooks/       — infraestructura de hooks de símbolos: genérico
 ├── profiles/    — GameProfile/PlatformProfile: genérico, datos + parser
 └── platform/
      └── switch/ — TODO lo específico de Horizon OS/libnx: jitCreate, HidNpad, deko3d init,
                    filesystem de libnx, entry point (main.cpp)
```

No se han creado abstracciones para plataformas que no existen todavía (no hay `platform/linux/` ni `platform/windows/` — el encargo pide evitar abstracciones innecesarias, y de momento Switch es el único target real).

## 4. Flujo de ejecución (MVP, Lindbergh)

```text
1. platform/switch/main.cpp arranca, inicializa libnx (fs, hid, jit subsystem)
2. Lee profiles/<juego>.json → construye GameProfile (src/profiles/)
3. os/elf_loader/ carga el ELF x86 declarado, resuelve símbolos dinámicos
   contra la tabla de la libc-shim + Cg-shim + GL-shim registradas para
   el PlatformProfile "lindbergh"
4. arcade/devices/eeprom.cpp sintetiza eeprom.bin/sram.bin si no existen en sdmc:/
5. cpu/jit/ empieza a traducir desde el entry point del ELF, bloque a bloque,
   bajo demanda (no traducción estática por adelantado)
6. Cada llamada a gl*/cg* interceptada por graphics/gl_shim/ se traduce a
   comandos deko3d vía graphics/switch/
7. Cada acceso a "dispositivo" arcade (JVS, eeprom) se resuelve en arcade/
8. patch/ aplica los patches/hooks declarados en el GameProfile durante la
   construcción de IR de cada bloque afectado
9. Bucle principal: dispatcher de bloques JIT ↔ presentación de frame vía deko3d
```

## 5. Respuestas a las preguntas de la sección 23 del encargo

| Pregunta | Respuesta con lo investigado |
|---|---|
| ¿Cómo se carga un juego PC arcade? | Para Lindbergh: cargador ELF propio, no hace falta cargador PE/Windows |
| ¿Cómo se ejecuta x86/x64 en ARM64? | DBT/JIT propio con IR intermedia + backend ARM64 reutilizado de Super3-NX (`docs/CPU_TRANSLATION.md`, `docs/JIT.md`) |
| ¿Cómo se proporciona Win32? | **No hace falta para el MVP.** Para el roadmap: capa Win32 mínima construida perfil a perfil, nunca un port de Wine completo (`docs/WINDOWS_COMPATIBILITY.md`) |
| ¿Cómo se traducen los gráficos? | OpenGL+Cg → deko3d, con riesgo abierto en compilación de shaders (`docs/GRAPHICS.md`) |
| ¿Cómo se virtualiza el hardware arcade? | JVS virtual + síntesis de eeprom/sram, sin necesidad de bus serie real (`docs/ARCADE_HARDWARE.md`) |
| ¿Cómo recibe input? | HidNpad de libnx, mapeado por perfil a lo que el JVS virtual expone al juego |
| ¿Cómo se aplican patches? | Motor de patches sobre IR, datos declarados en `GameProfile` (`docs/PATCHING.md`) |
| ¿Cómo se comunica con los dispositivos de Switch? | Vía `platform/switch/`, la única capa que llama directamente a libnx |
| ¿Cómo se consigue un rendimiento aceptable? | **No resuelto todavía — es una hipótesis a validar, no un hecho.** Ver riesgo abierto en `docs/CPU_TRANSLATION.md` y las preguntas del MVP en `docs/ROADMAP.md` |

## 6. Pregunta central del encargo, respondida explícitamente

> ¿Cuál es la arquitectura mínima y más eficiente que permitiría ejecutar juegos arcade PC-based x86/x64 diseñados para Windows en Nintendo Switch ARM64, sin tener que emular innecesariamente un PC completo?

**No hace falta emular un PC completo en ningún caso** (ninguna de las plataformas de la matriz lo requiere, ni siquiera en PC real vía TeknoParrot). Para el subconjunto **no-Windows** (Lindbergh), la arquitectura mínima es: DBT/JIT x86→ARM64 + cargador ELF + libc-shim + traductor gráfico OpenGL/Cg→deko3d + JVS virtual — **sin ninguna capa Win32**. Para el subconjunto Windows (RingEdge en adelante, roadmap), a esa misma base se le añade una capa Win32 mínima construida incrementalmente por perfil de juego, nunca un port completo de Windows/Wine.

> ¿Qué partes de un sistema arcade PC-based son realmente necesarias para que el juego funcione y cuáles podemos sustituir mediante traducción, virtualización, hooks o patches?

Necesario de verdad: la lógica del propio juego (el binario, sin tocar su código salvo patches puntuales) y el runtime gráfico que declara usar (Cg, en el caso Lindbergh). Todo lo demás — la placa base, el "sistema operativo" embebido, la seguridad, el I/O físico — es sustituible por software, como demuestra que `lindbergh-loader` ya lo hace en PC real hoy.
