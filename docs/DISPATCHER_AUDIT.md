# AUDITORÍA TÉCNICA COMPLETA: DISPATCHER JIT Y ARQUITECTURA DE MEMORIA

**Fecha**: 2026-09-04  
**Estado**: ANÁLISIS CRÍTICO - NO IMPLEMENTAR HASTA REVISIÓN COMPLETA  
**Objetivo**: Validar el dispatcher implementado y diseñar correctamente el soporte de operandos de memoria

---

## RESUMEN EJECUTIVO

El dispatcher JIT implementado **COMPILA pero NO ESTÁ VALIDADO** para casos reales. Existen **limitaciones críticas** que deben corregirse antes de continuar:

1. **NO hay tests** que validen transiciones multi-bloque o el dispatcher loop
2. **next_PC calculation es naive** - solo funciona para RET y fall-through lineal
3. **No se puede compilar el proyecto** en el entorno actual (cmake apunta a ruta incorrecta)
4. **La arquitectura de memoria NO está diseñada** - solo existe un placeholder en el IR

---

## 1. ESTADO REAL DEL DISPATCHER

### 1.1 Modificaciones Aplicadas

**Archivos modificados**:
- `src/cpu/translator/ir.h`: añadido campo `next_guest_address` a `Block`
- `src/cpu/translator/ir_builder.cpp`: cálculo de `next_guest_address` (líneas 173-180)
- `src/cpu/jit/arm64_codegen.cpp`: carga de `next_guest_address` en W0 antes de RET (líneas 95-101)
- `src/cpu/jit/jit.cpp`: dispatcher loop completo (líneas 27-68)

### 1.2 Qué Funciona (Verificado por Inspección)

✅ **Código consistente internamente**: las 4 piezas están conectadas correctamente  
✅ **Convención AAPCS64**: W0 como retorno es correcto  
✅ **Sentinel value**: 0xFFFFFFFF para "terminar" está implementado  
✅ **Dispatcher loop**: estructura del bucle es correcta (`while (current_pc != kExitSentinel)`)  
✅ **Invocación de bloques**: casting a `uint32_t (*)(void)` es correcto  
✅ **CodeCache lookup**: integración con cache funciona  

### 1.3 Qué NO Está Verificado (Requiere Ejecución Real)

❌ **Transiciones multi-bloque**: no hay test que ejecute A → B → C  
❌ **Preservación de registros guest**: no hay validación de que W20-W27 persisten entre bloques  
❌ **next_PC runtime**: solo hay un test estático de traducción, no ejecución de dispatcher  
❌ **Límite de iteraciones**: kMaxIterations=1M no ha sido ejercitado  
❌ **Error handling**: rutas de error en TranslateBlock() no han sido probadas  

### 1.4 Tests Existentes

El test actual `tests/test_jit_integration.cpp` **SOLO valida**:
```cpp
// Bloque único: ADD + AND + RET
uint8_t guest_memory[] = {0x83, 0xc0, 0x03, 0x83, 0xe0, 0x0f, 0xc3};
jit.RunFrom(0, guest_memory, sizeof(guest_memory));
```

**NO valida**:
- Dos bloques consecutivos sin saltos
- Bloques con CALL/JMP (cuando se implementen)
- Loops (branch back)
- Comportamiento del dispatcher con múltiples iteraciones

### 1.5 Compilación Bloqueada

**Problema**: cmake apunta a ruta incorrecta (`I:\ComfyUI_3D\python_embeded\python.exe`)  
**Impacto**: NO se puede compilar ni ejecutar tests reales  
**Workaround**: análisis estático solamente - **NO es suficiente para validación completa**

---

## 2. AUDITORÍA DE next_PC: LIMITACIONES CRÍTICAS

### 2.1 Implementación Actual (ir_builder.cpp:173-180)

```cpp
if (last_inst.opcode == x86::Opcode::Ret) {
    block.next_guest_address = 0xFFFFFFFF;  // sentinel: terminar
} else {
    // Dirección después de la última instrucción del bloque
    block.next_guest_address = last_inst.address + last_inst.length;
}
```

