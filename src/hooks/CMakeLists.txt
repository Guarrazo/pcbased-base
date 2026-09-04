add_library(pas_hooks STATIC
    hook.cpp
)
target_include_directories(pas_hooks PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pas_hooks PUBLIC pas_core)
