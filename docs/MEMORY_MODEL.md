# Modelo de memoria

## El problema

x86 tiene un modelo de memoria fuerte (TSO — Total Store Order): en la práctica, casi todas las reordenaciones que un compilador/programador "no espera" están prohibidas por hardware. ARM64 tiene un modelo de memoria débil: el hardware puede reordenar lecturas/escrituras salvo que se usen instrucciones de barrera explícitas (`DMB`, `DSB`, operaciones `LDAR`/`STLR`). Box64 y FEX resuelven esto con barreras configurables (desde "ninguna, más rápido pero puede fallar en código muy dependiente de threading" hasta "TSO estricto, más lento", ver `STATE_OF_THE_ART.md`).

## Decisión para el MVP

Los juegos Lindbergh objetivo del MVP son de un solo hilo de juego principal con hilos auxiliares limitados (audio, I/O) y **no** son código con sincronización fina agresiva tipo motor multihilo moderno. Se adopta el nivel de barrera **más barato que sea seguro**, configurable por perfil (`compatibility_flags: ["strict_memory_model"]` como opt-in, no por defecto), en vez de emitir barreras completas en cada acceso a memoria compartida — exactamente el mismo trade-off que exponen Box64/FEX como opción de usuario, pero aquí decidido por perfil de juego, dato, no interruptor global.

## Atomics

Las operaciones `LOCK`-prefixed de x86 (incrementos atómicos, CAS) se traducen a las instrucciones atómicas nativas de ARMv8 (`LDXR`/`STXR` o, si el core lo soporta, LSE `CAS`/`SWP` — Cortex-A57 es ARMv8.0, sin extensión LSE, así que se usa el par `LDXR`/`STXR` con reintento). Esto es una decisión de bajo riesgo porque el conjunto de operaciones atómicas x86 realmente emitido por compiladores de la época es pequeño y bien conocido.

## TLS (Thread-Local Storage)

x86 en Linux resuelve TLS vía el segmento `%gs`/`%fs` con una tabla gestionada por el sistema (`set_thread_area`). En nuestro entorno no hay ese syscall — el cargador ELF-shim (`src/os/elf_loader/`) reserva un bloque TLS por hilo Switch (vía las primitivas de hilos de libnx) y reescribe los accesos `%fs:offset` detectados en la IR para que apunten a ese bloque, en vez de emular el syscall real de configuración de segmento.

## Excepciones y floating point

Los flags de excepción FP de x86 (denormales, división por cero) generalmente no se comprueban activamente por juegos de esta época salvo casos concretos — se documentará por perfil si aparece un caso real; no se implementa comprobación de excepciones FP completa en el MVP salvo necesidad demostrada por un juego concreto.

## Self-modifying code

Ver `docs/JIT.md`, sección de invalidación de bloques — es un problema del JIT, no del modelo de memoria en sí, pero está relacionado porque la detección de escritura sobre páginas con código ya traducido requiere saber, para cada escritura de memoria del juego, si esa página tiene bloques traducidos activos.
