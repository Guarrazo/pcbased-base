# AUDITORÍA TÉCNICA EXHAUSTIVA
## PC-Arcade-Switch: Compatibility Layer para Sega Lindbergh

**Fecha**: 2026-09-04  
**Ingeniero Principal**: Auditoría arquitectónica completa  
**Target**: Sega Lindbergh (x86-32, Linux, OpenGL+Cg) → Nintendo Switch (ARM64)

---

## RESUMEN EJECUTIVO

### Veredicto Arquitectónico

**El diseño fundamental del proyecto es CORRECTO y bien fundamentado.**

Este NO es un emulador de PC completo, sino un **compatibility layer / traductor binario dinámico (DBT)** específicamente diseñado para sistemas arcade PC-based. La arquitectura elegida es la apropiada para el problema y el hardware objetivo (Nintendo Switch).

**Estado actual**: Esqueleto arquitectónico sólido con ~30-40% del MVP Nivel 1 implementado y validado. La mayoría de componentes críticos están diseñados pero no implementados.

### Hallazgos Principales

✅ **FORTALEZAS**:
- Arquitectura DBT correcta (similar a Box64/FEX, no emulación completa)
- Separación core/platform bien diseñada
- Pipeline JIT x86→IR→ARM64 implementado y validado end-to-end para subconjunto básico
- ELF loader funcional con tests exhaustivos
- ARM64 backend validado contra herramientas reales y ejecución bajo QEMU
- Sistema de tests robusto (validación diferencial x86 nativo vs ARM64 traducido)
- Documentación técnica excepcional

❌ **DEFICIENCIAS CRÍTICAS**:
- **Accesos a memoria no implementados** (bloquea TODO código real)
- **Libc shim completamente vacía** (sin malloc, I/O, syscalls)
- **Graphics backend stub completo** (sin OpenGL/Cg→deko3d)
- **JVS/EEPROM stubs** (sin datos sintéticos de Lindbergh)
- **Dispatcher del JIT incompleto** (traduce pero no ejecuta bloques enlazados)
- **x87/SSE/SSE2 no decodificados** (necesarios para código Lindbergh real)

### Prioridades Inmediatas

**P0 - BLOCKERS FUNDAMENTALES**:
1. Implementar traducción de accesos a memoria (LoadMem/StoreMem en IR + codegen)
2. Diseñar arquitectura de memoria guest (X28 base register según docs/MEMORY_ARCHITECTURE_ANALYSIS.md)
3. Implementar libc-shim mínima (malloc/free, open/fopen, gettimeofday)
4. Completar dispatcher con context switching de registros x86

**P1 - MVP NIVEL 1**:
5. Decodificador x86: LEA, PUSH/POP reales, Jcc/JMP, CALL con memoria
6. x87 básico (FLD/FST/FADD/FMUL)
7. SSE/SSE2 mínimo (MOVSS/MOVSD/ADDPS/MULPS)
8. EEPROM/SRAM synthesis según lindbergh-loader

---

## 1. CLASIFICACIÓN DE COMPONENTES

### 1.1 Componentes PC Genéricos (Reutilizables)

Estos componentes NO dependen de Lindbergh específicamente y podrían reutilizarse para otras plataformas PC-based arcade:

| Componente | Ubicación | Estado | Justificación |
|------------|-----------|--------|---------------|
| **ELF32 Loader** | `src/os/elf_loader/` | ✅ **IMPLEMENTADO** | Estándar ELF32, funciona para cualquier binario i386 Linux |
| **x86-32 Decoder** | `src/cpu/x86/decoder.{h,cpp}` | 🟡 **PARCIAL** (30%) | ISA estándar x86, no específico de Lindbergh |
| **IR (Intermediate Representation)** | `src/cpu/translator/ir.h` | ✅ **COMPLETO** | Agnóstico de plataforma origen/destino |
| **ARM64 Emitter** | `src/cpu/arm64/emitter.{h,cpp}` | ✅ **VALIDADO** | ISA estándar ARMv8-A, reutilizable para cualquier target ARM64 |
| **JIT Framework** | `src/cpu/jit/jit.{h,cpp}` | 🟡 **PARCIAL** (60%) | Orquestación genérica decode→IR→codegen |
| **Code Cache** | `src/cpu/jit/code_cache.{h,cpp}` | ✅ **FUNCIONAL** | Gestión de bloques traducidos, genérica |
| **Patch Engine** | `src/patch/` | ✅ **DISEÑADO** | Transformaciones sobre IR, no específico de juego |
| **Profile System** | `src/profiles/` | ✅ **FUNCIONAL** | Formato JSON declarativo, extensible |
| **Core Utilities** | `src/core/` | ✅ **COMPLETO** | Logging, config, module registry |

**Conclusión**: La base del DBT es **correctamente genérica**. No hay acoplamiento incorrecto a Lindbergh en el core.

### 1.2 Componentes Lindbergh-Específicos

Estos dependen de características específicas de Sega Lindbergh:

| Componente | Ubicación | Estado | Dependencia |
|------------|-----------|--------|-------------|
| **EEPROM Synthesizer** | `src/arcade/devices/eeprom.{h,cpp}` | ❌ **STUB** | Layout `amSysDataRecord` específico de Lindbergh |
| **JVS Protocol** | `src/arcade/jvs/virtual_jvs.{h,cpp}` | ❌ **STUB** | Protocolo JVS (común a múltiples arcade, pero mapeo específico) |
| **Platform Profile** | `profiles/lindbergh.platform.json` | ⚠️ **PENDIENTE** | Configuración de hardware Lindbergh |
| **OpenGL/Cg Shim** | `src/graphics/gl_shim/` | ❌ **STUB** | Superficie API específica de runtime Lindbergh (Cg 3.1) |

**Hallazgo**: Separación correcta. Lo Lindbergh-específico está en `arcade/devices/`, `profiles/`, y en el shim de OpenGL/Cg (que es específico del runtime, no del ISA).

### 1.3 Componentes Switch-Específicos

Solo estos dependen de Nintendo Switch / Horizon OS:

| Componente | Ubicación | Estado | Dependencia |
|------------|-----------|--------|-------------|
| **JIT Memory** | `src/platform/switch/jit_memory.{h,cpp}` | ✅ **IMPLEMENTADO** | `jitCreate/jitTransitionTo*` de libnx |
| **HID Input** | `src/platform/switch/input_hid.{h,cpp}` | ❌ **STUB** | `HidNpad*` de libnx |
| **Deko3d Backend** | `src/platform/switch/graphics_deko3d.{h,cpp}` | ❌ **STUB** | deko3d (wrapper de NVN) |
| **Main Entry Point** | `src/platform/switch/main.cpp` | 🟡 **ESQUELETO** | `consoleInit`, libnx init |

**Hallazgo**: Aislamiento perfecto. **Solo 4 archivos incluyen `<switch.h>`**, cumpliendo exactamente la separación core/platform documentada.

### 1.4 Verdict de Arquitectura

✅ **NO hay acoplamiento incorrecto**  
✅ **La separación PC-generic / Lindbergh-specific / Switch-specific es clara y correcta**  
✅ **Un futuro port a otra plataforma PC-based (RingEdge, Taito Type X) reutilizaría >70% del código**

---

## 2. ANÁLISIS DETALLADO POR SUBSISTEMA

### 2.1 CPU: Traducción x86→ARM64

#### 2.1.1 Decoder x86

**Archivo**: `src/cpu/x86/decoder.cpp`  
**Estado**: 🟡 **PARCIAL - 30% del MVP**

