add_library(pas_core STATIC
    log.cpp
    config.cpp
    module.cpp
)
target_include_directories(pas_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
