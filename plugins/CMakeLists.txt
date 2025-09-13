cmake_minimum_required(VERSION 3.16)

# NeRF Avatar Plugin
add_library(nerf_avatar_plugin SHARED
    nerf_avatar/nerf_avatar_plugin.cpp
)

target_include_directories(nerf_avatar_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/../framework/src/common
)

target_link_libraries(nerf_avatar_plugin
    ${CMAKE_DL_LIBS}
)

# Set plugin properties for dynamic loading
set_target_properties(nerf_avatar_plugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN 1
    PREFIX ""  # No lib prefix
    SUFFIX ".so"
)

# Memory optimization for 512MB containers
target_compile_definitions(nerf_avatar_plugin PRIVATE
    NERF_MAX_RESOLUTION=512
    NERF_MAX_SAMPLES=64
    NERF_ENABLE_MEMORY_OPT=1
)

# Install plugin
install(TARGETS nerf_avatar_plugin
    LIBRARY DESTINATION plugins
)
