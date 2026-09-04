add_library(pas_graphics STATIC
    gl_shim/gl_shim.cpp
)
target_include_directories(pas_graphics PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pas_graphics PUBLIC pas_core)
