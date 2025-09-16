cmake_minimum_required(VERSION 3.16)
project(DAF_Plugins VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard to match framework
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NeRF Avatar Plugin for MapReduce Framework
add_library(nerf_avatar_plugin SHARED
    nerf_avatar/nerf_avatar_plugin.cpp
)

# Include framework headers
target_include_directories(nerf_avatar_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/../framework/src/common
)

# Link to framework common library
target_link_directories(nerf_avatar_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/../framework/build
)

# Platform-specific settings
if(WIN32)
    # Windows DLL
    set_target_properties(nerf_avatar_plugin PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    # Link to the static common library (no -l prefix needed for static libs)
    target_link_libraries(nerf_avatar_plugin 
        ${CMAKE_SOURCE_DIR}/../framework/build/libdaf_common.a
        ws2_32 psapi
    )
    
    # MinGW static linking for portability
    if(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(nerf_avatar_plugin -static-libgcc -static-libstdc++)
    endif()
else()
    # Linux/Unix shared library
    target_link_libraries(nerf_avatar_plugin 
        ${CMAKE_SOURCE_DIR}/../framework/build/libdaf_common.a
        ${CMAKE_DL_LIBS} pthread
    )
    set_target_properties(nerf_avatar_plugin PROPERTIES
        PREFIX ""
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Set plugin properties for dynamic loading
set_target_properties(nerf_avatar_plugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN 1
)

# Memory optimization for 512MB containers
target_compile_definitions(nerf_avatar_plugin PRIVATE
    NERF_MAX_RESOLUTION=512
    NERF_MAX_SAMPLES=64
    NERF_ENABLE_MEMORY_OPT=1
    DAF_MAX_MEMORY_MB=400
)

# Install plugin
install(TARGETS nerf_avatar_plugin
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins  # For Windows DLLs
)
