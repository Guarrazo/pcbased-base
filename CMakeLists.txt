cmake_minimum_required(VERSION 3.13)

# Requiere invocarse con:
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
# El toolchain file lo provee el paquete "switch-cmake" de devkitPro.
# NOTA: los nombres exactos de las funciones nx_* / dkp_* pueden variar entre
# versiones del paquete devkitPro/cmake instalado. Verifica contra tu instalación
# local (`cmake --help-module` no aplica; revisa $DEVKITPRO/cmake/Switch*.cmake)
# antes de asumir que esta lista es exhaustiva o exacta para tu setup.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS
        "No se ha definido CMAKE_TOOLCHAIN_FILE. Compilando para HOST (tests only). "
        "Para compilar para Switch: cmake -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake")
    set(PAS_HOST_BUILD ON)
else()
    set(PAS_HOST_BUILD OFF)
endif()

project(pc-arcade-switch C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)

option(PAS_BUILD_TESTS "Compilar tests de host (no requieren hardware Switch)" OFF)

# --- Núcleo independiente de plataforma -------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/cpu)
add_subdirectory(src/os)
add_subdirectory(src/patch)
add_subdirectory(src/profiles)

# --- Componentes que requieren libnx (solo para Switch) ---------------------
if(NOT PAS_HOST_BUILD)
    add_subdirectory(src/graphics)
    add_subdirectory(src/arcade)
    add_subdirectory(src/input)
    add_subdirectory(src/hooks)
    add_subdirectory(src/platform/switch)
endif()

if(PAS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
