add_executable(pc-arcade-switch
    main.cpp
    jit_memory.cpp
    input_hid.cpp
    graphics_deko3d.cpp
)

target_include_directories(pc-arcade-switch PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(pc-arcade-switch PRIVATE
    pas_core
    pas_cpu
    pas_os
    pas_graphics
    pas_arcade
    pas_input
    pas_patch
    pas_hooks
    pas_profiles
)

# libnx la provee el toolchain de devkitA64 (variable NX_LIB / target
# "libnx" segun la version del paquete devkitPro/cmake instalado -- ver
# comentario en el CMakeLists.txt raiz). Se enlaza explicitamente aqui
# porque este es el UNICO target de todo el proyecto que debe depender de
# libnx (docs/ARCHITECTURE.md, separacion core/platform).
if(NINTENDO_SWITCH OR DEFINED ENV{DEVKITPRO})
    target_link_libraries(pc-arcade-switch PRIVATE nx)
endif()

# Generacion de .nro -- nombres de funcion tipicos del paquete switch-cmake
# de devkitPro. Verifica el nombre exacto contra tu instalacion local
# ($DEVKITPRO/cmake/Switch.cmake) antes de asumir que esta invocacion es
# correcta sin cambios -- no se ha podido validar contra una instalacion
# real de devkitPro en el entorno donde se genero este esqueleto.
if(COMMAND nx_generate_nacp)
    nx_generate_nacp(${CMAKE_BINARY_DIR}/pc-arcade-switch.nacp
        NAME "PC Arcade Switch"
        AUTHOR "Binomio"
        VERSION "0.0.1-skeleton"
    )
    nx_create_nro(pc-arcade-switch
        NACP ${CMAKE_BINARY_DIR}/pc-arcade-switch.nacp
    )
endif()
