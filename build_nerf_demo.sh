#!/bin/bash

echo "🚀 Building NeRF-Enhanced Distributed Framework..."

# Set up build environment
export CXX=g++
export CXXFLAGS="-std=c++17 -O2 -static"

# Create build directory
mkdir -p framework/build
cd framework/build

# Build the NeRF-enhanced coordinator
echo "📦 Building NeRF coordinator..."
$CXX $CXXFLAGS -I../src \
    ../src/coordinator/nerf_main.cpp \
    ../src/storage/redis_client.cpp \
    ../src/common/daf_utils.cpp \
    -o coordinator_nerf -lws2_32

# Copy to docker directory for containerization
cp coordinator_nerf ../docker/coordinator_nerf.exe

# Build regular worker (unchanged)
echo "📦 Building worker..."
$CXX $CXXFLAGS -I../src \
    ../src/worker/main.cpp \
    ../src/storage/redis_client.cpp \
    ../src/common/daf_utils.cpp \
    -o worker

cp worker ../docker/worker.exe

echo "✅ Build complete!"
echo "📁 Binaries available in framework/build/"
echo "🐳 Docker-ready binaries in framework/docker/"

# Show binary sizes
ls -lh coordinator_nerf* worker*