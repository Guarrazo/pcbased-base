# Plataforma objetivo: Nintendo Switch

## Hardware

| Componente | Dato | Fuente/estado |
|---|---|---|
| SoC | NVIDIA Tegra X1 (T210) | [CONFIRMADO] público, muy documentado en la escena homebrew |
| CPU | 4× ARM Cortex-A57 (cluster "big"), + 4× Cortex-A53 (cluster "little", no expuesto a apps — reservado por Horizon OS) | [CONFIRMADO] |
| CPU freq. (aplicación, docked) | ~1020 MHz para 3 núcleos A57 (el 4º reservado al sistema en config. estándar; overclock homebrew hasta ~1785-2000+ MHz en Erista según silicon lottery) | [CONFIRMADO], overclock es terreno conocido tuyo (Erista HAC-001, recomendaciones por speedo) |
| GPU | NVIDIA Maxwell (GM20B), 256 cores CUDA | [CONFIRMADO] |
| GPU freq. | ~307-384 MHz handheld / ~768 MHz docked (varía por firmware/OC) | [CONFIRMADO] |
| RAM | 4 GB LPDDR4 (Erista) / LPDDR4X (Mariko), ancho de banda ~25.6 GB/s | [CONFIRMADO]; de los 4GB, homebrew dispone típicamente de ~3.2-3.4 GB tras reservas del sistema |
| Almacenamiento | microSD (homebrew) — velocidad muy variable según tarjeta | [CONFIRMADO] |
| Erista vs Mariko | Mismo conteo de núcleos y arquitectura; Mariko (16nm→ en realidad 20nm→16nm FinFET revisado) es más eficiente térmicamente y tolera mejor overclock sostenido; Erista tiene más margen de OC "de pico" pero peor disipación en sesiones largas | [CONFIRMADO], coincide con tu experiencia de overclock |

### Lectura para este proyecto

- Un Cortex-A57 a ~1 GHz-1.8 GHz es **muy modesto** para hacer de anfitrión de una traducción binaria x86→ARM64 con overhead de JIT encima. Point de referencia: Box64/FEX en Cortex-A53 (más lento que A57) ya se consideran "usables pero limitados" para cargas ligeras (ver benchmark de impresoras `printserver.ink` en `STATE_OF_THE_ART.md`) — un shooter de recreativa Lindbergh con lógica de juego relativamente ligera (motor propio de Sega, no un motor moderno tipo UE) es un objetivo razonable; un juego con motor físico pesado o mucha IA no lo es para una primera versión.
- 3.2-3.4 GB de RAM utilizable es generoso para el tamaño de los juegos objetivo (Lindbergh: assets de mediados-de-los-2000, del orden de cientos de MB a pocos GB en disco, no todo en RAM a la vez).
- El ancho de banda de memoria (25.6 GB/s) es la limitación real para gráficos, no el cómputo puro de la GPU: hay que ser conservador con resolución de render interno y con conversiones de formato de textura.

## Horizon OS / libnx / entorno homebrew

- **[CONFIRMADO]** No hay `mmap(PROT_EXEC)` libre para aplicaciones homebrew: el kernel impone W^X. La única vía soportada para memoria ejecutable dinámica es el subsistema `jit` de libnx (`jitCreate`, `jitTransitionToWritable`, `jitTransitionToExecutable`, `jitClose`), que internamente usa `svcMapProcessCodeMemory`/mapeo dual (una región RW y una región RX que apuntan a la misma memoria física). **Ya validado por ti en Super3-NX.**
- **[CONFIRMADO]** No hay señales estilo POSIX (`SIGSEGV`, `sigaction`) en el sentido que usa Box64 para su manejo de excepciones de memoria/páginas. Horizon expone un mecanismo de excepciones distinto (manejadores de excepción de usuario vía `svcSetExceptionUserHandler` / el sistema de crash-handling de libnx). Cualquier técnica que dependa de "trampa por fallo de página + handler" (p. ej. para floats denormales, para detectar código automodificable por protección de página) hay que reimplementarla sobre este mecanismo, no asumir el de Linux.
- **[CONFIRMADO]** No hay `fork()`, no hay proceso-por-juego con memoria virtual arbitraria como en Linux; el naming de threads, sincronización (mutex/condvar/semáforo) y temporizadores es vía libnx (`svc`-based), no pthreads directamente (aunque libnx ofrece una capa pthreads-like limitada).
- **[CONFIRMADO]** Sistema de ficheros: acceso a `romfs`/`sdmc:/` vía FS de libnx, no rutas POSIX arbitrarias tipo `/dev/*`. Cualquier shim que intercepte accesos a "dispositivos" arcade (como hace lindbergh-loader con `/dev/lbb`, `/dev/i2c/0`, `/dev/ttyS0`) tiene que interceptarse a nivel de nuestra propia libc/syscall-translation, no delegarse al FS real de Horizon.
- **[CONFIRMADO]** devkitPro/devkitA64 es la toolchain estándar (GCC cross ARM64 + libnx). El patrón de proyecto habitual es un `Makefile` (plantillas `switch-example`), pero devkitPro también distribuye un toolchain file de CMake (`$DEVKITPRO/cmake/Switch.cmake`) que permite build con CMake — razonable para un proyecto de este tamaño con muchos módulos (más cómodo gestionar dependencias entre `cpu/`, `os/`, `graphics/`, etc. que con un Makefile plano). **Decisión: CMake + toolchain file de devkitPro.**
- **[CONFIRMADO]** No hay caché persistente de código JIT entre ejecuciones garantizada por el sistema — si queremos evitar recompilar bloques calientes en cada arranque (crítico para tiempos de carga aceptables), la caché de bloques traducidos hay que serializarla nosotros a `sdmc:/` y validarla por hash del binario+versión del traductor.

## Límites prácticos que condicionan cada decisión de arquitectura

| Recurso | Límite práctico | Consecuencia de diseño |
|---|---|---|
| CPU | 3-4 núcleos A57 modestos | El propio DynaRec/JIT tiene que ser barato de ejecutar; nada de pases de optimización pesados por bloque en caliente. Presupuesto de hilos: 1 hilo para el juego traducido, 1 para I/O/audio, 1 opcional para compilación JIT en segundo plano (patrón ya usado en Super3-NX si aplica) |
| GPU | Maxwell de gama baja, ancho de banda limitado | Traducción gráfica tiene que evitar conversiones de formato de textura innecesarias y minimizar draw calls (lección que ya sacaste tú mismo en Super3-NX: los binds de textura eran el cuello de botella, no la CPU) |
| RAM | ~3.2-3.4 GB utilizables | Suficiente para el target elegido (Lindbergh), pero descarta de entrada sistemas con requisitos de RAM de PC moderno (Taito Type X3 en adelante, Windows 10 based) |
| Memoria ejecutable | Solo vía `jitCreate`, tamaño de región limitado y fijado al crear el objeto JIT | Hay que dimensionar la caché de código de antemano por perfil de juego (campo `code_cache_size` en `GameProfile`) en vez de crecerla sin límite |
| Almacenamiento | microSD, velocidad variable | Carga de assets grandes (si algún día se aborda un juego con muchos GB) debe ser asíncrona/con streaming, no bloqueante |
| Térmico | Sostenido, sin disipación activa | Sesiones largas de traducción binaria + render son exactamente el peor caso térmico; hay que exponer un perfil de "cap de CPU/GPU" configurable y vigilar throttling, no asumir el reloj máximo indefinidamente |