### 2.2 Casos que Funciona

✅ **RET**: termina correctamente con sentinel  
✅ **Fall-through lineal**: MOV + ADD + ... sin branches → siguiente PC secuencial  

### 2.3 Casos que FALLAN (Bloqueadores Críticos)

❌ **CALL rel32**: 
- **Problema**: después de CALL, next_PC debería ser la dirección de retorno (PC+5), pero el bloque termina en CALL
- **Comportamiento actual**: calcularía `CALL_address + 5` como next_PC, pero eso no es correcto - debería **entrar en la función**, no caer después
- **Solución requerida**: necesita `CallNative` en IR o manejo especial

❌ **JMP incondicional**: 
- **Problema**: `next_guest_address = JMP_address + length` es INCORRECTO - debe saltar al target
- **Información faltante**: el decoder captura el target en `operands[0].immediate`, pero IrBuilder no lo usa
- **Bloqueo**: sin esto, cualquier loop infinito o función multi-bloque falla

❌ **Jcc (branch condicional)**:
- **Problema**: tiene **DOS** next_PC posibles: target si se toma, fall-through si no
- **Limitación arquitectural**: el IR actual solo permite UN `next_guest_address` en el bloque
- **Solución requerida**: necesita `BranchConditional` en IR con TWO targets, no return value

❌ **JMP/CALL indirecto** (`jmp [eax]`, `call [ebx+0x10]`):
- **Problema**: el target es un **valor runtime** calculado en memoria
- **Imposibilidad**: NO se puede determinar en compile-time
- **Solución requerida**: el bloque debe **calcular el target en W0** en runtime, no usar `block.next_guest_address`

### 2.4 Diseño Correcto para Branches

**OPCIÓN A: Dispatcher pattern (actual, pero extendido)**
- Bloques devuelven next_PC en W0 (ya implementado)
- Para JMP/CALL directo: **calcular el target en codegen**, no en IrBuilder
- Para Jcc: necesita codegen que haga `if (condition) W0 = target; else W0 = fall_through;`
- Para indirect: calcular dirección efectiva en W0

**OPCIÓN B: Block linking (optimización futura, NO MVP)**
- Branches directos se patchean como saltos ARM64 directos (B/BR)
- Evita volver al dispatcher en cada transición
- Complicación: necesita invalidación y re-linking

**RECOMENDACIÓN**: extender OPCIÓN A con:
1. `ir::OpCode::Branch` para JMP incondicional → carga target en W0
2. `ir::OpCode::BranchConditional` con dos targets → codegen elige en runtime cuál cargar en W0
3. `ir::OpCode::Call` para CALL → necesita **push return address a stack guest** antes de branch

---

## 3. AUDITORÍA COMPLETA DE MEMORIA

### 3.1 Modelo de Memoria Guest (ElfLoader)

**Estructura actual** (`src/os/elf_loader/elf_loader.h`):
```cpp
struct LoadedElf {
    std::vector<uint8_t> guest_memory;  // buffer contiguo
    uint32_t base_address;              // vaddr mínimo de PT_LOAD
    
    uint8_t* AddressToPointer(uint32_t virtual_address) {
        if (virtual_address < base_address) return nullptr;
        uint64_t rel = virtual_address - base_address;
        if (rel >= guest_memory.size()) return nullptr;
        return guest_memory.data() + rel;
    }
};
```

**Características**:
- Memoria guest es un **std::vector<uint8_t>** continuo (NO ejecutable)
- Rango: `[base_address, base_address + size)`
- Traducción: `guest_vaddr → host_ptr = guest_memory[vaddr - base]`
- Código JIT vive en **memoria ejecutable separada** (`IExecutableMemory`, no en `guest_memory`)

### 3.2 Addressing Modes x86 Capturados por Decoder

El decoder **YA captura** toda la información de ModRM/SIB (`src/cpu/x86/decoder.h`):

