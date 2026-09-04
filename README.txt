# FASE 1 Completa — ZIP de archivos

## Archivos incluidos (todos listos para copiar sobre tu repo)

### NUEVOS
- src/cpu/jit/x86_cpu_state.h
- tests/test_phase1_memory_stack.cpp

### MODIFICADOS (reemplazar los existentes)
- src/cpu/translator/ir.h
- src/cpu/jit/arm64_codegen.h
- src/cpu/jit/arm64_codegen.cpp
- src/cpu/translator/ir_builder.cpp
- src/cpu/jit/jit.h
- src/cpu/jit/jit.cpp
- tests/test_ir_end_to_end.cpp
- tests/test_jit_integration.cpp
- tests/CMakeLists.txt

## Pasos

1. Backup: cp -r pcbased-switch pcbased-switch-backup
2. Copiar: descomprime este ZIP sobre tu repo (sobrescribe)
3. Compilar: cd build-host && cmake .. -DPAS_BUILD_TESTS=ON && make -j$(nproc)
4. Testear: ./tests/pas_tests
5. Reportar: dime output completo

## Nota sobre ir_builder.cpp
No pude leer tu original (error 500 de GitHub). Reconstruí completo.
Si tenías código adicional, transfiérelo antes de compilar.