**Implementado y Validado**:
```
✅ PUSH r32 (0x50-0x57)
✅ POP r32 (0x58-0x5F)
✅ PUSH r/m32 (0xFF /6)
✅ MOV r32,r32 / r32,r/m32 / r/m32,r32 / r32,imm32 (0x89/0x8B/0xC7/0xB8-0xBF)
✅ MOV moffs (0xA1/0xA3) - direccionamiento absoluto
✅ LEA r32, [m] (0x8D) - con SIB completo
✅ Grupo 1 (ADD/OR/ADC/SBB/AND/SUB/XOR/CMP) con imm8/imm32 (0x80/0x81/0x83)
✅ CALL rel32 (0xE8)
✅ RET (0xC3)
✅ LEAVE (0xC9)
```

**Validación**:
- Decodificado 41 instrucciones reales de un binario compilado con GCC -m32
- Comparado byte a byte contra `objdump -d -M intel`
- Cuadre exacto de longitud total (tests/test_x86_decoder.cpp)

**FALTA (bloqueante para binarios reales)**:
```
❌ JMP rel8/rel32 (0xEB/0xE9)
❌ Jcc (0x70-0x7F near, 0x0F 0x80-0x8F far) - saltos condicionales
❌ CALL r/m32 (0xFF /2) - llamadas indirectas
❌ x87 FPU (0xD8-0xDF) - CRÍTICO para Lindbergh
❌ SSE/SSE2 (0x0F ...) - CRÍTICO para Lindbergh
❌ Prefijo 0x66 (operandos de 16 bits)
❌ Prefijo REP (0xF3) para string ops
❌ ModRM mod=3 sobre Grupo 1 (registro-a-registro, no memoria)
```

**Causa Raíz**: El decoder se implementó contra un fixture sintético simple que no ejercita control de flujo complejo ni FPU/SIMD. Es correcto para lo que cubre, pero el alcance es insuficiente.

**Estimación de Cobertura Real**: Un binario Lindbergh típico probablemente usa:
- x87: ~15-25% de instrucciones (código FP legacy)
- SSE/SSE2: ~10-20% (código FP moderno)
- Saltos condicionales: ~10-15%
- **Cobertura actual del decoder: ~30-40% de un binario real**

#### 2.1.2 IR Builder

**Archivo**: `src/cpu/translator/ir_builder.cpp`  
**Estado**: 🟡 **PARCIAL - Operandos de registro/inmediato únicamente**

**Implementado**:
```
✅ MOV reg←reg, reg←imm32
✅ ADD/SUB/AND/OR/XOR/CMP reg,reg / reg,imm
✅ RET (con next_pc en W0)
```

**FALTA (BLOCKER CRÍTICO)**:
```
❌ LoadMem / StoreMem - CUALQUIER acceso a memoria
❌ LEA (calcula dirección efectiva)
❌ PUSH/POP (stack operations)
❌ CALL (necesita push de return address)
❌ Saltos condicionales (Jcc)
❌ Flags x86 (ZF/CF/SF/OF) - calculados pero no usados
```

**Problema Arquitectónico Identificado**:

El IR builder actual hace **lowering directo instrucción-a-instrucción** sin dataflow:
```cpp
// Cada operando hace su propio LoadReg, aunque el valor ya estuviera disponible
ir::Value lhs = EmitLoadReg(block, x.operands[0].reg);  
ir::Value rhs = EmitLoadReg(block, x.operands[1].reg);
```

Esto funciona porque `Arm64CodeGen` usa **registros pineados** (cada registro x86 vive siempre en el mismo registro ARM64), así que `LoadReg` no cuesta instrucción real. Es **correcto pero subóptimo**.

**Decisión de Diseño**: Esto está documentado como deliberado (ver comentario en ir_builder.cpp). La optimización (eliminar LoadReg redundantes) es trabajo futuro. Aceptable para MVP.

**BLOCKER REAL**: Sin LoadMem/StoreMem, **el 70-80% de instrucciones x86 reales no se pueden traducir**.

#### 2.1.3 ARM64 Code Generator

**Archivo**: `src/cpu/jit/arm64_codegen.cpp`  
**Estado**: ✅ **IMPLEMENTADO para el subconjunto disponible**

**Validación Triple**:
1. ✅ Encodings verificados contra `aarch64-linux-gnu-as` (GNU binutils)
2. ✅ Contrastado bit a bit con `switch_ppc_jit_arm64.h` (Super3-NX)
3. ✅ **Ejecutado de verdad** bajo `qemu-aarch64` con validación diferencial

**Cobertura Actual**:
```
✅ LoadImm: MOVZ + MOVK (inmediatos de 32 bits)
✅ LoadReg: mapeo directo a registro pineado (W20-W27)
✅ StoreReg: MOV (ORR Wd,WZR,Wn)
✅ ADD/SUB/AND/OR/XOR: operaciones aritméticas
✅ Return: carga next_pc en W0 + RET
```

**FALTA (bloqueado por IR Builder)**:
```
❌ LoadMem / StoreMem - traducción de [base+index*scale+disp]
❌ CompareAndSetFlags - mapeo ZF/CF/SF/OF → NZCV
❌ Branch condicional - B.cond
❌ Call indirecto - BLR con registro calculado
```

**Convención de Registros**:
```
W20-W27 (X20-X27): EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI (pinned)
X28: Memory base pointer (reservado para traducción de direcciones)
X9-X15: Scratch registers (no preservados entre bloques)
X30/LR: Return address (cada bloque termina en RET)
```

**Hallazgo**: Esta convención es **callee-saved correcta** según AAPCS64. Permite llamar a funciones nativas (libc-shim) sin perder estado de registros x86.

#### 2.1.4 JIT Dispatcher

**Archivo**: `src/cpu/jit/jit.cpp::RunFrom()`  
**Estado**: 🟡 **IMPLEMENTADO pero INCOMPLETO**

**Implementado**:
```cpp
while (current_pc != kExitSentinel) {
    const CachedBlock* block = code_cache_.Find(current_pc);
    if (!block) {
        block = TranslateBlock(current_pc, ...);
    }
    auto fn = reinterpret_cast<BlockFn>(code);
    uint32_t next_pc = fn();  // ← invoca bloque traducido
    current_pc = next_pc;
}
```

**Problema**: Cada bloque devuelve `next_pc` en W0 y retorna al dispatcher C++. Esto es el **"slow path"** - correcto pero ineficiente.

**FALTA**:
```
❌ Context switching de registros x86 (ESP, EBP, etc.)
❌ Inicialización de X28 (memory base pointer)
❌ Block linking (saltos directos bloque→bloque sin volver a C++)
❌ Manejo de excepciones x86 (page faults, división por cero)
```

**Causa Raíz**: El dispatcher actual asume que cada bloque es **auto-contenido** y no modifica estado persistente de registros x86. Esto solo funciona para el fixture de prueba actual (calculadoras simples sin stack/memoria).

**Blocker Real**: Sin context switching, el código que usa ESP/EBP (TODO el código real) fallará.

#### 2.1.5 Code Cache

**Archivo**: `src/cpu/jit/code_cache.cpp`  
**Estado**: ✅ **FUNCIONAL para uso básico**

**Implementado**:
```
✅ Insert(guest_addr, arm64_bytes, size) - copia a memoria ejecutable
✅ Find(guest_addr) - lookup por dirección guest
✅ ExecutableAddress(block) - conversión a puntero ejecutable
```

