cmake_minimum_required(VERSION 3.16)
project(DAF_Framework VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    if(WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src/common)

add_library(daf_common STATIC
    src/common/daf_utils.cpp
    src/storage/redis_client.cpp
)

if(WIN32)
    target_link_libraries(daf_common ws2_32 psapi)
else()
    target_link_libraries(daf_common dl pthread)
endif()

add_executable(daf_coordinator
    src/coordinator/main.cpp
)

target_link_libraries(daf_coordinator daf_common)

add_executable(daf_worker
    src/worker/main.cpp
)

target_link_libraries(daf_worker daf_common)

# Build plugins separately after framework is built
