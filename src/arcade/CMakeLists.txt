add_library(pas_arcade STATIC
    jvs/virtual_jvs.cpp
    devices/eeprom.cpp
)
target_include_directories(pas_arcade PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pas_arcade PUBLIC pas_core)
