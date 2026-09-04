# JIT / DBT para Switch

## Pipeline

```text
x86 code (32-bit, Lindbergh)
      ↓
Decoder (src/cpu/x86/)           — decodifica solo el subconjunto real usado (ver CPU_TRANSLATION.md)
      ↓
IR builder (src/cpu/translator/) — IR propia, inspirada en el diseño de FEX
      ↓
Patch Engine hooks aplicados aquí (docs/PATCHING.md) — transformación sobre la IR, no sobre bytes
      ↓
Optimizador ligero (src/cpu/translator/) — mínimo: propagación de flags (Kildall-style, igual que hace Box64),
                                            eliminación de instrucciones muertas obvias. NADA de optimizaciones
                                            caras: presupuesto de CPU es el recurso más escaso (ver SWITCH_PLATFORM.md)
      ↓
ARM64 backend (src/cpu/arm64/)   — emisor propio, validado por contraste contra un
                                    ensamblador ARM64 real (ver más abajo)
      ↓
Code cache (src/cpu/jit/)        — memoria vía jitCreate/jitTransitionToWritable/jitTransitionToExecutable
                                    (patrón ya validado en Super3-NX, incluye BRK #0xDEAD como canario)
      ↓
Ejecución
```

## Estado de implementación (esta sección se actualiza según avanza)

| Etapa | Estado |
|---|---|
| Decoder x86 | Subconjunto real implementado y probado (PUSH/POP/MOV/LEA/Grupo1/CALL/RET/LEAVE) — ver `docs/CPU_TRANSLATION.md` |
| IR builder | **Implementado para el subconjunto registro+inmediato** (`src/cpu/translator/ir_builder.cpp`): MOV reg↔reg/inmediato, Grupo1 (ADD/SUB/AND/OR/XOR/CMP) reg,inmediato, RET. Lowering directo sin dataflow entre instrucciones (cada operando hace su propio `LoadReg`, ver comentario de cabecera del fichero) — suficiente porque `Arm64CodeGen` trata los registros x86 como "pinned" (ver más abajo), así que un `LoadReg` no cuesta ninguna instrucción real. **No soporta todavía ningún operando de memoria** (LEA, PUSH/POP reales, MOV con memoria) — se registra como error explícito, no se ignora |
| Patch Engine sobre IR | Interfaz lista (`ApplyPatches()`), sin patches reales que aplicar todavía (ningún `GameProfile` real los declara aún) |
| Optimizador | No implementado — ni falta hasta que haya bloques con dataflow real que optimizar |
| ARM64 backend | Implementado y **triplemente validado**: contra `aarch64-linux-gnu-as`, contra `switch_ppc_jit_arm64.h` (coinciden bit a bit), y por **ejecución real** bajo `qemu-aarch64` (ver más abajo) |
| Code generator (IR→ARM64) | **Implementado** (`src/cpu/jit/arm64_codegen.h/.cpp`) para el mismo subconjunto que el IR builder — ver "Convención de registros" más abajo |
| Code cache | `Insert()` ahora copia bytes de verdad (antes solo reservaba espacio) — ver `src/cpu/jit/code_cache.cpp` |
| `Jit::TranslateBlock` | **Conectado de extremo a extremo**: decodifica hasta la primera instrucción de control de flujo (RET/CALL/JMP/Jcc, ahí termina el bloque), construye IR, genera ARM64, inserta en la cache |
| `Jit::RunFrom` (bucle de dispatch) | **No implementado** — traduce el bloque de entrada y se detiene; no salta al código generado ni seguiría la ejecución más allá de un bloque. Es el siguiente paso obvio, pero requiere antes decidir la convención de llamada entre bloques (qué registro dice "a dónde saltar después") |

## Convención de registros ARM64 (asignación fija, no dinámica)