```cpp
struct X86Operand {
    bool has_base, has_index;
    Reg base_reg, index_reg;
    uint8_t scale;  // 1,2,4,8
    int32_t disp;
};
```

**Ejemplos**:
- `[eax]` → has_base=true, base_reg=EAX, disp=0
- `[eax+0x10]` → has_base=true, base_reg=EAX, disp=0x10
- `[eax+ebx*4]` → has_base=true, base_reg=EAX, has_index=true, index_reg=EBX, scale=4
- `[eax+ebx*4+0x20]` → todas las anteriores + disp=0x20
- `[0x804c000]` → has_base=false, has_index=false, disp=0x804c000 (absoluto)
- `[esp-0x10]` → stack access típico

### 3.3 Representación en IR (FALTA DISEÑO)

**Estado actual**: el IR tiene placeholder `LoadMem`/`StoreMem` pero **NO están definidos**:

```cpp
enum class OpCode {
    LoadMem, StoreMem,  // acceso a memoria del proceso emulado
    // ...
};

struct Instruction {
    // NO hay campos para base/index/scale/disp
    uint8_t reg_index;  // solo sirve para LoadReg/StoreReg
};
```

**Problema**: NO hay forma de representar un operando de memoria completo.

### 3.4 Propuesta X28 como Memory Base

**Idea original**: reservar X28 permanentemente como `guest_memory.data()`

**Evaluación**:

✅ **Ventajas**:
- Un registro fijo simplifica codegen: `LDR W9, [X28, offset]`
- No necesita calcular base en cada acceso

❌ **Desventajas**:
- X28 está en la **ABI caller-saved zone** - requiere preservación explícita
- Conflicto con convención actual: W20-W27 = registros guest, X9-X15 = scratch
  - X28 no está documentado en `docs/JIT.md`
- **Problema arquitectural**: ¿qué pasa con direcciones fuera de `[base, base+size)`?
  - Accesos a stack pueden estar en regiones mapeadas dinámicamente
  - Direcciones absolutas tipo `mov eax, [0x804c000]` pueden estar fuera de rango

✅ **Casos que funciona bien**:
- `[reg+disp]` donde reg es un registro guest pinneado y está en rango
- Accesos relativos a data segment del ELF

❌ **Casos problemáticos**:
- **Stack**: ESP puede estar en región diferente a code/data del ELF
- **Heap**: malloc() en libc-shim puede asignar fuera del vector inicial
- **Múltiples regiones**: si hay memory-mapped I/O o device memory (JVS, etc.)
- **Address overflow**: si `base_reg + index*scale + disp` desborda, necesita check

### 3.5 Alternativas a X28

**OPCIÓN B: Helper function**
```cpp
// En cada LoadMem:
MOV X9, #guest_vaddr       // calcular dirección efectiva
BL  guest_memory_translate  // helper en C++ que traduce vaddr → host ptr
LDR W10, [X0]              // X0 = resultado del helper
```
- Más flexible (puede manejar múltiples regiones, faults, etc.)
- **MUY lento**: call overhead en cada acceso

**OPCIÓN C: X28 + bounds check**
```cpp
// Pseudo-código:
addr_guest = base_reg + index*scale + disp
offset = addr_guest - base_address
if (offset >= size) { fault_handler(); }
LDR W9, [X28, offset]
```
- Más robusto que X28 puro
- Overhead de branch en cada acceso

**OPCIÓN D: Hybrid - X28 fast path + slow path**
```cpp
// Fast path para accesos conocidos en rango (stack frame local, datos estáticos)
LDR W9, [X28, #offset]

// Slow path para indirecciones, heap, etc.
BL guest_memory_helper
```
- Requiere análisis en IrBuilder para clasificar accesos
- Complejo pero óptimo

### 3.6 Recomendación

**Para MVP (Lindbergh target real)**:

