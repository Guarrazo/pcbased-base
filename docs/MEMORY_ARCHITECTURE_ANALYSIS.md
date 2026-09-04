# ANÁLISIS TÉCNICO: ARQUITECTURA DE MEMORIA PARA JIT x86→ARM64

**Fecha**: 2026-09-04  
**Contexto**: Emulador Sega Lindbergh (x86-32 PC-based arcade)  
**Objetivo**: Diseñar la traducción de accesos de memoria guest sin introducir arquitectura incorrecta

---

## PROBLEMA A RESOLVER

Los binarios x86 acceden a memoria con direcciones virtuales de 32 bits. El JIT ARM64 debe traducir estos accesos a punteros host reales sin:

1. Emular una MMU completa (demasiado lento)
2. Asumir que guest y host usan el mismo address space (imposible en DBT)
3. Requerir traducción en runtime para cada acceso (mata performance)

---

## CONTEXTO DEL TARGET REAL

### Lindbergh Memory Layout Típico

Basado en arquitectura PC-based x86 Linux estándar de mediados-2000s:

```
0x08048000 - 0x08xxxxxx   .text (código)
0x08xxxxxx - 0x08yyyyyy   .data/.bss
[heap dinámico]           malloc/mmap
0xBFxxxxxx - 0xBFFFFFFF   stack (crece hacia abajo desde ~0xBFFFFFFF)
```

**Características**:
- Binarios NO son PIE (Position Independent Executable) → direcciones fijas
- Stack en top of memory (~3GB user space)
- Heap puede crecer significativamente (juegos con assets grandes)
- NO hay segments en el sentido tradicional (CS/DS/SS) - flat memory model

### Patrones de Acceso Esperados

**Alto volumen** (hot path):
- Stack frame local: `[esp-0x10]`, `[ebp+0x8]`
- Global data: `mov eax, [0x804c000]` (direcciones absolutas)
- Structured access: `mov eax, [ebx+ecx*4]` (arrays/structs)

**Medio volumen**:
- Function parameters: `[esp+4]`, `[esp+8]`
- Heap indirection: `mov eax, [ptr]; mov ebx, [eax+offset]`

**Bajo volumen**:
- String operations (rep movs)
- memcpy/memset desde libc-shim

---

## OPCIÓN 1: REGISTRO BASE (X28)

### Diseño

```
guest_memory = std::vector<uint8_t> contiguo
base_address = vaddr mínimo de los PT_LOAD del ELF

X28 = &guest_memory[0]  (host pointer, reservado permanentemente)

Para acceder a guest vaddr V:
  offset = V - base_address
  host_ptr = X28 + offset
  LDR Wn, [X28, offset]
```

### Implementación en Codegen

```armasm
; MOV EAX, [EBX+ECX*4+0x10]  (x86)

; Paso 1: calcular effective address guest
MOV  W9, W23              ; W23 = EBX (guest reg pinned)
LSL  W10, W21, #2         ; W21 = ECX, shift left 2 (scale=4)
ADD  W9, W9, W10
ADD  W9, W9, #0x10        ; disp

; Paso 2: convertir a offset en buffer
SUB  W9, W9, #base_addr   ; restar base del ELF

; Paso 3: load desde X28
LDR  W20, [X28, W9, UXTW] ; W20 = EAX, extend W9 a 64-bit y añadir a X28
```

**Instrucciones ARM64**: ~5-8 por acceso (dependiendo de complejidad de addressing mode)

### Ventajas

✅ **Fast path óptimo**: load/store son solo 1 instrucción ARM64 una vez calculado el offset  
✅ **Sin overhead de call**: todo inline en el bloque JIT  
✅ **Predecible**: latencia constante, friendly para branch predictor  
✅ **Simple de implementar**: no requiere runtime support complejo  

### Desventajas

❌ **Requiere memoria contigua**: heap dinámico (malloc) rompe el modelo  
❌ **Bounds checking manual**: offset fuera de rango = comportamiento indefinido  
❌ **X28 no disponible**: un registro menos para codegen (aunque tampoco se usa mucho en AAPCS64)  
❌ **Fixed base address assumption**: si guest usa múltiples memory maps (mmap), falla  

### Casos que Funciona

✅ Código estático (`.text`, `.data`, `.bss`) dentro del ELF inicial  
✅ Stack si está pre-allocado dentro de `guest_memory`  
✅ Heap pequeño si ElfLoader reserva región de heap en el vector inicial  

### Casos que Falla

