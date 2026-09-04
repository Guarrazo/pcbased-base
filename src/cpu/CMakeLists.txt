add_library(pas_cpu STATIC
    x86/decoder.cpp
    translator/ir.cpp
    translator/ir_builder.cpp
    arm64/emitter.cpp
    jit/jit.cpp
    jit/code_cache.cpp
    jit/arm64_codegen.cpp
)
target_include_directories(pas_cpu PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pas_cpu PUBLIC pas_core)