1. **Usar X28 como base** inicialmente, con estas restricciones documentadas:
   - SOLO funciona si `guest_memory` es un bloque contiguo
   - Accesos fuera de rango causan fallo (no hay recovery)
   - X28 debe preservarse en función boundaries (caller-save)

2. **IrBuilder debe validar** que el acceso está en rango conocido:
   - Stack accesses (ESP-based): validar que ESP está dentro del rango
   - Absolute accesses: verificar que la dirección cae en `[base, base+size)`
   - Si NO se puede garantizar → fallar en IrBuilder con mensaje claro

3. **Documentar limitaciones** en `docs/JIT.md`:
   - "Memory model assumes single contiguous guest_memory region"
   - "Out-of-bounds accesses cause translation failure (not runtime trap)"
   - "X28 reserved for memory base - not available for other uses"

4. **Plan de migración futura**:
   - Cuando se necesite memoria dinámica (malloc/mmap): migrar a helper function
   - O implementar page table en guest (más parecido a emulador real)

---

## 4. DISEÑO DE LoadMem/StoreMem

### 4.1 Extensión del IR

**Añadir a `ir::Instruction`**:
```cpp
struct Instruction {
    // ... campos existentes ...
    
    // Para LoadMem/StoreMem:
    struct MemoryOperand {
        bool has_base;
        bool has_index;
        uint8_t base_reg;   // index en el array de registros pinneados (0-7)
        uint8_t index_reg;
        uint8_t scale;      // 1,2,4,8
        int32_t disp;
        uint8_t size;       // 1,2,4 bytes
        bool sign_extend;   // para loads de 8/16 bits
    } mem;
};
```

### 4.2 IrBuilder: x86 → IR

**Para cada instrucción con operando de memoria**:

```cpp
// Ejemplo: MOV eax, [ebx+ecx*4+0x10]
// Decoder ya produjo: operand.has_base=true, base_reg=EBX, has_index=true, ...

ir::Instruction load;
load.op = ir::OpCode::LoadMem;
load.dst = NewValue();
load.mem.has_base = x86_operand.has_base;
load.mem.base_reg = static_cast<uint8_t>(x86_operand.base_reg);  // EBX=3
load.mem.has_index = x86_operand.has_index;
load.mem.index_reg = static_cast<uint8_t>(x86_operand.index_reg); // ECX=1
load.mem.scale = x86_operand.scale;  // 4
load.mem.disp = x86_operand.disp;    // 0x10
load.mem.size = 4;  // Dword
load.mem.sign_extend = false;
block.instructions.push_back(load);

// Luego: StoreReg EAX, load.dst
```

### 4.3 Arm64CodeGen: IR → ARM64

**Para LoadMem con X28 approach**:

```cpp
case ir::OpCode::LoadMem: {
    uint8_t result = AllocScratch();  // e.g. W9
    
    // Paso 1: calcular offset guest
    // offset = (base ? W_base : 0) + (index ? W_index*scale : 0) + disp
    
    uint8_t offset_reg = AllocScratch();  // e.g. W10
    
    if (inst.mem.has_base) {
        uint8_t base_arm = MappedRegister(inst.mem.base_reg);  // e.g. W23 si EBX
        emitter.EmitOrrReg(offset_reg, 31, base_arm, false);  // MOV W10, W23
    } else {
        emitter.EmitMovz(offset_reg, 0, 0, false);  // MOV W10, #0
    }
    
    if (inst.mem.has_index) {
        uint8_t index_arm = MappedRegister(inst.mem.index_reg);
        uint8_t temp = AllocScratch();  // W11
        
        // LSL temp, index_arm, #log2(scale)
        uint8_t shift = (inst.mem.scale == 8) ? 3 : (inst.mem.scale == 4) ? 2 : 
                        (inst.mem.scale == 2) ? 1 : 0;
        emitter.EmitLsl(temp, index_arm, shift, false);
        
        // ADD offset_reg, offset_reg, temp
        emitter.EmitAdd(offset_reg, offset_reg, temp, false);
    }
    
    if (inst.mem.disp != 0) {
        // ADD offset_reg, offset_reg, #disp (o secuencia si disp > 12 bits)
        EmitAddImmediate(emitter, offset_reg, offset_reg, inst.mem.disp, false);
    }
    
    // Paso 2: restar base_address del guest para obtener offset en el buffer
    // SUB offset_reg, offset_reg, #base_address
    uint32_t base = GetGuestMemoryBase();  // del ElfLoader
    EmitSubImmediate(emitter, offset_reg, offset_reg, base, false);
    
    // Paso 3: load desde [X28 + offset_reg]
    // LDR W9, [X28, W10, UXTW]  (extend 32→64 y añadir)
    switch (inst.mem.size) {
        case 1: emitter.EmitLdrbExtendReg(result, 28, offset_reg, false); break;
        case 2: emitter.EmitLdrhExtendReg(result, 28, offset_reg, false); break;
        case 4: emitter.EmitLdrExtendReg(result, 28, offset_reg, false); break;
    }
    
    value_location_[inst.dst.id] = result;
    break;
}
```