❌ `malloc()` que asigna fuera del vector (requiere extender guest_memory dinámicamente)  
❌ `mmap()` que mapea archivo en dirección arbitraria  
❌ Memory-mapped I/O (si Lindbergh usa ioctl para JVS/EEPROM)  
❌ Shared memory entre procesos (no aplica en target, pero sería bloqueador)  

---

## OPCIÓN 2: HELPER FUNCTION

### Diseño

```cpp
// Runtime C++
uint8_t* TranslateGuestAddress(uint32_t guest_vaddr) {
    // Buscar en tabla de regiones
    for (auto& region : memory_regions) {
        if (guest_vaddr >= region.base && guest_vaddr < region.base + region.size) {
            return region.host_ptr + (guest_vaddr - region.base);
        }
    }
    // Fault handling: segfault, lazy allocate, etc.
    return HandleMemoryFault(guest_vaddr);
}
```

**Codegen**:
```armasm
; MOV EAX, [EBX+0x10]

; Calcular guest effective address
ADD  W9, W23, #0x10       ; W23 = EBX

; Call helper
MOV  W0, W9               ; arg 0 = guest address
BL   TranslateGuestAddress
; X0 = host pointer

; Load desde resultado
LDR  W20, [X0]            ; W20 = EAX
```

### Ventajas

✅ **Flexible**: soporta múltiples regiones, heap dinámico, mmap, etc.  
✅ **Puede implementar bounds checking robusto**  
✅ **Permite lazy allocation**: HandleMemoryFault puede extender memoria bajo demanda  
✅ **No requiere reservar X28**  

### Desventajas

❌ **EXTREMADAMENTE LENTO**: call overhead en CADA acceso a memoria  
  - Branch misprediction (indirect call)
  - Register spill/restore alrededor del call
  - No inline en bloque JIT
❌ **Mata locality**: dispatcher loop + helper calls = cache thrashing  
❌ **Difícil de optimizar**: compiler no puede ver través de la indirection  

### Estimación de Performance

Asumiendo ~10 ciclos de overhead por call (conservador):
- Instrucción con 2 memory operands (e.g., `add [eax], ebx`) = ~20 ciclos overhead
- Loop típico con 5 mem accesses/iteration = 50 ciclos overhead/iteration
- Comparado con X28 approach: ~5-10x slower

**CONCLUSIÓN**: NO viable para MVP sin profile evidence que muestre que no es el bottleneck.

---

## OPCIÓN 3: SOFTWARE TLB (Translation Lookaside Buffer)

### Diseño

```cpp
struct TLBEntry {
    uint32_t guest_page;  // bits [31:12] de guest vaddr
    uint64_t host_base;   // ptr a la página host
};

TLBEntry tlb[256];  // cache de traducciones recientes
```

**Codegen**:
```armasm
; MOV EAX, [EBX+0x10]

ADD  W9, W23, #0x10         ; guest effective address

; TLB lookup
LSR  W10, W9, #12           ; page number
AND  W10, W10, #0xFF        ; TLB index (256 entries)
ADRP X11, tlb
ADD  X11, X11, :lo12:tlb
LDR  X12, [X11, W10, LSL #3] ; X12 = tlb[index].host_base
LSR  W13, W12, #32          ; extract guest_page from TLBEntry
CMP  W13, W9, LSR #12       ; compare guest pages
B.NE tlb_miss

; TLB hit: extract host pointer
AND  X14, X12, #0xFFFFFFFF  ; host_base (lower 32 bits)
AND  W15, W9, #0xFFF        ; page offset
LDR  W20, [X14, W15]        ; load from host
B    done

tlb_miss:
BL   TLBMissHandler         ; slow path
; ...
done:
```

**Instrucciones**: ~12-15 en fast path (hit), ~30+ en slow path (miss)

### Ventajas

✅ **Balance speed/flexibility**: fast path razonable cuando hit rate es alto  
✅ **Soporta múltiples regiones**: TLB puede mapear guest pages a diferentes host regions  
✅ **Industry standard**: usado en emuladores reales (QEMU, Dolphin)  

### Desventajas

❌ **Complejidad alta**: necesita implementar TLB miss handler, eviction policy, invalidation  
❌ **Overhead significativo vs X28**: 12+ instrucciones vs 5-8  
❌ **Cache pressure**: TLB table ocupa cache lines  
❌ **Overkill para MVP**: target Lindbergh probablemente no necesita este nivel de flexibilidad  

---

## OPCIÓN 4: HYBRID (X28 + Fallback)