**FALTA**:
```
❌ InvalidateRange() - stub completo
❌ SaveToDisk() / LoadFromDisk() - persistencia entre ejecuciones
❌ Eviction policy - qué hacer cuando se llena
❌ Thread safety - no hay locks (OK para single-threaded MVP)
```

**Hallazgo**: `InvalidateRange()` está documentado como "lección aprendida de Super3-NX" (`PpcJitInvalidate()` sin call site). El stub está presente pero vacío. **Decisión correcta**: diseñar la invalidación desde el principio, no añadirla después.

### 2.2 OS Layer: ELF Loading & Shims

#### 2.2.1 ELF Loader

**Archivo**: `src/os/elf_loader/elf_loader.cpp`  
**Estado**: ✅ **IMPLEMENTADO Y VALIDADO**

**Capacidades**:
```
✅ Parseo completo de ELF32 i386
✅ Carga de PT_LOAD segments
✅ Resolución de .dynsym/.dynstr
✅ Aplicación de relocations:
   - R_386_RELATIVE (base+addend)
   - R_386_32 (symbol+addend)
   - R_386_PC32 (symbol+addend-offset)
   - R_386_GLOB_DAT (GOT entries)
   - R_386_JMP_SLOT (PLT entries)
✅ Reporte de símbolos no resueltos (no falla silencioso)
✅ Reserva de memoria contigua para address space guest
```

**Test Coverage**:
- ELF32 sintético (construido a mano)
- ELF32 real compilado con GCC -m32 (con PLT/GOT reales)
- Casos de error (bad magic, direcciones fuera de rango, etc.)

**Limitación Documentada**:
```cpp
out.guest_memory.assign(max_vaddr_end - min_vaddr, 0);
// NO reserva heap dinámico ni stack todavía
```

**Hallazgo Crítico**: El loader funciona pero **NO implementa la arquitectura de memoria** descrita en `docs/MEMORY_ARCHITECTURE_ANALYSIS.md`:

```cpp
// DEBERÍA ser (según el documento):
const size_t kHeapSize = 64 * 1024 * 1024;  // 64MB heap
const size_t kStackSize = 8 * 1024 * 1024;  // 8MB stack
size_t total_size = (max_vaddr_end - min_vaddr) + kHeapSize + kStackSize;
out.guest_memory.assign(total_size, 0);
out.heap_start = max_vaddr_end;
out.stack_start = out.heap_end;
out.initial_esp = out.stack_end - 4;  // ← FALTA en LoadedElf
```

**Blocker**: Sin heap/stack pre-allocated, `malloc()` y cualquier uso de stack fallarán.

#### 2.2.2 Libc Shim

**Archivo**: `src/os/syscall/libc_shim.cpp`  
**Estado**: ❌ **VACÍO COMPLETO - BLOCKER CRÍTICO**

**Implementado**:
```cpp
void LibcShim::Register(const std::string& symbol_name, NativeFn fn) {
    functions_[symbol_name] = fn;
}
NativeFn LibcShim::Resolve(const std::string& symbol_name) const {
    auto it = functions_.find(symbol_name);
    return it != functions_.end() ? it->second : nullptr;
}
```

Es solo un **registro de símbolos vacío**. NO hay ninguna función implementada.

**FALTA (P0 - sin esto NADA funciona)**:
```
❌ malloc/calloc/realloc/free - gestión de heap
❌ open/close/read/write/lseek - I/O de archivos
❌ fopen/fclose/fread/fwrite/fseek - I/O buffered
❌ gettimeofday/clock_gettime - timing
❌ memcpy/memset/strlen/strcmp - string ops (¿inline o shim?)
❌ printf/fprintf/snprintf - logging (redirigir a PAS_LOG_*)
❌ pthread_* - threading (Lindbergh probablemente NO use threads en MVP)
```

**Decisión de Diseño Documentada** (en libc_shim.h):
> "NO se implementa 'toda la libc' de antemano. Cada símbolo que el ElfLoader reporte como no resuelto se añade aquí uno a uno, contra el binario real del título elegido"

**Hallazgo**: Enfoque correcto (incremental), pero **sin binario real todavía, no se ha empezado**. Causa raíz: docs/ROADMAP.md dice explícitamente:
> "no se dispone de un binario real de Lindbergh para probar esto — son juegos comerciales con copyright"

**Problema**: El proyecto está **bloqueado esperando un binario real** para determinar qué símbolos necesita.

#### 2.2.3 Filesystem

**Ubicación**: `src/os/filesystem/`  
**Estado**: 🔍 **NO EXISTE TODAVÍA**

Según `docs/ARCADE_HARDWARE.md`, Lindbergh intercepta rutas específicas:
```
/dev/lbb → baseboard device
/dev/i2c/0 → eeprom
/dev/ttyS0, /dev/ttyS1 → serie (JVS, ride boards)
/proc/bus/pci/... → PCI config space sintético
```

**FALTA**: No hay ningún VFS (Virtual File System) implementado. Cuando `open("/dev/lbb")` se traduzca, no hay nada que lo capture.

### 2.3 Graphics: OpenGL/Cg → deko3d

#### 2.3.1 GL Shim

**Archivo**: `src/graphics/gl_shim/gl_shim.cpp`  
**Estado**: ❌ **STUB COMPLETO**

```cpp
void GlShim::RegisterSymbols() {
    // TODO(Nivel 1): registrar los simbolos gl*/cg* realmente usados
    PAS_LOG_WARN("GlShim", "RegisterSymbols no implementado todavia");
}
```

**FALTA**: TODO. Ni un solo símbolo OpenGL registrado.

**Superficie API Esperada** (según lindbergh-loader y docs):
```
OpenGL 1.x/2.x función fija:
- glBegin/glEnd, glVertex*, glColor*, glTexCoord*
- glEnable/glDisable (GL_TEXTURE_2D, GL_BLEND, GL_DEPTH_TEST...)
- glBindTexture, glTexImage2D, glTexParameteri
- glMatrixMode, glLoadMatrix, glMultMatrix (pipeline fijo)
- glDrawArrays, glDrawElements (si usa VBOs)

NVIDIA Cg 3.1:
- cgCreateContext, cgCreateProgram, cgCompileProgram
- cgGLLoadProgram, cgGLBindProgram
- cgGetNamedParameter, cgGLSetParameter*
```

**Complejidad Estimada**: ~200-300 funciones para un juego típico (comparar con lindbergh-loader, que intercepta ~150 símbolos GL/Cg).

#### 2.3.2 Deko3d Backend

**Archivo**: `src/platform/switch/graphics_deko3d.cpp`  
**Estado**: ❌ **STUB COMPLETO**

**Reutilización de DLSS-Switch**:
El archivo tiene comentario explícito:
```cpp
// TODO: reutilizar inicialización de DLSS-Switch
```

**Hallazgo**: Ya tienes una inicialización de deko3d funcional en otro proyecto. **Copiar ese código es el siguiente paso obvio**, no reinventarlo.

**FALTA**:
```
❌ dkMemBlockMaker, dkMemBlockCreate - gestión de memoria GPU
❌ dkCmdBufMaker, dkCmdBufCreate - command buffers
❌ dkQueueMaker, dkQueueCreate - submit queue
❌ dkSwapchainMaker, dkSwapchainCreate - presentación
❌ dkShaderMaker, dkShaderInitialize - shaders DKSH
❌ State tracking GL→deko3d (blend modes, depth test, etc.)
```

**Riesgo Técnico** (documentado en GRAPHICS.md):
> **Compilación de shaders en tiempo de ejecución**: deko3d consume DKSH precompilado. Los shaders Cg del juego se compilan en runtime. ¿Compilador Cg→DKSH en runtime, o extracción offline?

