# IMPLEMENTACIÓN: DISPATCHER JIT FUNCIONAL

## Estado Actual
- Jit::RunFrom() traduce el primer bloque pero no lo ejecuta
- Los bloques ARM64 terminan en RET pero no devuelven next_PC
- No hay loop de dispatch

## Cambios Necesarios

### 1. Modificar Jit::RunFrom() (src/cpu/jit/jit.cpp)
- Implementar loop: while(pc != SENTINEL)
- Invocar bloques como: uint32_t (*fn)(void)
- Capturar next_PC del retorno en W0
- Traducir a demanda si bloque no existe

### 2. Modificar Arm64CodeGen::Generate() (src/cpu/jit/arm64_codegen.cpp)
- Antes de EmitRet(): cargar next_PC en W0
- Para Return (terminar): MOV W0, #0xFFFFFFFF
- Para bloques que continúan: calcular next_PC y cargarlo

### 3. Extender IR para almacenar next_PC
- Añadir campo next_guest_address a ir::Block
- IrBuilder debe capturar la dirección siguiente al terminar bloque
- Para RET: 0xFFFFFFFF (sentinel)
- Para otros: dirección de la siguiente instrucción

## Convención de Llamada
- Bloques tienen firma: uint32_t (*)(void)
- Retornan en W0 (AAPCS64)
- 0xFFFFFFFF = terminar ejecución
- Otro valor = siguiente dirección guest a ejecutar

## Verificación
- Compilar sin errores
- Tests existentes deben seguir pasando
- Nuevo test: bloque sintético que retorna sentinel

---
Generado por: agente de continuación
