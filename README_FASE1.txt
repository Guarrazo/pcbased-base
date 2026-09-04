# FASE 1 — ZIP de archivos

## Archivos incluidos

### NUEVOS (copiar directamente)
- src/cpu/jit/x86_cpu_state.h
- tests/test_phase1_memory_stack.cpp

### MODIFICADOS (reemplazar los existentes en tu repo)
- src/cpu/translator/ir.h
- src/cpu/jit/arm64_codegen.h
- src/cpu/jit/arm64_codegen.cpp
- src/cpu/jit/jit.h
- src/cpu/jit/jit.cpp
- tests/CMakeLists.txt

### REESCRITO COMPLETO (⚠️ revisar merge)
- src/cpu/translator/ir_builder.cpp
  **IMPORTANTE**: No pude leer el ir_builder.cpp original de tu repo (error 500
  de GitHub), así que reconstruí una implementación completa desde cero.
  Si tenías código adicional en el original (hooks, patches inline, etc.),
  haz un diff antes de reemplazar y transfiere lo que falte.

## Pasos para aplicar

1. Haz backup de tu repo actual:
   cp -r pcbased-base pcbased-base-backup

2. Copia los archivos del ZIP sobre tu repo:
   cd pcbased-base
   cp -r /ruta/al/zip/src/* src/
   cp -r /ruta/al/zip/tests/* tests/

3. Compila para host:
   rm -rf build-host && mkdir build-host && cd build-host
   cmake .. -DPAS_BUILD_TESTS=ON
   make -j$(nproc)

4. Ejecuta tests:
   ./tests/pas_tests

5. Si falla la compilación, manda el output completo de error.

## Qué valida este test

El test test_phase1_memory_stack.cpp ejecuta código x86 real que:
- Usa PUSH/POP (pila)
- Usa MOV [mem], reg / MOV reg, [mem] (memoria)
- Usa SUB esp, 4 (aritmética con inmediato)
- Usa LEA implícito en el direccionamiento [ebp-4]
- Termina con RET

Si pasa: el JIT puede ejecutar código x86 con estado persistente,
memoria y pila. Eso desbloquea la FASE 2 (JMP/Jcc/CALL/RET + flags).