Decisión de diseño para el generador de código (`Arm64CodeGen`): cada registro x86 vive siempre en el mismo registro ARM64 mientras el JIT está corriendo — no hay asignación de registros dinámica por bloque. Es la misma clase de decisión que el "modo estático" de Box64 (ver `docs/STATE_OF_THE_ART.md`): más simple y predecible a costa de no poder mantener valores temporales en registro entre bloques.

| Registros ARM64 | Uso |
|---|---|
| `W20`-`W27` (`X20`-`X27`) | `EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI` (orden ModRM 0-7, ver `cpu::x86::Reg`) |
| `X28` | "Translation base": `host_ptr = X28 + guest_virtual_addr` — reservado para cuando el IR builder soporte operandos de memoria (no usado todavía, ver tabla de arriba) |
| `X9`-`X15` | Scratch, libres de usar dentro de un bloque, no se preservan entre bloques |
| `X30`/LR | Dirección de retorno de la llamada `BLR` que invocó el bloque — cada bloque termina en `RET`, que vuelve ahí |

Por qué W20-W27 y no W0-W7: los registros `X19`-`X28` son *callee-saved* según AAPCS64 — si en algún momento el dispatcher del JIT necesita llamar a una función C++ normal (p. ej. un hook de `LibcShim`) sin perder el estado de los registros x86 pineados, esa función puede hacerlo sin instrucción adicional siempre que respete la ABI estándar (que ya garantiza que no toque `X19`-`X28`). Esto no está aprovechado todavía (no hay ninguna llamada nativa implementada) pero es la razón de la elección, no un accidente.

`LoadReg`/`StoreReg` en la IR se traducen así: leer un registro x86 no cuesta ninguna instrucción ARM64 (el `Value` simplemente *es* el registro pineado); escribir sí cuesta una instrucción (`MOV`, vía `ORR Wd,WZR,Wn`) salvo que el valor a guardar ya esté físicamente en el registro pineado correcto (caso no optimizado todavía, ver tabla de arriba).

## Validación por ejecución real: tres niveles

1. **`tests/test_arm64_emitter.cpp`** — cada instrucción ARM64 individual, codificación correcta (bits), contra `aarch64-linux-gnu-as`.
2. **`tests/test_arm64_dynamic_exec.cpp`** — secuencias de instrucciones sueltas, **ejecutadas de verdad** bajo `qemu-aarch64`, resultado numérico comprobado.
3. **`tests/test_ir_end_to_end.cpp`** y **`tests/test_jit_integration.cpp`** — el pipeline **completo** (decode x86 → IR → ARM64 → ejecución), comparado **contra la ejecución nativa del x86 original** (Linux ejecuta binarios ia32 estáticos directamente en este host x86-64, sin emulación) para 11 valores de entrada distintos, incluyendo `INT_MIN`/`INT_MAX`. `test_jit_integration.cpp` además pasa por la clase `Jit` real (`RunFrom`), no piezas sueltas.

Las tres capas son opcionales (se saltan con aviso si no hay `gcc-multilib`/`gcc-aarch64-linux-gnu`/`qemu-user` instalados) pero están todas verdes en el entorno de desarrollo actual.

## Validación dinámica del emisor ARM64 (ejecución real, no solo codificación)

`tests/test_arm64_emitter.cpp` comprueba que cada instrucción se **codifica** con los bits correctos (contrastado contra `aarch64-linux-gnu-as` y contra `switch_ppc_jit_arm64.h`, ver más abajo). Eso no detecta un error semántico en una *secuencia* de instrucciones bien codificadas por separado (p. ej. operandos en el orden equivocado de una resta, un registro reutilizado sin querer).

Para eso existe `tests/test_arm64_dynamic_exec.cpp`: genera una secuencia completa con el `Emitter`, la escribe a un fichero, y la **ejecuta de verdad** en una CPU ARM64 mediante:
- `tests/fixtures/arm64_exec_harness.c` — un programa ARM64 real (compilado con `aarch64-linux-gnu-gcc`, no cross-compilado desde C++ del proyecto) que mmapea el código como ejecutable, lo invoca como `int64_t(*)(int64_t)`, e imprime el resultado.
- `qemu-aarch64` (qemu-user) — ejecuta ese binario ARM64 sobre el host x86-64 de desarrollo. El código máquina que corre es exactamente el mismo que correría en hardware ARM64 real; lo único emulado es la CPU anfitriona, no el ISA en sí.

