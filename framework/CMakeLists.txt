cmake_minimum_required(VERSION 3.16)
project(DAF_Framework VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization and memory settings for 512MB containers
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -fno-exceptions -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find required packages
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

# Find Redis (hiredis)
pkg_check_modules(HIREDIS REQUIRED hiredis)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src/common)
include_directories(${CMAKE_BINARY_DIR}/generated) # For protobuf generated files

# Generate protobuf and gRPC files
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/proto)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(PROTO_SRCS "${GENERATED_DIR}/daf.pb.cc")
set(PROTO_HDRS "${GENERATED_DIR}/daf.pb.h")
set(GRPC_SRCS "${GENERATED_DIR}/daf.grpc.pb.cc")
set(GRPC_HDRS "${GENERATED_DIR}/daf.grpc.pb.h")

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND protobuf::protoc
    ARGS --grpc_out=${GENERATED_DIR} --cpp_out=${GENERATED_DIR}
         --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}
         -I${PROTO_DIR} ${PROTO_DIR}/daf.proto
    DEPENDS ${PROTO_DIR}/daf.proto
    COMMENT "Generating protobuf and gRPC files"
)

# Common library
add_library(daf_common STATIC
    src/common/daf_utils.cpp
    src/common/plugin_loader.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_link_libraries(daf_common
    protobuf::libprotobuf
    gRPC::grpc++
    OpenSSL::SSL
    OpenSSL::Crypto
    ${HIREDIS_LIBRARIES}
    dl
)

target_include_directories(daf_common PUBLIC
    ${HIREDIS_INCLUDE_DIRS}
)

# Storage library
add_library(daf_storage STATIC
    src/storage/metadata_store.cpp
    src/storage/object_store.cpp
    src/storage/partition_manager.cpp
)

target_link_libraries(daf_storage daf_common)

# Coordinator executable
add_executable(daf_coordinator
    src/coordinator/main.cpp
    src/coordinator/coordinator_service.cpp
    src/coordinator/job_scheduler.cpp
    src/coordinator/task_manager.cpp
    src/coordinator/worker_manager.cpp
)

target_link_libraries(daf_coordinator
    daf_common
    daf_storage
    pthread
)

# Worker executable
add_executable(daf_worker
    src/worker/main.cpp
    src/worker/worker_service.cpp
    src/worker/task_executor.cpp
    src/worker/map_context_impl.cpp
    src/worker/reduce_context_impl.cpp
)

target_link_libraries(daf_worker
    daf_common
    daf_storage
    pthread
)

# Installation
install(TARGETS daf_coordinator daf_worker
    RUNTIME DESTINATION bin
)

install(FILES src/common/daf_types.h src/common/plugin_loader.h
    DESTINATION include/daf
)

install(FILES ${PROTO_HDRS} ${GRPC_HDRS}
    DESTINATION include/daf/generated
)

# Enable testing
enable_testing()

# Add subdirectories for plugins
add_subdirectory(${CMAKE_SOURCE_DIR}/../plugins plugins)

# Memory optimization flags for 512MB constraint
target_compile_definitions(daf_common PRIVATE
    DAF_MAX_MEMORY_MB=400  # Leave 112MB for system overhead
    DAF_ENABLE_MEMORY_MONITORING=1
)

# Link-time optimization for smaller binaries
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET daf_coordinator daf_worker PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
