add_library(pas_patch STATIC
    patch_engine.cpp
)
target_include_directories(pas_patch PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pas_patch PUBLIC pas_core)