### 2.4 Arcade Hardware: JVS & EEPROM

#### 2.4.1 Virtual JVS I/O Board

**Archivo**: `src/arcade/jvs/virtual_jvs.cpp`  
**Estado**: ❌ **STUB con estructura básica**

```cpp
JvsButtonState VirtualJvsIoBoard::PollState() {
    // TODO: leer input_backend_.Poll() y aplicar mapeo
    JvsButtonState state;
    state.coin_count = pending_coins_;
    pending_coins_ = 0;
    return state;
}
```

**FALTA**:
- Protocolo JVS real (framing de paquetes)
- Mapeo de botones Switch → inputs JVS
- Lectura de HidNpad (Joy-Con / Pro Controller)
- Configuración por GameProfile

**Hallazgo**: La estructura es correcta (VirtualJvsIoBoard ← IInputBackend), pero sin implementación.

#### 2.4.2 EEPROM Synthesizer

**Archivo**: `src/arcade/devices/eeprom.cpp`  
**Estado**: ❌ **STUB COMPLETO**

```cpp
std::vector<uint8_t> EepromSynthesizer::SynthesizeEeprom() {
    PAS_LOG_ERROR("EepromSynthesizer", "SynthesizeEeprom no implementado todavia");
    return {};
}
```

**Referencia Disponible**: lindbergh-loader tiene la implementación completa:
```c
// De lindbergh.so (documentado públicamente):
amSysDataRecord record = {
    .serial = "AAVE-01A12345678",
    .region = REGION_JAPAN,
    // ... más campos ...
};
uint16_t crc = CalculateCRC(&record, sizeof(record));
```

**Hallazgo**: **Esto es copy-paste directo de documentación pública**. No hay ingeniería inversa necesaria. Solo hay que transcribir el código ya publicado.

### 2.5 Platform/Switch: Horizon OS Integration

#### 2.5.1 JIT Memory

**Archivo**: `src/platform/switch/jit_memory.cpp`  
**Estado**: ✅ **IMPLEMENTADO pero NO VERIFICADO**

Nota de honestidad del autor:
```cpp
// Ver la nota de honestidad en jit_memory.h: API jit* de libnx tal y como
// se documenta públicamente [...] No verificado contra una compilación real
// en este entorno.
```

**Hallazgo**: El código está escrito contra la API pública documentada de libnx, pero **no ha pasado por un compilador real de devkitA64**.

**Probabilidad de Éxito al Compilar**: Alta (>90%). Usa la API estándar de Super3-NX. Posibles problemas menores: nombres de funciones, headers includes.

#### 2.5.2 Input (HID)

**Archivo**: `src/platform/switch/input_hid.cpp`  
**Estado**: ❌ **STUB COMPLETO**

```cpp
void SwitchHidInputBackend::Poll() {
    // TODO: padUpdate() + lectura de estado real. No implementado.
}
```

**FALTA**: TODO. Reutilizar código HID de Super3-NX / DLSS-Switch.

#### 2.5.3 Main Entry Point

**Archivo**: `src/platform/switch/main.cpp`  
**Estado**: 🟡 **ESQUELETO compilable**

Flujo implementado parcialmente:
```
1. ✅ consoleInit()
2. ✅ SetLogSink()
3. ✅ Config::LoadFromFile()
4. 🟡 PlatformProfileLoader::LoadFromFile() - parser funciona, archivo no existe
5. 🟡 GameProfileLoader::LoadFromFile() - parser funciona, archivo no existe
6. ❌ ElfLoader::Load() - no se llama (bloqueado por #4/#5)
7. ❌ Jit::RunFrom() - no se llama
```

**Blocker**: Se detiene en paso #5 porque no hay `profiles/example.json` real todavía.

### 2.6 Tests: Cobertura de Validación

**Ubicación**: `tests/`  
**Estado**: ✅ **COBERTURA EXCEPCIONAL del código existente**

#### Test Coverage por Componente

| Test File | Cubre | Metodología | Estado |
|-----------|-------|-------------|--------|
| `test_arm64_emitter.cpp` | Emitter ARM64 | Comparación bit a bit vs `aarch64-as` | ✅ Pasando |
| `test_arm64_dynamic_exec.cpp` | Emitter ARM64 | **Ejecución real** bajo qemu-aarch64 | ✅ Pasando |
| `test_x86_decoder.cpp` | Decoder x86 | Comparación vs `objdump -d` | ✅ Pasando |
| `test_elf_loader.cpp` | ELF Loader | ELF sintético + casos de error | ✅ Pasando |
| `test_elf_loader_real_binary.cpp` | ELF Loader | **ELF real** compilado con GCC -m32 | ✅ Pasando |
| `test_ir_end_to_end.cpp` | Pipeline completo | **Validación diferencial** x86 nativo vs ARM64 | ✅ Pasando |
| `test_jit_integration.cpp` | JIT completo | Traducción + ejecución bajo QEMU | ✅ Pasando |
| `test_code_cache.cpp` | Code Cache | Insert/Find/ExecutableAddress | ✅ Pasando |
| `test_game_profile.cpp` | Profile Parser | JSON real de `profiles/` | ✅ Pasando |
| `test_json_lite.cpp` | JSON Parser | Parser propio sin dependencias | ✅ Pasando |
| `test_patch_engine.cpp` | Patch Engine | Transformaciones IR | ✅ Pasando |

**Validación Diferencial** (test_ir_end_to_end.cpp):
```cpp
// Compara x86 nativo vs ARM64 traducido para 11 valores de entrada:
int32_t test_inputs[] = {0, 1, -1, 42, -42, 100, -100, 
                         INT_MAX, INT_MIN, INT_MAX/2, INT_MIN/2};
// Si coinciden todos → el pipeline completo es correcto
```

**Hallazgo**: Este nivel de validación es **excepcional**. Muy pocos proyectos de DBT tienen validación diferencial end-to-end en su test suite.

**Cobertura de Funcionalidad**:
- Código implementado: ~95% cubierto por tests
- Código stub: 0% cubierto (obviamente)

---

## 3. DEFICIENCIAS ARQUITECTÓNICAS

### 3.1 Deficiencias Estructurales

#### 3.1.1 Arquitectura de Memoria Guest NO Implementada

**Severidad**: 🔴 **CRÍTICA - BLOQUEA TODO**

**Documento**: `docs/MEMORY_ARCHITECTURE_ANALYSIS.md` (creado pero NO implementado)

**Problema**:
El documento de 600+ líneas analiza exhaustivamente cómo traducir accesos x86 a memoria guest, propone 4 opciones (X28 register, helper function, software TLB, hybrid), decide por X28, y documenta la implementación completa con código de ejemplo.

**Pero ninguna línea de ese diseño está implementada en el código real.**

**Impacto**:
- `LoadMem`/`StoreMem` en IR no tienen lowering a ARM64
- `X28` no se inicializa en el dispatcher
- ELF loader no reserva heap/stack
- Sin esto, **TODO código con accesos a memoria falla**

**Causa Raíz**: Análisis arquitectónico exhaustivo pero sin transición a implementación. El documento es de Septiembre 2026, el código es previo.

**Decisión Recomendada**: La decisión de X28 es correcta. Hay que implementarla.

#### 3.1.2 Falta de Integración entre Componentes

**Severidad**: 🟡 **MEDIA - Arquitectura correcta, falta conexión**

**Problema**:
Muchos componentes están implementados **individualmente** y validados, pero **no conectados**:

```
✅ ElfLoader carga el binario → pero no llama a LibcShim.Register()
✅ LibcShim.Register() existe → pero ningún símbolo registrado
✅ Jit.TranslateBlock() traduce → pero no inicializa registros x86
✅ IrBuilder construye IR → pero ApplyPatches() es stub
✅ PatchEngine lee patches → pero no se conecta al IrBuilder
```

**Causa Raíz**: Desarrollo bottom-up (cada pieza funciona sola) sin integración top-down todavía.

**Impacto**: El `main.cpp` se detiene temprano porque los componentes no están enlazados.

### 3.2 Decisiones de Diseño Cuestionables

❌ **NO SE ENCONTRARON DECISIONES ARQUITECTÓNICAS INCORRECTAS**

Todas las decisiones importantes están justificadas:
- ✅ DBT en vez de emulación completa: correcto
- ✅ IR intermedia: justificado para patches y portabilidad
- ✅ Registros pineados x86→ARM64: simplifica codegen
- ✅ X28 como memory base: apropiado para Lindbergh
- ✅ Separación core/platform: limpia y correcta
- ✅ No portar Box64/FEX directamente: imposible por dependencias Linux
- ✅ Lindbergh como primer target: mejor documentado, sin Windows

### 3.3 Acoplamiento Incorrecto

✅ **NO SE ENCONTRÓ ACOPLAMIENTO INCORRECTO**

- CPU layer no sabe de Lindbergh
- OS layer no sabe de Switch
- Graphics layer tiene separación `gl_shim/` (genérico) vs `switch/` (deko3d)
- Solo `src/platform/switch/` incluye `<switch.h>`

---

## 4. ESTADO REAL DE IMPLEMENTACIÓN

### 4.1 Taxonomía de Estado

| Estado | Definición | Criterio |
|--------|------------|----------|
| ✅ **CORRECTO** | Implementado + validado + funciona | Tests pasando, código robusto |
| 🟢 **FUNCIONAL** | Implementado + funciona, validación básica | Sin tests exhaustivos pero usado |
| 🟡 **PARCIAL** | Implementado parcialmente, falta alcance | Parte del diseño hecho |
| ⚠️ **STUB FUNCIONAL** | Esqueleto con estructura correcta | Interfaces definidas, cuerpo vacío |
| ❌ **STUB VACÍO** | Placeholder sin implementación | Solo TODO/warning |
| 🔴 **INEXISTENTE** | Ni diseñado ni implementado | Falta por completo |

### 4.2 Matriz de Estado por Componente

#### CPU Subsystem

| Componente | Estado | % Implementado | Blocker para MVP |
|------------|--------|----------------|------------------|
| x86 Decoder base | ✅ CORRECTO | 30% | ❌ Falta x87, SSE, Jcc |
| ModRM/SIB decoding | ✅ CORRECTO | 100% | ✅ |
| IR Definition | ✅ CORRECTO | 100% | ✅ |
| IR Builder (reg/imm) | ✅ CORRECTO | 40% | ❌ Falta LoadMem/StoreMem |
| IR Builder (memory) | 🔴 INEXISTENTE | 0% | 🔴 **P0 BLOCKER** |
| ARM64 Emitter | ✅ CORRECTO | 95% | ⚠️ Falta shift/FP |
| Arm64CodeGen (reg) | ✅ CORRECTO | 50% | ❌ Falta LoadMem/StoreMem |
| Arm64CodeGen (mem) | 🔴 INEXISTENTE | 0% | 🔴 **P0 BLOCKER** |
| JIT Dispatcher | 🟡 PARCIAL | 60% | ❌ Falta context switch |
| Code Cache | 🟢 FUNCIONAL | 80% | ⚠️ Falta invalidation |
| Block Linking | 🔴 INEXISTENTE | 0% | 🟡 Optimización futura |

**Veredicto**: CPU layer tiene **fundamentos sólidos** pero **sin memoria ni FPU es inútil**.

#### OS Subsystem

| Componente | Estado | % Implementado | Blocker para MVP |
|------------|--------|----------------|------------------|
| ELF32 Parser | ✅ CORRECTO | 100% | ✅ |
| ELF32 Relocation | ✅ CORRECTO | 100% | ✅ |
| Symbol Resolution | 🟢 FUNCIONAL | 90% | ⚠️ Falta linkear con shims |
| Memory Layout (heap/stack) | 🔴 INEXISTENTE | 0% | 🔴 **P0 BLOCKER** |
| LibcShim Registry | ✅ CORRECTO | 100% | ✅ |
| LibcShim Functions | ❌ STUB VACÍO | 0% | 🔴 **P0 BLOCKER** |
| VFS (/dev/...) | 🔴 INEXISTENTE | 0% | 🔴 **P1 BLOCKER** |
| Filesystem (sdmc:/) | ⚠️ STUB FUNCIONAL | 10% | ⚠️ |

**Veredicto**: ELF loader ✅, pero **TODO el runtime está vacío**.

#### Graphics Subsystem

| Componente | Estado | % Implementado | Blocker para MVP |
|------------|--------|----------------|------------------|
| GL Shim Registry | ⚠️ STUB FUNCIONAL | 5% | ❌ |
| GL State Tracking | 🔴 INEXISTENTE | 0% | 🔴 **P1 BLOCKER** |
| Cg Runtime | ❌ STUB VACÍO | 0% | 🔴 **P1 BLOCKER** |
| Deko3d Init | ❌ STUB VACÍO | 0% | 🔴 **P1 BLOCKER** |
| Shader Compilation | 🔴 INEXISTENTE | 0% | 🔴 **RIESGO TÉCNICO** |
| Texture Mapping | 🔴 INEXISTENTE | 0% | 🔴 **P1 BLOCKER** |

**Veredicto**: Graphics **completamente pendiente**. Pero hay código reutilizable (DLSS-Switch).

#### Arcade Subsystem

| Componente | Estado | % Implementado | Blocker para MVP |
|------------|--------|----------------|------------------|
| JVS Protocol | ⚠️ STUB FUNCIONAL | 10% | ❌ |
| EEPROM Synthesis | ❌ STUB VACÍO | 0% | 🔴 **P1 BLOCKER** |
| Device VFS | 🔴 INEXISTENTE | 0% | 🔴 **P1 BLOCKER** |

**Veredicto**: Diseño correcto, implementación 0%. Hay referencia (lindbergh-loader).

#### Platform/Switch

| Componente | Estado | % Implementado | Blocker para MVP |
|------------|--------|----------------|------------------|
| JIT Memory | 🟢 FUNCIONAL | 95% | ⚠️ No compilado |
| HID Input | ❌ STUB VACÍO | 0% | ❌ |
| Deko3d Backend | ❌ STUB VACÍO | 0% | 🔴 **P1 BLOCKER** |
| Main Entry Point | 🟡 PARCIAL | 50% | ⚠️ |

**Veredicto**: JIT memory listo, resto pendiente.

### 4.3 Estimación de Completitud del MVP Nivel 1

**Según docs/ROADMAP.md, MVP Nivel 1 requiere**:
```
1. ✅ Cargador ELF x86 de 32 bits — HECHO
2. ❌ libc-shim mínima (malloc/free, I/O, gettimeofday) — 0%
3. ❌ Decodificador x86 (subconjunto real usado) — 30%
4. ✅ Memoria ejecutable Horizon-safe — HECHO (no compilado)
5. ❌ Shim de eeprom/sram — 0%
6. ❌ Traductor GL/Cg → deko3d — 0%
7. ✅ Parser de GameProfile — HECHO
```

