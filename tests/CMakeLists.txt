# Tests de host (docs/ROADMAP.md): corren en la máquina de desarrollo, NO
# requieren hardware Switch ni devkitPro -- cubren solo los módulos
# independientes de plataforma (core/, patch/, cpu/ sin el backend ARM64
# real). Se activan con -DPAS_BUILD_TESTS=ON al invocar cmake SIN el
# toolchain file de Switch (build de host normal).

add_executable(pas_tests
    main.cpp
    test_patch_engine.cpp
    test_elf_loader.cpp
    test_elf_loader_real_binary.cpp
    test_code_cache.cpp
    test_json_lite.cpp
    test_game_profile.cpp
    test_arm64_emitter.cpp
    test_x86_decoder.cpp
    test_arm64_dynamic_exec.cpp
    test_ir_end_to_end.cpp
    test_jit_integration.cpp
)
target_link_libraries(pas_tests PRIVATE pas_core pas_patch pas_cpu pas_os pas_profiles)
target_compile_definitions(pas_tests PRIVATE PAS_REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME pas_tests COMMAND pas_tests)

# --- Binario ELF32 real (opcional) para test_elf_loader_real_binary.cpp ---
# Ver tests/fixtures/real_elf32_sample.c: no es un binario de Lindbergh (no
# disponible, ver docs/ROADMAP.md) sino un sustituto legal generado con un
# compilador real (gcc -m32), para probar el ElfLoader contra un ELF32
# dinamico de verdad en vez de solo el construido a mano en
# test_elf_loader.cpp. Esto requiere soporte multilib de 32 bits en el host
# de desarrollo (paquete `gcc-multilib`/`libc6-dev-i386` en Debian/Ubuntu) --
# si no esta disponible, el test correspondiente se salta con un aviso en
# vez de romper el build (no todo el mundo tiene multilib instalado).
find_program(PAS_HOST_GCC gcc)
set(PAS_REAL_ELF32_SAMPLE "")
set(PAS_X86_HARNESS "")
if(PAS_HOST_GCC)
    execute_process(
        COMMAND ${PAS_HOST_GCC} -m32 -O0 -fno-pie -no-pie
                ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/real_elf32_sample.c
                -o ${CMAKE_CURRENT_BINARY_DIR}/real_elf32_sample
        RESULT_VARIABLE PAS_M32_BUILD_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(PAS_M32_BUILD_RESULT EQUAL 0)
        set(PAS_REAL_ELF32_SAMPLE "${CMAKE_CURRENT_BINARY_DIR}/real_elf32_sample")
        message(STATUS "pas_tests: binario ELF32 real generado para test_elf_loader_real_binary "
                        "(${PAS_REAL_ELF32_SAMPLE})")

        # Reutiliza el mismo gcc -m32 para el arnes de ejecucion x86-32 nativa
        # (ver tests/fixtures/x86_exec_harness.c y test_ir_end_to_end.cpp): el
        # host x86-64 puede correr binarios x86-32 estaticos directamente
        # (soporte ia32 del kernel Linux), sin qemu.
        execute_process(
            COMMAND ${PAS_HOST_GCC} -m32 -static -O0
                    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/x86_exec_harness.c
                    -o ${CMAKE_CURRENT_BINARY_DIR}/x86_exec_harness
            RESULT_VARIABLE PAS_X86_HARNESS_BUILD_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )
        if(PAS_X86_HARNESS_BUILD_RESULT EQUAL 0)
            set(PAS_X86_HARNESS "${CMAKE_CURRENT_BINARY_DIR}/x86_exec_harness")
            message(STATUS "pas_tests: arnes de ejecucion x86-32 nativa generado (${PAS_X86_HARNESS})")
        endif()
    else()
        message(STATUS "pas_tests: sin soporte multilib de 32 bits (gcc -m32) -- "
                        "test_elf_loader_real_binary se saltara en tiempo de ejecucion. "
                        "Instala gcc-multilib/libc6-dev-i386 para activarlo.")
    endif()
endif()
target_compile_definitions(pas_tests PRIVATE PAS_REAL_ELF32_SAMPLE="${PAS_REAL_ELF32_SAMPLE}")
target_compile_definitions(pas_tests PRIVATE PAS_X86_HARNESS="${PAS_X86_HARNESS}")

# --- Arnes de ejecucion ARM64 real (opcional) para test_arm64_dynamic_exec.cpp ---
# Ver tests/fixtures/arm64_exec_harness.c: compila un pequeno programa ARM64
# real con un cross-compilador (aarch64-linux-gnu-gcc) y lo ejecuta bajo
# qemu-aarch64 (qemu-user) -- permite EJECUTAR de verdad el codigo que
# genera cpu::arm64::Emitter, no solo comprobar su codificacion a nivel de
# bits (que ya cubren los tests de test_arm64_emitter.cpp). Es la validacion
# mas fuerte posible: si el emisor genera una secuencia de instrucciones
# semanticamente incorrecta (aunque cada instruccion individual este bien
# codificada), esto lo detectaria por resultado incorrecto en tiempo de
# ejecucion. Requiere el paquete `gcc-aarch64-linux-gnu` y `qemu-user`
# (Debian/Ubuntu) -- si no estan disponibles, el test se salta con un aviso.
find_program(PAS_AARCH64_GCC aarch64-linux-gnu-gcc)
find_program(PAS_QEMU_AARCH64 qemu-aarch64)
set(PAS_ARM64_HARNESS "")
if(PAS_AARCH64_GCC AND PAS_QEMU_AARCH64)
    execute_process(
        COMMAND ${PAS_AARCH64_GCC} -static -O0
                ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/arm64_exec_harness.c
                -o ${CMAKE_CURRENT_BINARY_DIR}/arm64_exec_harness
        RESULT_VARIABLE PAS_ARM64_HARNESS_BUILD_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(PAS_ARM64_HARNESS_BUILD_RESULT EQUAL 0)
        set(PAS_ARM64_HARNESS "${CMAKE_CURRENT_BINARY_DIR}/arm64_exec_harness")
        message(STATUS "pas_tests: arnes de ejecucion ARM64 generado para "
                        "test_arm64_dynamic_exec (${PAS_ARM64_HARNESS}, via ${PAS_QEMU_AARCH64})")
    else()
        message(STATUS "pas_tests: fallo compilando el arnes ARM64 -- "
                        "test_arm64_dynamic_exec se saltara en tiempo de ejecucion.")
    endif()
else()
    message(STATUS "pas_tests: sin gcc-aarch64-linux-gnu y/o qemu-user -- "
                    "test_arm64_dynamic_exec se saltara en tiempo de ejecucion. "
                    "Instala gcc-aarch64-linux-gnu y qemu-user para activarlo.")
endif()
target_compile_definitions(pas_tests PRIVATE
    PAS_ARM64_HARNESS="${PAS_ARM64_HARNESS}"
    PAS_QEMU_AARCH64="${PAS_QEMU_AARCH64}"
)