**PROBLEMA**: esto requiere **nuevos métodos en Emitter** que NO existen:
- `EmitLsl()`
- `EmitAddImmediate()` con immediate grande
- `EmitSubImmediate()` con immediate grande
- `EmitLdrbExtendReg()`, `EmitLdrhExtendReg()`, `EmitLdrExtendReg()`

### 4.4 Complejidad de Implementación

**Nuevos métodos en Emitter requeridos** (aprox. 8-12 funciones):
- Shifts (LSL/LSR/ASR)
- ADD/SUB con immediate de 32 bits (secuencia MOVZ+MOVK+ADD)
- Load/Store con extend mode (UXTW/SXTW)
- Load/Store byte/halfword

**Código estimado**: ~200-300 líneas en `emitter.cpp` + tests

---

## 5. IMPACTO EN OTROS COMPONENTES

### 5.1 Registros Guest

**Actual**: W20-W27 = EAX-EDI (pinned)  
**Impacto**: necesita ESP para stack accesses  
**Solución**: ESP YA está en W24 según convención (Reg::Esp=4 → W20+4=W24)

### 5.2 Flags

**Actual**: NO implementadas - CompareAndSetFlags es placeholder  
**Impacto con memoria**: instrucciones tipo `cmp [eax], 5` necesitan LoadMem + Sub + SetFlags  
**Órden**: implementar flags DESPUÉS de memoria básica funcional

### 5.3 Stack (PUSH/POP)

**Decoder**: PUSH/POP ya decodificados  
**IR**: necesita traducción a `LoadMem [ESP]` + `StoreMem [ESP]` + ajuste de ESP  
**Complicación**: ESP debe actualizarse DENTRO del IR, no solo en codegen

**Ejemplo PUSH EAX**:
```cpp
// x86: push eax  →  ESP -= 4; [ESP] = EAX

// IR correcto:
temp1 = LoadReg ESP
temp2 = LoadImm 4
temp3 = Sub temp1, temp2
StoreReg ESP, temp3
temp4 = LoadReg EAX
StoreMem [ESP+0], temp4  // usa el ESP actualizado
```

### 5.4 Code Cache Invalidation

**Problema**: si el guest escribe en memoria de código (self-modifying code)  
**Detección**: NO implementada actualmente  
**Solución MVP**: asumir que NO hay SMC en binarios Lindbergh (válido para código compilado normal)  
**Plan futuro**: necesita memory protection o write tracking

---

## 6. TESTS REQUERIDOS

### 6.1 ANTES de Implementar Memoria

**Test 1: Dispatcher Multi-Block**
```cpp
// Bloque A: MOV EAX, 10
uint8_t block_a[] = {0xB8, 0x0A, 0x00, 0x00, 0x00};  // termina sin RET

// Bloque B: ADD EAX, 5
uint8_t block_b[] = {0x83, 0xC0, 0x05};

// Memoria guest: A seguido de B
uint8_t guest[] = {0xB8, 0x0A, 0x00, 0x00, 0x00,  // offset 0
                   0x83, 0xC0, 0x05,              // offset 5
                   0xC3};                         // offset 8: RET

jit.RunFrom(0, guest, sizeof(guest));
// Verificar: dispatcher ejecutó 3 bloques (A, B, RET), EAX=15
```