**Completitud Real del MVP Nivel 1**: **~35%**

**Progreso por Subsistema**:
- ✅ Infrastructure (tests, profiles, core): 95%
- 🟡 CPU (decoder, JIT framework): 40%
- ❌ Memory Translation: 0%
- ❌ OS Runtime (libc, VFS): 5%
- ❌ Graphics: 0%
- ❌ Arcade Devices: 0%

---

## 5. DIAGNÓSTICO TÉCNICO: CAUSAS RAÍZ

### 5.1 Bloqueadores Fundamentales Identificados

#### BLOCKER #1: Arquitectura de Memoria NO Implementada

**Síntoma**: `LoadMem`/`StoreMem` en IR no tienen lowering.

**Causa Raíz Técnica**:
El diseño existe (docs/MEMORY_ARCHITECTURE_ANALYSIS.md) pero no se implementó. El documento propone:
```cpp
// X28 = base de guest_memory
// Para MOV EAX, [EBX+0x10]:
ADD  W9, W23, #0x10       // guest effective address
SUB  W9, W9, #base_addr   // offset en buffer
LDR  W20, [X28, W9, UXTW] // load desde base
```

Pero en `arm64_codegen.cpp::Generate()`:
```cpp
case ir::OpCode::LoadMem: {
    // NO IMPLEMENTADO
}
```

**Causa Raíz Organizacional**:
- Análisis arquitectónico exhaustivo ANTES de codificar (correcto)
- Pero análisis no se tradujo a código (gap de implementación)
- Posible causa: documento creado recientemente (Sept 2026), código es anterior

**Impacto**: Sin esto, **el 70% de instrucciones x86 no se pueden traducir**.

**Dependencias**:
1. ELF loader debe reservar heap/stack (`LoadedElf` no tiene `heap_start`, `initial_esp`)
2. Dispatcher debe inicializar X28
3. Arm64CodeGen debe implementar `EmitEffectiveAddress()` + `LDR/STR con X28`
4. Emitter debe añadir variantes de LDR/STR con extend register

**Estimación**: 2-3 días de trabajo para un ingeniero senior.

#### BLOCKER #2: Libc Shim Vacía

**Síntoma**: `LibcShim::Resolve()` siempre devuelve `nullptr`.

**Causa Raíz**:
Decisión de diseño documentada:
> "Cada símbolo que el ElfLoader reporte como no resuelto se añade aquí uno a uno, contra el binario real del título elegido"

Pero:
> "no se dispone de un binario real de Lindbergh para probar esto — son juegos comerciales con copyright"

**Bloqueo Circular**:
```
Sin binario real → no se sabe qué símbolos necesita
  ↓
Sin símbolos → no se puede correr binario
  ↓
Sin binario corriendo → no se puede determinar si funciona
```

**Solución Propuesta**:
1. **Implementar conjunto mínimo conocido** (de lindbergh-loader):
   ```c
   malloc, free, open, close, read, write, fopen, fclose, 
   fread, fwrite, fseek, gettimeofday, memcpy, memset
   ```
2. **Iterar al encontrar símbolos no resueltos** (logs del ElfLoader)

**Estimación**: 3-5 días para MVP (10-15 funciones básicas).

#### BLOCKER #3: Decoder x86 Incompleto (x87, SSE)

**Síntoma**: Fixture de prueba usa solo instrucciones básicas.

**Causa Raíz**:
Test escribía contra binario real compilado (`real_elf32_sample.c`) pero ese código es:
```c
int add(int a, int b) { return a + b; }  // solo aritmética entera
```

**No ejercita**:
- Floating point (x87: `fld`, `fst`, `fadd`, `fmul`)
- SIMD (SSE: `movss`, `addps`, `mulps`)
- Control de flujo complejo (loops con `jcc`)

**Impacto**: Lindbergh usa FP extensivamente (juegos 3D, físicas).

**Solución**:
1. Crear `real_elf32_float_sample.c` con código FP real
2. Decodificar opcodes x87 (0xD8-0xDF) según manual Intel
3. Definir IRs para FP (`FAdd`, `FMul`, etc.)
4. Codegen ARM64 con NEON (no x87, ARM no tiene x87)

**Estimación**: 5-7 días (x87 es complejo, hay 80+ instrucciones FP).

### 5.2 Otros Problemas Técnicos

#### PROBLEMA #4: No hay Binario Real para Validar

**Impacto**: Proyecto bloqueado en ~35% porque no puede determinar:
- Qué símbolos libc necesita
- Qué opcodes x86 se usan realmente
- Si EEPROM synthesis es correcto
- Si el juego arranca

**Recomendación**:
- Conseguir un dump legal de un juego Lindbergh (poseer placa física)
- O usar binario de test de lindbergh-loader si existe
- O crear un "hello world Lindbergh-like" que simule las características

#### PROBLEMA #5: Graphics Backend Sin Inicializar

**Causa Raíz**: Ya tienes código deko3d funcional en DLSS-Switch, pero no se ha copiado aquí.

**Solución**: Copy-paste directo de DLSS-Switch (no hay por qué reinventar).

**Estimación**: 1 día para inicialización básica.

---

## 6. ROADMAP TÉCNICO PRIORIZADO

### FASE 0: Pre-Requisitos (ANTES de continuar)

**Duración**: 2-3 días

```
□ Compilar el proyecto contra devkitA64 real
  - Verificar jit_memory.cpp, main.cpp compilan
  - Ajustar includes/nombres de funciones si necesario
  - Validar que .nro se genera

□ Conseguir binario de test Lindbergh O crear uno sintético
  - Si legal: dump de juego propio
  - Si no: programa mínimo ELF32 i386 que simule características Lindbergh
    - Usa OpenGL/Cg
    - Lee /dev/lbb, /dev/i2c/0
    - Usa x87/SSE
    - Llama malloc, fopen, gettimeofday

□ Crear profiles reales
  - profiles/lindbergh.platform.json
  - profiles/test_game.json (para el binario de test)
```

### FASE 1: Desbloqueadores Fundamentales (P0)

**Duración**: 1-2 semanas  
**Objetivo**: Conseguir que un binario mínimo ejecute su primera instrucción con memoria

#### Sprint 1.1: Arquitectura de Memoria (3-4 días)

```
□ Implementar reserva de heap/stack en ElfLoader
  struct LoadedElf {
      uint32_t heap_start, heap_end;
      uint32_t stack_start, stack_end;
      uint32_t initial_esp;
  };
  - Reservar 64MB heap, 8MB stack después de PT_LOAD
  - Calcular initial_esp = stack_end - 4

□ Inicializar X28 en Dispatcher
  void Jit::RunFrom(...) {
      // Inicializar X28 una vez al inicio
      InitializeMemoryBase(guest_memory_base);
      // Inicializar ESP guest
      InitializeGuestRegisters(loaded_elf.initial_esp);
  }

□ Implementar EmitEffectiveAddress() en Arm64CodeGen
  uint8_t EmitEffectiveAddress(const ir::MemoryOperand& mem, Emitter& e) {
      // Calcular base + index*scale + disp
      // Ver código completo en docs/MEMORY_ARCHITECTURE_ANALYSIS.md
  }

□ Implementar LoadMem/StoreMem lowering
  case ir::OpCode::LoadMem:
      addr_reg = EmitEffectiveAddress(...);
      EmitSubImmediate32(offset_reg, addr_reg, base);
      emitter.EmitLdrExtendReg(result, 28, offset_reg);

□ Añadir instrucciones ARM64 faltantes en Emitter
  - EmitLdrExtendReg, EmitLdrbExtendReg, EmitLdrhExtendReg
  - EmitStrExtendReg, EmitStrbExtendReg, EmitStrhExtendReg
  - EmitLsl, EmitAddImmediate32, EmitSubImmediate32

□ Test end-to-end: MOV EAX, [0x1000]; RET
  - x86 nativo vs ARM64 traducido, validar resultado
```

