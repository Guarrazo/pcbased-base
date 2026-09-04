add_library(pas_os STATIC
    elf_loader/elf_loader.cpp
    syscall/libc_shim.cpp
    filesystem/virtual_fs.cpp
)
target_include_directories(pas_os PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pas_os PUBLIC pas_core)