**Test 2: Registro Preservation**
```cpp
// Bloque A: MOV EAX, 1; MOV EBX, 2
// Bloque B: ADD EAX, EBX
// Verificar que EBX=2 persiste entre bloques
```

**Test 3: Límite de Iteraciones**
```cpp
// Crear loop infinito (cuando se implemente JMP)
// Verificar que kMaxIterations detiene la ejecución
```

### 6.2 DESPUÉS de Implementar Memoria

**Test 4: Load Inmediato Absoluto**
```cpp
// MOV EAX, [0x1000]
// Pre-llenar guest_memory[0x1000] = 0x12345678
// Verificar EAX = 0x12345678
```

**Test 5: Store Register a Memoria**
```cpp
// MOV EAX, 42
// MOV [0x2000], EAX
// Verificar guest_memory[0x2000] = 42
```

**Test 6: Addressing Mode Complejo**
```cpp
// MOV EBX, 0x1000
// MOV ECX, 2
// MOV EAX, [EBX+ECX*4+0x10]
// Pre-llenar guest_memory[0x1018] = 99
// Verificar EAX = 99
```

**Test 7: Stack Operations**
```cpp
// MOV EAX, 123
// PUSH EAX
// MOV EAX, 0
// POP EAX
// Verificar EAX = 123 y ESP restaurado
```

---

## 7. ORDEN ÓPTIMO DE IMPLEMENTACIÓN

### Fase 1: Validar Dispatcher Actual (CRÍTICO)

1. **Arreglar entorno de compilación**
   - Diagnosticar cmake config issue
   - O crear Makefile alternativo mínimo

2. **Implementar Test Multi-Block**
   - Sin saltos, solo fall-through
   - Validar que dispatcher loop funciona

3. **Arreglar next_PC para bloques sin RET**
   - Implementar fall-through correcto en IrBuilder

### Fase 2: Branches Básicos (Preparación para Real Code)

4. **Implementar JMP incondicional**
   - Añadir `ir::OpCode::Branch`
   - Codegen carga target en W0
   - Test: loop simple

5. **Implementar CALL básico**
   - Añadir `ir::OpCode::Call`
   - **BLOQUEADOR**: necesita stack (PUSH return address)
   - → Fuerza implementar memoria AHORA

### Fase 3: Memoria (SIGUIENTE BLOQUEADOR)

6. **Extender Emitter con nuevas instrucciones ARM64**
   - LSL/LSR/ASR
   - ADD/SUB immediate large
   - Load/Store variants
   - **Tests unitarios para cada nuevo método**

7. **Extender IR con MemoryOperand**
   - Añadir struct a `ir::Instruction`
   - Modificar IrBuilder para MOV con memoria

8. **Implementar codegen LoadMem/StoreMem**
   - Con X28 approach
   - Documentar limitaciones

9. **Implementar PUSH/POP**
   - Ahora que memoria y stack accesses funcionan
   - Test stack operations

### Fase 4: Control Flow Completo

10. **Implementar Jcc (branches condicionales)**
    - Requiere flags (implementar CompareAndSetFlags)
    - Requiere BranchConditional con dos targets

11. **Implementar indirect branches**
    - JMP [reg], CALL [reg+disp]
    - Runtime calculation de target

---

## 8. RIESGOS DE ARQUITECTURA

### RIESGO 1: X28 es Insuficiente para Código Real

**Probabilidad**: ALTA  
**Impacto**: RE-ARQUITECTURA completa de memoria  
**Trigger**: cuando veamos malloc/mmap en binarios reales  
**Mitigación**: documentar asunción y tener plan B (helper function)