### Diseño

**Clasificación en IrBuilder**:
```cpp
enum class MemoryAccessType {
    StaticKnownInRange,   // [0x804c000] donde 0x804c000 está en [base, base+size)
    StackLocal,           // [esp-offset] donde offset < stack_frame_size
    Dynamic,              // [eax], [ebx+ecx*4], etc. - runtime address
};
```

**Codegen según tipo**:
- `StaticKnownInRange`: inline con X28, sin bounds check
- `StackLocal`: inline con X28, puede hacer bounds check si paranoid
- `Dynamic`: call a helper (slow path)

### Ejemplo

```cpp
// Análisis en IrBuilder:
if (operand.kind == Memory && !operand.has_base && !operand.has_index) {
    // Absolute address
    uint32_t addr = static_cast<uint32_t>(operand.disp);
    if (addr >= base && addr < base + size) {
        inst.mem.access_type = StaticKnownInRange;
        inst.mem.known_offset = addr - base;
    } else {
        // Fuera de rango: error en traducción, no en runtime
        return Error("Address out of range");
    }
} else if (operand.has_base && operand.base_reg == Reg::Esp && !operand.has_index) {
    inst.mem.access_type = StackLocal;
} else {
    inst.mem.access_type = Dynamic;
}
```

### Ventajas

✅ **Best of both worlds**: fast path para casos comunes, flexibility para edge cases  
✅ **Incremental**: empezar con X28-only, añadir fallback cuando se necesite  
✅ **Profile-guided**: puede decidir según qué accesos son realmente hot  

### Desventajas

❌ **Complejidad de implementación**: necesita dos code paths en codegen  
❌ **Análisis no trivial**: determinar "known in range" requiere tracking de valores  
❌ **Potencialmente inconsistente**: mixing fast/slow puede causar bugs sutiles  

---

## DECISIÓN PARA MVP: X28 CON RESTRICCIONES DOCUMENTADAS

### Justificación

1. **Target Lindbergh es específico**: no es un emulador de PC genérico
   - Binarios compilados estáticamente
   - Heap dinámico probablemente pequeño (assets se cargan a direcciones pre-conocidas)
   - Stack se puede pre-allocar en el vector inicial

2. **Performance es crítico**: arcade games corren a 60fps locked
   - Helper function mataría performance
   - TLB es overkill para memoria flat

3. **Incremental**: si X28 no basta, migrar a hybrid es factible
   - Código existente no cambia (X28 sigue siendo el fast path)
   - Solo añade slow path para casos dinámicos

### Restricciones Documentadas

**DEBE cumplirse**:
- `guest_memory` es un buffer contiguo que contiene TODO el address space usado
- ElfLoader DEBE reservar región de heap en el vector inicial (e.g., 64MB extra después de .bss)
- Stack DEBE estar en `guest_memory` (pre-allocated en top o bottom del buffer)
- Accesos fuera de `[base_address, base_address+size)` causan **fallo de traducción** (no runtime trap)

**Limitaciones aceptadas**:
- `malloc()` solo puede asignar dentro del heap pre-reservado
  - Si se queda sin espacio: crash o error (no grow dinámico)
- `mmap()` NO soportado en MVP (shim devuelve error)
- Memory-mapped I/O NO soportado (devices como JVS usan ioctl-shim, no mmap)

### Plan de Migración si Falla

**Señales de que X28 no basta**:
1. Binario real hace `malloc()` grande que excede heap pre-reservado
2. Binario usa `mmap()` para cargar assets
3. Stack overflow porque pre-alloc fue insuficiente

**Migración a hybrid**:
1. Añadir `MemoryManager` class que trackea múltiples regiones
2. Codegen emite fast path X28 para stack/globals
3. Codegen emite call a `MemoryManager::Translate()` para heap/dynamic
4. **NO requiere cambiar dispatcher ni IR** - solo codegen

---

## IMPLEMENTACIÓN CONCRETA PARA X28

### Paso 1: Modificar ElfLoader

