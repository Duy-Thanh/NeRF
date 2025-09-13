#!/bin/bash

# DAF Framework Build and Deployment Script
set -e

echo "=== DAF Distributed Framework Build Script ==="
echo "Building C++ MapReduce framework with 512MB memory constraints"

# Configuration
BUILD_TYPE=${1:-Release}
DOCKER_REGISTRY=${DOCKER_REGISTRY:-localhost:5000}
VERSION=${VERSION:-1.0.0}

echo "Build type: $BUILD_TYPE"
echo "Docker registry: $DOCKER_REGISTRY"
echo "Version: $VERSION"

# Create build directory
echo "Creating build directory..."
cd framework
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Build the framework
echo "Building DAF framework..."
make -j$(nproc)

echo "✅ Framework build completed successfully!"

# Build plugins
echo "Building plugins..."
cd ../../plugins
mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
make -j$(nproc)

echo "✅ Plugins build completed successfully!"

# Go back to project root
cd ../../

# Build Docker images
echo "Building Docker images..."

# Build coordinator image
echo "Building coordinator image..."
docker build -f framework/docker/Dockerfile.coordinator \
    -t daf/coordinator:$VERSION \
    -t daf/coordinator:latest \
    framework/

# Build worker image  
echo "Building worker image..."
docker build -f framework/docker/Dockerfile.worker \
    -t daf/worker:$VERSION \
    -t daf/worker:latest \
    framework/

echo "✅ Docker images built successfully!"

# Tag for registry (if not localhost)
if [ "$DOCKER_REGISTRY" != "localhost:5000" ]; then
    echo "Tagging images for registry: $DOCKER_REGISTRY"
    docker tag daf/coordinator:$VERSION $DOCKER_REGISTRY/daf/coordinator:$VERSION
    docker tag daf/worker:$VERSION $DOCKER_REGISTRY/daf/worker:$VERSION
fi

echo "=== Build Summary ==="
echo "Framework binaries:"
ls -la framework/build/daf_*

echo ""
echo "Plugin libraries:"
ls -la plugins/build/*.so

echo ""
echo "Docker images:"
docker images | grep daf

echo ""
echo "=== Deployment Commands ==="
echo "To start the distributed system:"
echo "  cd framework/docker"
echo "  docker-compose up -d"
echo ""
echo "To scale workers:"
echo "  docker-compose up -d --scale worker=5"
echo ""
echo "To monitor logs:"
echo "  docker-compose logs -f coordinator"
echo "  docker-compose logs -f worker"
echo ""
echo "To submit a NeRF job:"
echo "  # TODO: Add job submission example"
echo ""
echo "✅ Build completed successfully!"
echo "Memory constraint: Each container limited to 512MB RAM"
echo "Ready for distributed 3D avatar generation!"
