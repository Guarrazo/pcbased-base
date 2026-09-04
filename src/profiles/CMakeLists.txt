add_library(pas_profiles STATIC
    json_lite.cpp
    game_profile.cpp
    platform_profile.cpp
)
target_include_directories(pas_profiles PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pas_profiles PUBLIC pas_core pas_patch)
