add_library(pas_input STATIC
    input_backend.cpp
)
target_include_directories(pas_input PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pas_input PUBLIC pas_core)
