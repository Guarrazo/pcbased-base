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
    test_phase1_memory_stack.cpp
)
target_link_libraries(pas_tests PRIVATE pas_core pas_patch pas_cpu pas_os pas_profiles)
target_compile_definitions(pas_tests PRIVATE PAS_REPO_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME pas_tests COMMAND pas_tests)

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
        message(STATUS "pas_tests: ELF32 real generado")
        execute_process(
            COMMAND ${PAS_HOST_GCC} -m32 -static -O0
                    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/x86_exec_harness.c
                    -o ${CMAKE_CURRENT_BINARY_DIR}/x86_exec_harness
            RESULT_VARIABLE PAS_X86_HARNESS_BUILD_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )
        if(PAS_X86_HARNESS_BUILD_RESULT EQUAL 0)
            set(PAS_X86_HARNESS "${CMAKE_CURRENT_BINARY_DIR}/x86_exec_harness")
            message(STATUS "pas_tests: arnes x86 generado")
        endif()
    else()
        message(STATUS "pas_tests: sin multilib de 32 bits")
    endif()
endif()
target_compile_definitions(pas_tests PRIVATE PAS_REAL_ELF32_SAMPLE="${PAS_REAL_ELF32_SAMPLE}")
target_compile_definitions(pas_tests PRIVATE PAS_X86_HARNESS="${PAS_X86_HARNESS}")

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
        message(STATUS "pas_tests: arnes ARM64 generado")
    else()
        message(STATUS "pas_tests: fallo arnes ARM64")
    endif()
else()
    message(STATUS "pas_tests: sin gcc-aarch64/qemu")
endif()
target_compile_definitions(pas_tests PRIVATE
    PAS_ARM64_HARNESS="${PAS_ARM64_HARNESS}"
    PAS_QEMU_AARCH64="${PAS_QEMU_AARCH64}"
)