### RIESGO 2: Emitter Incompleto para Memory Addressing

**Probabilidad**: MEDIA  
**Impacto**: 1-2 semanas de trabajo adicional en Emitter  
**Trigger**: necesitar modes que no están implementados  
**Mitigación**: implementar incrementalmente según necesidad real

### RIESGO 3: Self-Modifying Code en Lindbergh

**Probabilidad**: BAJA (binarios compilados normalmente no usan SMC)  
**Impacto**: necesita code cache invalidation (grande)  
**Trigger**: ejecutar binario real y ver comportamiento raro  
**Mitigación**: MVP asume NO SMC, implementar si es necesario

### RIESGO 4: Stack No Está en guest_memory

**Probabilidad**: MEDIA  
**Impacto**: X28 approach falla para stack accesses  
**Trigger**: ESP apunta fuera de `[base, base+size)`  
**Mitigación**: ElfLoader debe reservar stack en `guest_memory` inicial

### RIESGO 5: Dispatcher es Demasiado Lento

**Probabilidad**: BAJA para MVP  
**Impacto**: necesita block linking (optimización grande)  
**Trigger**: profile muestra >50% tiempo en dispatcher  
**Mitigación**: MVP tolera lentitud, optimizar después

---

## 9. DECISIONES TÉCNICAS RECOMENDADAS

### ✅ APROBAR PARA MVP:

1. **Dispatcher pattern** (retorno a C++ en cada bloque)
2. **X28 como memory base** con limitaciones documentadas
3. **LoadMem/StoreMem en IR** con struct MemoryOperand completo
4. **next_PC calculation extendido** para JMP/CALL/Jcc
5. **Implementación incremental** del Emitter según necesidad

### ❌ RECHAZAR PARA MVP:

1. Block linking (optimización futura)
2. Helper function para memoria (demasiado lento)
3. Page table guest (overkill para target actual)
4. Full x86 flags (implementar solo los que realmente se usen)

### ⚠️ INVESTIGAR ANTES DE DECIDIR:

1. **¿ESP está dentro de guest_memory?** → necesita verificar ElfLoader
2. **¿Hay SMC en Lindbergh?** → buscar en documentación/forums
3. **¿Qué calling conventions usa libc-shim?** → afecta CallNative

---

## 10. SIGUIENTE PASO CONCRETO

**NO implementar todavía.**

**PRIMERO**: arreglar entorno de compilación y validar dispatcher con test multi-block.

**DESPUÉS**: si dispatcher funciona, aprobar esta arquitectura y crear SPEC para:

```
SPEC: Implementar Soporte de Operandos de Memoria en JIT

Requisitos:
- Extender Emitter con LSL, ADD/SUB imm32, LDR/STR variants
- Extender IR con MemoryOperand struct
- Implementar IrBuilder lowering de MOV con memoria
- Implementar Arm64CodeGen para LoadMem/StoreMem
- Tests para cada addressing mode
- Documentar convención X28

Bloqueadores conocidos:
- Requiere X28 free (verificar no usado en código actual)
- Requiere base_address disponible en codegen (pasar como contexto)
- Requiere tests de Emitter antes de integración

Estimación: NO DAR HASTA VER EVIDENCIA DE COMPILACIÓN EXITOSA
```

---

## CONCLUSIÓN

El dispatcher **NO está validado** pero el diseño es **architecturally sound**.  

Los **bloqueadores críticos** son:
1. Falta de tests de ejecución real
2. next_PC naive (no soporta branches)
3. Memoria NO diseñada

**Ruta crítica**:
1. Arreglar cmake → compilar → validar dispatcher con test multi-block
2. Implementar branches básicos (JMP)
3. Implementar memoria (LoadMem/StoreMem con X28)
4. Implementar PUSH/POP (requiere memoria funcionando)
5. Entonces y solo entonces: probar con binario real simple

**NO continuar con memoria hasta que dispatcher esté EJECUTANDO correctamente un test multi-block.**