**Criterio de Éxito**: Un binario que hace `mov eax, [fixed_address]; ret` ejecuta correctamente y devuelve el valor esperado.

#### Sprint 1.2: Libc Shim Mínima (2-3 días)

```
□ Implementar gestión de heap sintético
  - malloc/calloc/realloc/free sobre región pre-reservada
  - Allocator simple (first-fit o bump allocator, suficiente para MVP)

□ Implementar I/O básico
  - open/close/read/write: redirigir a sdmc:/arcade/data/...
  - fopen/fclose/fread/fwrite: sobre open/close

□ Implementar timing
  - gettimeofday: mapear a svcGetSystemTick() de libnx

□ String operations (decidir: inline o shim)
  - Si shim: memcpy, memset, strlen, strcmp
  - Si inline: el decoder reconoce 0xF3 0xA5 (rep movsd) y traduce a bucle ARM64

□ Logging
  - printf/fprintf: redirigir a PAS_LOG_INFO

□ Registrar símbolos en LibcShim::Instance()
  - Crear thunks que conviertan convención de llamada x86 → C++ nativo

□ Test: binario que hace malloc + fopen + gettimeofday + printf
```

**Criterio de Éxito**: Binario simple ejecuta, alloca memoria, abre archivo, imprime log.

#### Sprint 1.3: Context Switching del Dispatcher (1-2 días)

```
□ Definir estructura de contexto x86
  struct X86Context {
      uint32_t regs[8];  // EAX-EDI
      uint32_t eflags;
      uint32_t eip;
  };

□ Modificar Jit::RunFrom para mantener contexto
  X86Context ctx;
  ctx.regs[4] = loaded_elf.initial_esp;  // ESP
  while (...) {
      // Pasar contexto a bloques traducidos
      fn(&ctx);
  }

□ Modificar Arm64CodeGen para leer/escribir contexto
  - LoadReg lee de ctx->regs[reg_index]
  - StoreReg escribe a ctx->regs[reg_index]
  - (Alternativa: mantener registros pineados pero guardar/restaurar al inicio/fin de bloque)

□ Test: binario con múltiples funciones, stack frames, push/pop
```

**Criterio de Éxito**: Binario con CALL/RET ejecuta correctamente, stack no se corrompe.

### FASE 2: Decoder x86 Completo para Lindbergh (P1)

**Duración**: 1-2 semanas

#### Sprint 2.1: Control de Flujo (2-3 días)

```
□ Implementar decodificación de saltos
  - JMP rel8/rel32 (0xEB/0xE9)
  - Jcc near (0x70-0x7F)
  - Jcc far (0x0F 0x80-0x8F)
  - CALL r/m32 (0xFF /2)

□ Implementar IRs condicionales
  - ir::Branch(condition, target_true, target_false)
  - ir::CompareAndSetFlags(src1, src2) - actualiza ZF/CF/SF/OF

□ Mapeo de flags x86 → ARM64
  - ZF → Z (zero)
  - CF → C (carry)
  - SF → N (negative)
  - OF → V (overflow)

□ Codegen para saltos condicionales
  - B.EQ, B.NE, B.LT, B.GE, etc.

□ Test: binario con loop (for/while)
```

#### Sprint 2.2: x87 FPU (3-4 días)

```
□ Decodificar opcodes x87 (0xD8-0xDF)
  - FLD m32/m64 (push a stack FP)
  - FST/FSTP m32/m64 (pop de stack FP)
  - FADD/FSUB/FMUL/FDIV

□ Diseño de x87 stack sintético
  - x87 tiene stack de 8 registros ST(0)-ST(7)
  - Mantener en memoria guest (no usar registros ARM, demasiado complejo)

□ IRs FP
  - ir::FLoad, ir::FStore, ir::FAdd, ir::FMul, etc.

□ Codegen ARM64 con NEON (no x87)
  - FMOV, FADD, FMUL (single/double precision)

□ Test: binario con cálculos FP (sin, cos, sqrt si usados)
```

#### Sprint 2.3: SSE/SSE2 (2-3 días)

```
□ Decodificar SSE (0x0F ...)
  - MOVSS, MOVSD, ADDPS, MULPS, etc.

□ IRs SIMD
  - ir::VectorLoad, ir::VectorAdd, ir::VectorMul

□ Codegen con NEON
  - LDR Qn, [addr]
  - FADD Vn.4S, Vm.4S, Vk.4S

□ Test: binario con vectorización (arrays de float)
```

### FASE 3: Graphics & Arcade Devices (P1)

**Duración**: 2-3 semanas

#### Sprint 3.1: Deko3d Initialization (2-3 días)

```
□ Copiar init de DLSS-Switch
  - dkMemBlockMaker, dkCmdBufCreate, dkQueueCreate, dkSwapchainCreate

□ Implementar CreateTexture, CreateVertexBuffer stubs funcionales
  - Allocar memoria GPU, NO traducción de shaders todavía

□ Test: clear screen a color sólido
```

#### Sprint 3.2: GL Shim Básico (4-5 días)

```
□ Registrar símbolos OpenGL básicos
  - glBegin/glEnd, glVertex3f, glColor4f
  - glEnable/glDisable
  - glBindTexture, glTexImage2D
  - glMatrixMode, glLoadMatrix

□ State tracking
  - Mantener estado OpenGL actual (matriz activa, blend mode, etc.)

□ Traducción básica a deko3d
  - glBegin → iniciar command buffer
  - glVertex3f → escribir vertex buffer
  - glEnd → submit draw

□ Test: triángulo rojo sin shaders (fixed pipeline simulado)
```

#### Sprint 3.3: Cg Runtime Stub (3-4 días)

```
□ Registrar símbolos Cg
  - cgCreateContext, cgCreateProgram, cgCompileProgram
  - cgGLLoadProgram, cgGLBindProgram

□ Opción A (recomendada para MVP): Precompilación offline
  - cgCompileProgram devuelve "compiled" sin hacer nada
  - cgGLLoadProgram carga .dksh precompilado desde sdmc:/

□ Test: shader Cg precompilado (vertex + fragment triviales)
```

#### Sprint 3.4: EEPROM & JVS (2-3 días)

```
□ Implementar EepromSynthesizer
  - Copiar código de lindbergh-loader (público)
  - Calcular CRC de amSysDataRecord

□ Implementar VFS para /dev/lbb, /dev/i2c/0
  - Interceptar open("/dev/lbb") → devolver fd sintético
  - read(fd) → devolver bytes de eeprom

□ Implementar JVS básico
  - InsertCoin(), PollState() funcionan
  - Mapear botones Switch → JVS (según GameProfile)

□ Test: binario lee EEPROM, detecta "coin inserted"
```

### FASE 4: Integración & Primera Ejecución (P0)

**Duración**: 1 semana

```
□ Conectar todos los componentes en main.cpp
  - ElfLoader carga binario real
  - LibcShim registra símbolos antes de Load()
  - GlShim registra símbolos
  - EEPROM synthesis antes de RunFrom()
  - Jit::RunFrom() con contexto inicializado

□ Crear GameProfile real para juego de test

□ Debugging end-to-end
  - Logging extensivo (cada instrucción traducida, cada símbolo resuelto)
  - Seguir ejecución paso a paso hasta primera llamada GL

□ Criterio de Éxito: El juego ejecuta hasta glClear() o primer frame
```