```cpp
std::optional<LoadedElf> ElfLoader::Load(...) {
    // ... código actual calcula min_vaddr, max_vaddr_end ...
    
    // NUEVO: reservar heap y stack
    const size_t kHeapSize = 64 * 1024 * 1024;  // 64MB heap
    const size_t kStackSize = 8 * 1024 * 1024;  // 8MB stack
    
    size_t total_size = (max_vaddr_end - min_vaddr) + kHeapSize + kStackSize;
    out.guest_memory.assign(total_size, 0);
    
    out.heap_start = max_vaddr_end;
    out.heap_end = out.heap_start + kHeapSize;
    out.stack_start = out.heap_end;
    out.stack_end = out.stack_start + kStackSize;
    
    // Inicializar ESP al top del stack
    out.initial_esp = out.stack_end - 4;  // crece hacia abajo
    
    PAS_LOG_INFO("ElfLoader", "Memory layout: code/data [0x%08x-0x%08x], "
                              "heap [0x%08x-0x%08x], stack [0x%08x-0x%08x]",
                 min_vaddr, max_vaddr_end, 
                 out.heap_start, out.heap_end,
                 out.stack_start, out.stack_end);
    
    // ... resto del load ...
}
```

### Paso 2: Pasar X28 al Dispatcher

```cpp
// jit.cpp
void Jit::RunFrom(uint32_t guest_entry_point, const uint8_t* guest_memory_base,
                  size_t guest_memory_size, uint32_t initial_esp) {
    // Inicializar registros guest (incluyendo ESP)
    InitializeGuestRegisters(initial_esp);
    
    // X28 setup - DEBE hacerse ANTES del primer bloque
    SetMemoryBase(guest_memory_base);
    
    // ... dispatcher loop ...
}
```

**PROBLEMA**: ¿cómo se pasa X28 a los bloques JIT?

**OPCIÓN A**: caller-saved convention
- Dispatcher hace `MOV X28, <base>` antes de llamar a cada bloque
- Bloques asumen X28 válido, pero NO lo preservan
- **Problema**: si un bloque hace call a libc-shim, X28 se pierde

**OPCIÓN B**: callee-saved convention
- X28 se inicializa UNA VEZ al inicio
- Bloques JIT NO tocan X28
- libc-shim DEBE preservar X28
- **Ventaja**: más eficiente (no re-load en cada bloque)

**RECOMENDACIÓN**: OPCIÓN B, pero requiere documentar en `docs/JIT.md`:

> **X28 Calling Convention**
> 
> X28 is reserved as the guest memory base pointer for the duration of JIT execution.
> - Initialized once by `Jit::RunFrom()` before entering dispatcher loop
> - ALL generated blocks assume X28 points to `guest_memory.data()`
> - libc-shim, GL-shim, Cg-shim functions MUST preserve X28 (callee-saved)
> - X28 is NOT available for scratch use in codegen

### Paso 3: Codegen LoadMem

```cpp
// arm64_codegen.cpp
case ir::OpCode::LoadMem: {
    // Calcular effective address guest en scratch register
    uint8_t addr_reg = EmitEffectiveAddress(inst.mem, emitter);
    
    // Convertir a offset en buffer
    uint32_t base = GetGuestMemoryBase();  // pasado como contexto al codegen
    uint8_t offset_reg = AllocScratch();
    EmitSubImmediate32(emitter, offset_reg, addr_reg, base);
    
    // Load desde [X28 + offset]
    uint8_t result = AllocScratch();
    switch (inst.mem.size) {
        case 1:
            if (inst.mem.sign_extend) {
                emitter.EmitLdrsbExtendReg(result, 28, offset_reg);
            } else {
                emitter.EmitLdrbExtendReg(result, 28, offset_reg);
            }
            break;
        case 2:
            if (inst.mem.sign_extend) {
                emitter.EmitLdrshExtendReg(result, 28, offset_reg);
            } else {
                emitter.EmitLdrhExtendReg(result, 28, offset_reg);
            }
            break;
        case 4:
            emitter.EmitLdrExtendReg(result, 28, offset_reg);
            break;
    }
    
    value_location_[inst.dst.id] = result;
    break;
}
```

**Helper**:
```cpp
uint8_t Arm64CodeGen::EmitEffectiveAddress(const ir::MemoryOperand& mem,
                                            cpu::arm64::Emitter& emitter) {
    uint8_t result = AllocScratch();
    
    if (mem.has_base) {
        uint8_t base_arm = MappedRegister(mem.base_reg);
        emitter.EmitOrrReg(result, 31, base_arm, false);  // MOV result, base
    } else {
        emitter.EmitMovz(result, 0, 0, false);  // MOV result, #0
    }
    
    if (mem.has_index) {
        uint8_t index_arm = MappedRegister(mem.index_reg);
        uint8_t scaled = AllocScratch();
        
        // LSL scaled, index, #log2(scale)
        uint8_t shift = ...;  // calcular de mem.scale
        emitter.EmitLsl(scaled, index_arm, shift, false);
        
        // ADD result, result, scaled
        emitter.EmitAdd(result, result, scaled, false);
    }
    
    if (mem.disp != 0) {
        EmitAddImmediate32(emitter, result, result, mem.disp);
    }
    
    return result;
}
```