Esto ya encontró y confirmó correcto un flujo real: `(x0+5-2) & 0xF`, `(x0*6)/4`, y un roundtrip `STUR`+`LDUR` sobre `[sp,#-8]` — los tres devuelven el valor exactamente esperado al ejecutarse. Es opcional (se salta con un aviso si el host no tiene `gcc-aarch64-linux-gnu`/`qemu-user` instalados, igual que el fixture ELF32 real de `docs/ROADMAP.md`) pero es la validación más fuerte disponible para este emisor: más allá de "¿el bit pattern es correcto?", responde "¿la CPU calcula lo que se pretendía?".

Este mismo mecanismo (`RunOnRealArm64()` en el test) es reutilizable para validar bloques completos generados por `IrBuilder`/`Jit` más adelante, no solo secuencias de prueba sueltas.

## Qué se reutiliza literalmente de Super3-NX y qué es nuevo

| Componente | Origen |
|---|---|
| Emisor de instrucciones ARM64 a nivel de bit | **Implementado y doblemente validado** (`src/cpu/arm64/emitter.{h,cpp}`). Primero contra `aarch64-linux-gnu-as` (ensamblador ARM64 real de GNU binutils): decenas de combinaciones de registros/inmediatos, comparadas bit a bit con `objdump -d`. Después, al recibir `switch_ppc_jit_arm64.h` real de Super3-NX, se contrastaron ambas implementaciones formula a formula (LDR/STR/LDUR/STUR con offset escalado y sin escalar, LDP/STP, BLR, B, B.cond, ADD/SUB, MOVZ/MOVK/MOVN, ADD/SUB/CMP inmediato, AND/ORR/EOR, LSL/LSR/ASR, MUL/UDIV/SDIV) y **coinciden bit a bit en todo lo comprobado** — dos derivaciones completamente independientes llegando al mismo resultado. El propio proceso de portar las nuevas instrucciones cazó un bug real: las primeras versiones de `EmitAddImm`/`EmitAndReg`/etc. rechazaban `rd=31` (registro cero/SP) como "fuera de rango", lo que rompía `EmitCmpImm` (que es `SUBS` con `Rd=31`) — corregido y cubierto por test (`TestAddSubCmpImmediate`). Cubre `Xn`/`Wn` (0-31 en las instrucciones aritmético/lógicas; 0-30 en LDR/STR/LDP/STP/BLR, que todavía no soportan direccionamiento vía SP); no cubre registros vectoriales (`Vn`) todavía. El fichero original de Super3-NX usa un patrón más seguro que el de aquí — registros tipados (`XReg`/`WReg` como enums en vez de `uint8_t` plano) que impiden mezclar accidentalmente un registro de 32 y otro de 64 bits — y también una capa de validación estructural de opcodes (`ValidateOpcode()`, que detecta familias enteras de encodings inválidos por regla general en vez de caso a caso); ninguna de las dos cosas se ha adoptado aquí todavía para no romper la interfaz ya cubierta por tests justo antes de empezar el decodificador x86, pero valen la pena como mejora futura. |
| Gestión de memoria ejecutable Horizon-safe (`jitCreate` etc.) | **Escrito contra la API pública**, sin verificar todavía contra una compilación real de devkitA64 (ver nota de honestidad en `platform/switch/jit_memory.h`) |
| Detección de overflow del emisor | **Implementada** (`Emitter::Emit32`, con test específico bajo ASan/UBSan) — mismo hallazgo que motivó el fix original en Super3-NX |
| Canario `BRK #0xDEAD` | El propio `Emitter::EmitBrk()` lo soporta (`EmitBrk(0xDEAD)`), pero todavía no hay código en `code_cache.cpp` que lo inserte automáticamente al final de cada bloque -- pendiente |
| Decodificador de instrucciones | **Primer subconjunto implementado y probado** (`src/cpu/x86/decoder.{h,cpp}`) — PUSH/POP r32, PUSH r/m32, MOV, LEA (con SIB), Grupo 1 con imm8/imm32, CALL rel32, RET, LEAVE. Validado decodificando byte a byte una función real compilada con GCC -m32 (`tests/test_x86_decoder.cpp`, 41 instrucciones, cuadre exacto de longitud total). Pendiente: Jcc/JMP, SSE/SSE2/x87 (estos últimos imprescindibles para Lindbergh, no cubiertos por el fixture genérico actual) |
| IR intermedia | **Definida** (`src/cpu/translator/ir.h`) pero `IrBuilder::BuildBlock()` sigue sin implementar — ahora sí hay `X86Instruction` reales con los que construirla, en vez de solo la definición de datos. Super3-NX traduce más directamente PPC→ARM64 sin una IR explícita separada; aquí se introduce IR porque x86 tiene una superficie de instrucciones más irregular (prefijos, modos de direccionamiento variables) donde una IR normaliza mejor el trabajo del backend |
| Mapeo de flags | **Sin implementar** — los flags de x86 (`ZF`,`CF`,`SF`,`OF`,`PF`,`AF`) no se corresponden 1:1 con `NZCV` de ARM64; hace falta la misma clase de análisis de "qué flags usa realmente la siguiente instrucción" que ya usa Box64 (evita calcular flags que ningún código posterior consulta) |
| Validación diferencial (bloque por bloque, intérprete vs JIT) | **Patrón a reutilizar** de `switch_ppc_jit_diff.cpp/h` — se construirá el equivalente para x86 cuando exista un intérprete de referencia; de momento la validación del emisor es contra el ensamblador real (ver arriba), que cubre "¿el encoding es correcto?" pero no "¿el bloque completo hace lo mismo que el x86 original?" |

