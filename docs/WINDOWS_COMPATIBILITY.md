# Compatibilidad Windows / Win32

## Decisión para el MVP: no hace falta ninguna capa Win32

El primer objetivo elegido (Sega Lindbergh, ver `ROADMAP.md`) corre sobre **MontaVista Linux embebido**, no sobre Windows. Los ejecutables son ELF x86 enlazados contra una libc estándar y librerías Sega propietarias (`liblindbergh`-equivalentes) más NVIDIA Cg/OpenGL. Por tanto **el MVP no necesita Win32, ntdll, kernel32 ni Wine en absoluto** — necesita:

1. Un **cargador ELF x86 de 32 bits** minimalista (parsear cabecera ELF, secciones `.text`/`.data`/`.bss`/`.dynamic`, resolver símbolos dinámicos contra nuestra libc-shim, aplicar relocations).
2. Una **libc-shim** que cubra el subconjunto real de syscalls/funciones de libc que usan estos binarios (malloc/free, hilos POSIX básicos si el juego los usa, I/O de fichero redirigido a nuestro sistema de ficheros virtual, `gettimeofday`/temporizadores).
3. Los shims específicos de Lindbergh descritos en `ARCADE_HARDWARE.md` (equivalentes a lo que hace `lindbergh.so`).

Esto es una superficie muchísimo más pequeña que Win32 y evita el mayor riesgo del proyecto completo (portar o reimplementar Windows).

## Roadmap posterior: cuándo sí hace falta Win32

Los sistemas basados en Windows real (RingEdge/RingWide con Windows Embedded Standard 2009, Taito Type X con Windows XP, Type X2/X3 con Windows 7/8/10 — ver matriz en `docs/ARCHITECTURE.md`) sí requieren una capa Win32, para cualquier fase posterior a la validación del MVP. Investigación relevante:

- **[CONFIRMADO]** Wine es la única implementación de Win32 completa y madura que existe, pero está diseñada para un huésped tipo Unix (Linux/macOS/BSD) con syscalls POSIX, `mmap` arbitrario, señales, `fork()`, hilos POSIX. Horizon OS no ofrece nada de eso de forma nativa.
- **[CONFIRMADO]** Wine 8.8+ introdujo carga de módulos ARM64EC (ABI híbrido de Microsoft para Windows-on-ARM) y Wine 10 (2025) tiene soporte más completo; Wine 11.0 (enero 2026) es la última estable. Todo esto está pensado para sistemas Windows-on-ARM o Linux/macOS ARM64 reales — **no hay ningún trabajo público de portar Wine a un SO homebrew de consola**.
- **[INFERENCIA]** Portar Wine completo a Horizon es, por tamaño de codebase (millones de líneas, ~30 años de desarrollo) y por la cantidad de asunciones de kernel Unix que tiene incrustadas, un proyecto que fácilmente supera en esfuerzo a todo lo demás de este documento junto. **Se descarta explícitamente como estrategia**, no solo para el MVP sino como aproximación general.
- **Alternativa recomendada quando llegue el momento: una "Win32 compatibility layer mínima" propia**, al estilo de lo que hacía Wine en sus primeros años (no reimplementar todo el SO, solo las ~200-300 funciones de `kernel32`/`user32`/`ntdll` que un juego arcade concreto realmente llama — esto se descubre por perfil de juego, no a priori). Esto es coherente con el patrón `GameProfile` de la sección 11: cada perfil puede declarar qué subconjunto de Win32 necesita, y la capa crece perfil a perfil, nunca "de golpe".

## Qué reutilizar cuando se aborde esta fase

- **[INFERENCIA]** El diseño de `so-loader` que ya conoces (cargador que sustituye el runtime de origen — ahí Android/Bionic — por shims propios sobre libnx) es el mismo patrón que hace falta aquí para PE/Win32: un cargador de **PE** (en vez de ELF) que resuelve imports contra una tabla de funciones Win32 propia en vez de contra DLLs reales.
- No implementar `ntdll`/`kernel32` genéricos de entrada: implementar solo lo que un juego Windows arcade concreto (p. ej. un título RingEdge sencillo) llama, verificado con listado de imports del PE real, exactamente igual que se hizo con el subconjunto de PowerPC realmente usado en Super3-NX en vez de todo el ISA.

## Explícitamente fuera de alcance de este documento

No se ha investigado en profundidad DirectInput/XInput/COM/DirectShow porque son secundarios hasta que exista un target Windows concreto seleccionado; se retoman en la fase correspondiente del roadmap con un perfil de juego real delante, no de forma especulativa.