### Paso 4: Nuevas Instrucciones en Emitter

**Requeridas** (NO existen actualmente):
- `EmitLsl(rd, rn, shift, is64)` - Logical Shift Left
- `EmitAddImmediate32(rd, rn, imm32)` - ADD con immediate de 32 bits (secuencia MOVZ+MOVK+ADD)
- `EmitSubImmediate32(rd, rn, imm32)` - SUB con immediate de 32 bits
- `EmitLdrbExtendReg(rd, rn, rm)` - LDRB Wd, [Xn, Wm, UXTW]
- `EmitLdrhExtendReg(rd, rn, rm)` - LDRH Wd, [Xn, Wm, UXTW]
- `EmitLdrExtendReg(rd, rn, rm)` - LDR Wd, [Xn, Wm, UXTW]
- `EmitLdrsbExtendReg(rd, rn, rm)` - LDRSB Wd, [Xn, Wm, UXTW]
- `EmitLdrshExtendReg(rd, rn, rm)` - LDRSH Wd, [Xn, Wm, UXTW]
- `EmitStrbExtendReg(...)` - store variants

**Estimación**: ~300 líneas de código en `emitter.cpp` + ~200 líneas de tests

---

## TESTING STRATEGY

### Unit Tests (Emitter)

```cpp
TEST(Emitter, LdrExtendReg) {
    uint8_t buf[16];
    Emitter e(buf, sizeof(buf));
    e.EmitLdrExtendReg(/*rd=*/9, /*rn=*/28, /*rm=*/10);
    
    // LDR W9, [X28, W10, UXTW]
    // encoding: 0xB8 6A 5B 39  (verificar contra ARM ARM)
    EXPECT_EQ(...);
}
```

### Integration Tests (IrBuilder → Codegen)

```cpp
TEST(MemoryAccess, AbsoluteLoad) {
    // MOV EAX, [0x1000]
    uint8_t code[] = {0xA1, 0x00, 0x10, 0x00, 0x00};  // MOV EAX, moffs32
    
    Decoder decoder;
    X86Instruction inst;
    ASSERT_TRUE(decoder.DecodeOne(code, sizeof(code), 0, inst));
    
    IrBuilder builder;
    Block block = builder.BuildBlock(&inst, 1);
    
    // Verificar que IrBuilder emite LoadMem con operando correcto
    ASSERT_EQ(block.instructions[0].op, OpCode::LoadMem);
    ASSERT_FALSE(block.instructions[0].mem.has_base);
    ASSERT_FALSE(block.instructions[0].mem.has_index);
    ASSERT_EQ(block.instructions[0].mem.disp, 0x1000);
}
```

### End-to-End Tests (JIT Execution)

```cpp
TEST(JitMemory, LoadStore) {
    // Setup guest memory con base_address = 0x1000
    std::vector<uint8_t> guest(0x2000, 0);
    uint32_t base = 0x1000;
    
    // Pre-fill [0x1500] = 0x12345678
    *reinterpret_cast<uint32_t*>(&guest[0x500]) = 0x12345678;
    
    // Código: MOV EAX, [0x1500]; RET
    uint8_t code[] = {0xA1, 0x00, 0x15, 0x00, 0x00, 0xC3};
    std::memcpy(&guest[0], code, sizeof(code));
    
    auto exec_mem = std::make_unique<FakeExecutableMemory>(4096);
    Jit jit(std::move(exec_mem));
    
    // Inicializar X28 con guest.data()
    jit.SetMemoryBase(guest.data(), base);
    
    jit.RunFrom(base, guest.data(), guest.size());
    
    // Verificar EAX = 0x12345678
    EXPECT_EQ(jit.GetGuestRegister(Reg::Eax), 0x12345678);
}
```

---

## CONCLUSIÓN

**X28 approach es VIABLE para MVP Lindbergh** con estas condiciones:

1. ElfLoader reserva heap+stack en `guest_memory` inicial (no grow dinámico)
2. X28 es callee-saved en toda la ABI (JIT blocks, shims, etc.)
3. Accesos fuera de rango fallan en traducción (no en runtime)
4. Plan de migración a hybrid existe si se encuentran limitaciones

**Próximo paso**: implementar DESPUÉS de validar dispatcher básico funciona.