## Tamaño y linkado de bloques

- Un bloque termina en salto, llamada, retorno o límite de tamaño configurable (empezar conservador: bloques pequeños, más fáciles de depurar e invalidar granularmente; ampliar después de medir en hardware real, no antes).
- Block linking (saltos entre bloques ya traducidos sin volver al dispatcher) se implementa **después** de tener el MVP correcto con dispatcher simple — es una optimización, no un requisito de correctitud, y añade complejidad de invalidación (un bloque enlazado a otro que se invalida tiene que desenlazarse).

## Invalidación de bloques (self-modifying code)

- **Riesgo heredado y ya identificado en Super3-NX** (`PpcJitInvalidate()` sin call site, marcado P0 allí): el x86 JIT nuevo debe tener este mecanismo diseñado desde el principio, no como añadido posterior. Cada escritura de memoria del juego (interceptada en la IR de bloques de escritura, o vía protección de página si el patrón de escritura es impredecible) comprueba si la página de destino tiene bloques traducidos activos y los invalida.
- Para el target Lindbergh el riesgo real de self-modifying code es bajo (motor de juego de mediados-2000, no packers/protecciones agresivas tipo los sistemas de DRM de placas posteriores) pero el mecanismo se implementa igualmente porque es barato de tener desde el diseño y caro de añadir después.

## Caché de código persistente

- Serializar bloques traducidos a `sdmc:/arcade/<juego>/jit_cache/` entre ejecuciones, invalidada por hash del ELF + versión del traductor (campo interno, no visible en el `GameProfile`). Reduce tiempos de carga en ejecuciones repetidas — importante en un dispositivo donde recompilar en cada arranque en un Cortex-A57 puede no ser instantáneo.

## Perfilado

- Contador simple de ejecuciones por bloque, sin overhead de timestamp por ejecución (demasiado caro en A57) — umbral de "bloque caliente" basado en conteo, no en tiempo, para decidir cuándo vale la pena el linkado de bloques cuando se implemente.