### FASE 5: Optimización & Nivel 2 MVP

**Duración**: 2-4 semanas (fuera del scope inmediato)

```
□ Block linking (saltos directos sin volver a dispatcher)
□ Code cache persistente (SaveToDisk/LoadFromDisk)
□ Invalidación de bloques (self-modifying code)
□ Optimización de IR (eliminación de LoadReg redundantes)
□ Perfilado real en hardware Switch
□ Shader compilation full (si precompilación no basta)
□ Audio backend
□ Input mapping completo (analógicos, triggers)
```

---

## 7. ESTIMACIONES DE ESFUERZO

### Tiempo Total Estimado para MVP Nivel 1 Funcional

| Fase | Duración | Dependencias |
|------|----------|--------------|
| FASE 0: Pre-requisitos | 2-3 días | Ninguna |
| FASE 1: Fundamentals (memoria + libc + dispatcher) | 1-2 semanas | FASE 0 |
| FASE 2: Decoder completo | 1-2 semanas | FASE 1 |
| FASE 3: Graphics + Arcade | 2-3 semanas | FASE 1, parcial FASE 2 |
| FASE 4: Integración | 1 semana | TODAS |
| **TOTAL** | **6-9 semanas** | **~1.5-2 meses** |

**Con un ingeniero senior a tiempo completo**: 2 meses para primer frame.

**Factores de Riesgo**:
- ❌ Sin binario real: +1-2 semanas de iteración
- ⚠️ Shader compilation: riesgo técnico no resuelto, podría añadir +2-4 semanas
- ✅ Código reutilizable (DLSS-Switch, Super3-NX): reduce riesgo

### Trabajo Ya Completado (Estimado)

- Infrastructure (tests, profiles, docs): **~3-4 semanas de trabajo**
- CPU framework: **~2 semanas**
- ELF loader: **~1 semana**
- ARM64 backend: **~2 semanas** (comparar con Super3-NX)
- **Total invertido**: ~8-10 semanas

**Progreso Real**: 40% del camino a MVP Nivel 1.

---

## 8. RECOMENDACIONES FINALES

### 8.1 Prioridades Absolutas (Hacer AHORA)

1. **Implementar arquitectura de memoria (X28)** - sin esto, nada funciona
2. **Libc shim mínima (malloc, fopen, gettimeofday)** - 10-15 funciones
3. **Context switching del dispatcher** - inicializar ESP correctamente
4. **Conseguir o crear binario de test** - sin esto, no se puede validar

### 8.2 Prioridades Secundarias (Siguiente Sprint)

5. **Decoder x86: Jcc, CALL indirect, LEA con memoria**
6. **x87 FPU básico** (crítico para Lindbergh)
7. **Copiar deko3d init de DLSS-Switch**
8. **EEPROM synthesis** (copy-paste de lindbergh-loader)

### 8.3 Decisiones Arquitectónicas Validadas

✅ **MANTENER**:
- DBT con IR intermedia
- Separación core/platform
- Registros pineados x86→ARM64
- X28 como memory base
- Validación diferencial en tests
- Lindbergh como primer target

❌ **NO CAMBIAR**:
- No migrar a Box64/FEX (imposible por dependencias)
- No emular PC completo (innecesario y lento)
- No añadir Win32 todavía (bloqueado por Lindbergh primero)

### 8.4 Deuda Técnica Aceptable (Documentar, no Arreglar Ahora)

🟡 **Aceptar en MVP**:
- Lowering directo sin optimización de IR (es correcto, solo subóptimo)
- Bloques pequeños sin linking (optimización futura)
- malloc simple (first-fit/bump, no dlmalloc completo)
- Graphics sin batching (traducir 1:1 GL→deko3d primero)
- Sin threading (Lindbergh probablemente single-threaded en MVP)

### 8.5 Documentación

**Crear**:
- `docs/IMPLEMENTATION_STATUS.md` - actualizar con cada sprint
- `docs/MISSING_OPCODES.md` - lista de x86 opcodes por implementar
- `docs/LIBC_SURFACE.md` - símbolos libc necesarios (descubrir con binario real)

**Actualizar**:
- `docs/ROADMAP.md` - marcar ítems completados
- `docs/JIT.md` - añadir sección "Memory Translation" con código real
- `docs/MEMORY_ARCHITECTURE_ANALYSIS.md` - mover a `IMPLEMENTED.md` cuando esté hecho

---

## 9. COMANDO DE COMPILACIÓN

```bash
# BUILD PARA SWITCH (cuando tengas devkitPro instalado):
export DEVKITPRO=/opt/devkitpro   # o tu ruta
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
make -j$(nproc)

# Genera: pc-arcade-switch.nro

# BUILD DE HOST (para tests, sin Switch):
mkdir -p build-host
cd build-host
cmake .. -DPAS_BUILD_TESTS=ON
make -j$(nproc)
./tests/pas_tests

# DEPENDENCIAS OPCIONALES (Linux host, para tests avanzados):
sudo apt install gcc-multilib libc6-dev-i386           # tests x86 nativos
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu  # cross-compile ARM64
sudo apt install qemu-user                             # ejecución ARM64 en x86
```

---

## 10. CONCLUSIÓN

### Veredicto Final

Este proyecto tiene **bases arquitectónicas excelentes** y está siguiendo el camino correcto. El diseño es apropiado, la separación de responsabilidades es clara, y la metodología de validación (tests diferenciales, comparación con herramientas reales) es ejemplar.

**El problema NO es arquitectónico, es de completitud de implementación.**

### Estado Actual: 40% del MVP Nivel 1

**Lo que funciona**:
- ✅ Infrastructure sólida (tests, profiles, docs)
- ✅ ELF loader completo
- ✅ Pipeline JIT básico (para subconjunto sin memoria)
- ✅ ARM64 backend validado triple

**Lo que falta**:
- ❌ Traducción de accesos a memoria (BLOCKER #1)
- ❌ Libc shim (BLOCKER #2)
- ❌ Decoder x86 completo (x87, SSE, Jcc)
- ❌ Graphics backend
- ❌ Arcade devices

### Próximo Paso Inmediato

**IMPLEMENTAR ARQUITECTURA DE MEMORIA (X28)**

Todo el resto está bloqueado por esto. El diseño está completo en `docs/MEMORY_ARCHITECTURE_ANALYSIS.md`. Solo falta codificarlo.

**Estimación**: 3-4 días de trabajo enfocado.

**Después de eso**: libc shim (otros 3-4 días).

**Entonces**: el proyecto desbloqueará y podrá progresar rápidamente.

### Probabilidad de Éxito

✅ **ALTA** - Si se mantiene el rumbo actual:
- Arquitectura correcta
- Separación limpia
- Tests robustos
- Documentación excepcional
- Código reutilizable disponible (DLSS-Switch, Super3-NX)
- Referencia pública disponible (lindbergh-loader)

❌ **Riesgos**:
- Sin binario real, difícil validar
- Shader compilation es riesgo técnico abierto
- Rendimiento en Cortex-A57 desconocido (pero mitigable con perfilado)

**Tiempo estimado a primer frame**: **1.5-2 meses** con ingeniero senior dedicado.

---

**FIN DE LA AUDITORÍA TÉCNICA**

*Documento generado el 2026-09-04*  
*Revisar y actualizar con cada sprint completado*
