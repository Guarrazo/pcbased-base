# Traducción de CPU: x86/x86-64 → ARM64

## Decisión

**No se porta Box64 ni FEX tal cual. Se escribe un DBT/JIT propio para Switch, con FEX como referencia de arquitectura (IR propia) y con el emisor ARM64 y el patrón de memoria ejecutable de Super3-NX reutilizados directamente.**

Justificación, comparando las 5 opciones que pedía el encargo:

1. **Adaptar Box64** — descartado como base de código directa. Box64 asume glibc, `mmap`/`mprotect` arbitrarios, señales POSIX y un layout de proceso Linux; nada de eso existe en Horizon OS. Habría que reescribir toda la capa de sistema y buena parte del manejo de memoria/excepciones — en la práctica equivale a reescribirlo, no a "adaptarlo". Se conserva como **referencia de diseño del decodificador x86 y de las pasadas del DynaRec** (documentadas públicamente, no su código con licencia MIT reutilizado literalmente salvo que se audite compatibilidad de licencia).
2. **Adaptar FEX** — mismo problema de dependencia de Linux, agravado porque FEX está más integrado con el kernel Linux (usa `ptrace`-like mecanismos y gestión de hilos más sofisticada). Se descarta como base de código por el mismo motivo. Se conserva como **referencia de arquitectura IR** (ver más abajo, es la pieza de su diseño que sí vale la pena copiar conceptualmente).
3. **Integrar otro DBT** (QEMU TCG, DynamoRIO) — descartado. QEMU TCG apunta a emulación de sistema completo o usuario Linux, con un frontend/backend genérico pensado para muchísimas arquitecturas a la vez; el overhead de abstracción no compensa en un target tan limitado como Cortex-A57. DynamoRIO es una infraestructura de instrumentación binaria pensada para x86-en-x86, no para cross-arquitectura de producción con estas restricciones de memoria.
4. **Desarrollar uno propio** — **elegido**, por descarte de las anteriores y porque:
   - El coste marginal de escribir un decodificador x86 (32 bits primero, target Lindbergh) es acotado: el subconjunto de instrucciones que emiten compiladores de la época (GCC de mediados-2000 para juegos Lindbergh) es mucho menor que "todo x86".
   - **Ya tienes el componente más difícil de reutilizar hecho y depurado**: el emisor de instrucciones ARM64 a nivel de bit (encodings de LDR/STR, BLR, LDP/STP corregidos en `switch_ppc_jit_arm64.h`) y el patrón de memoria ejecutable Horizon-safe (`jitCreate`/`jitTransitionToWritable`/`jitTransitionToExecutable`, con el canario `BRK #0xDEAD` y detección de overflow de `Arm64Emitter::Emit32()`) son **agnósticos del ISA de origen**. Ese backend se reutiliza casi literal; lo único nuevo es el frontend (decodificador x86) y el mapeo de registros/flags x86→ARM64.
   - Permite integrar el motor de patches/hooks (sección `PATCHING.md`) directamente en el pipeline de traducción, en vez de como una capa añadida después.
5. **Combinar varias soluciones** — es efectivamente lo que se hace: arquitectura de FEX (decodificar → IR → optimizar → backend ARM64) + backend/gestión de memoria de Super3-NX + decodificador propio acotado al subconjunto real usado por los binarios objetivo.

## Por qué IR propia y no traducción directa instrucción-a-instrucción

Box64 traduce mayormente bloque a bloque sin una IR explícita separada; FEX sí tiene una IR. Para este proyecto interesa la IR por dos motivos que no son solo "rendimiento":

- **Punto de enganche único para el Patch Engine y los hooks**: un patch de memoria o un hook de función se puede expresar como una transformación sobre la IR de un bloque concreto (reemplazar/insertar nodos IR) en vez de tener que parchear bytes ARM64 ya generados o (peor) bytes x86 antes incluso de decodificar. Esto es justo lo que pide la sección 12 del encargo ("debe ser especialmente eficiente en ARM64").
- **Aislar la parte "cara" (decodificación x86 + construcción IR) de la parte "barata pero repetida muchas veces" (emisión ARM64)**: en un Cortex-A57 interesa que el camino caliente (ejecución de bloques ya traducidos) no pague overhead de abstracción; con IR, la complejidad se paga una vez por bloque, no en cada ejecución.

## Alcance de la CPU x86 a decodificar (target: Lindbergh, ver ROADMAP.md)

- **[CONFIRMADO]** CPU real de Lindbergh: Intel Pentium 4 (Lindbergh Yellow, 3.0 GHz) o Celeron (Lindbergh Red, 2.8 GHz) — **x86 de 32 bits**, generación Pentium 4/Netburst con SSE2 (Pentium 4 introduce SSE2 nativo; SSE3 llega con revisiones posteriores).
- Extensiones a decodificar en el MVP: x86 base de 32 bits, SSE/SSE2 (imprescindible, compiladores de la época ya las generan para FP), x87 (todavía usado por código legacy), CPUID/RDTSC (hay que virtualizarlos: CPUID debe reportar un "Pentium 4 genérico" creíble o lo que el juego concreto espere; RDTSC hay que mapearlo a un contador de Switch con la escala de frecuencia correcta, no al reloj real de la CPU host, o el timing del juego se desincroniza).
- Explícitamente **fuera de alcance para el MVP**: AVX/AVX2/AVX-512 (no existen en hardware de esta generación), x86-64/instrucciones REX (Lindbergh es 32 bits — esto es una simplificación real, no falta de rigor: **no hace falta Box32 ni nada equivalente a "box64 en modo 32 bits"**, se decodifica x86 de 32 bits de raíz).
- Self-modifying code y generación dinámica de código por parte del propio juego: hay que soportar invalidación de bloques traducidos cuando el juego escribe sobre memoria que ya se tradujo (motivo por el que en Super3-NX se señaló `PpcJitInvalidate()` como P0 pendiente — mismo problema, aplica igual aquí).

## Riesgo técnico abierto

**[DESCONOCIDO]** No hay ningún dato público de rendimiento de un DynaRec x86→ARM64 corriendo específicamente en Cortex-A57 a las frecuencias de Switch. Los benchmarks públicos de Box64/FEX son sobre Cortex-A72/A76/A78 o superiores. Esto es el riesgo número uno del proyecto y solo se resuelve con medición propia sobre el primer bloque de código traducido — ver `docs/ROADMAP.md`, "Preguntas que el MVP tiene que responder".
