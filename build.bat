@echo off
REM Windows build script for C++ MapReduce Framework (No Fortran)
REM Optimized for MinGW-w64 GCC 15.2.0

echo ====================================================================
echo   Building C++ MapReduce Framework for NeRF Distributed System
echo   Using MinGW-w64 GCC 15.2.0
echo ====================================================================

REM Check prerequisites
echo [INFO] Checking prerequisites...

REM Check for CMake
cmake --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found. Please install CMake and add to PATH.
    exit /b 1
)

REM Check for MinGW GCC (preferred for this project)
gcc --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] GCC not found in PATH. Please ensure MinGW-w64 GCC is installed and in PATH.
    echo [INFO] Expected: GCC 15.2.0 or compatible MinGW-w64 version
    exit /b 1
) else (
    echo [INFO] Found GCC compiler:
    gcc --version | findstr gcc
)

REM Check for Make
mingw32-make --version >nul 2>&1
if errorlevel 1 (
    make --version >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Make not found. Please ensure MinGW make is installed.
        exit /b 1
    ) else (
        set MAKE_CMD=make
    )
) else (
    set MAKE_CMD=mingw32-make
)
echo [INFO] Using make command: %MAKE_CMD%

REM Check for Docker (optional)
docker --version >nul 2>&1
if errorlevel 1 (
    echo [WARNING] Docker not found. Docker builds will be skipped.
    set SKIP_DOCKER=1
) else (
    echo [INFO] Docker found: Docker builds will be included.
    set SKIP_DOCKER=0
)

echo [INFO] Prerequisites check completed.
echo [INFO] Building C++ MapReduce framework with MinGW...

REM Create build directories
if not exist build mkdir build
if not exist build\bin mkdir build\bin
if not exist build\obj mkdir build\obj
if not exist build\logs mkdir build\logs

REM Build core framework (using CMake with MinGW)
echo [INFO] Building core MapReduce framework with MinGW...
cd framework
if exist build rmdir /s /q build
mkdir build
cd build

REM Configure with CMake for MinGW Makefiles
echo [INFO] Configuring with CMake (MinGW Makefiles)
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..

if errorlevel 1 (
    echo [ERROR] CMake configuration failed for framework
    goto :error
)

REM Build framework
echo [INFO] Building framework with %MAKE_CMD%...
%MAKE_CMD% -j4

if errorlevel 1 (
    echo [ERROR] Framework build failed
    goto :error
)

echo [SUCCESS] Framework core built successfully
cd ..\..

REM Build plugins
echo [INFO] Building plugins with MinGW...
cd plugins
if exist build rmdir /s /q build
mkdir build
cd build

REM Configure plugins with MinGW
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..

if errorlevel 1 (
    echo [ERROR] CMake configuration failed for plugins
    goto :error
)

REM Build plugins
%MAKE_CMD% -j4

if errorlevel 1 (
    echo [ERROR] Plugin build failed
    goto :error
)

echo [SUCCESS] Plugins built successfully
cd ..\..

REM Generate protobuf files if they exist (skip for now to get basic build working)
REM if exist framework\proto\daf.proto (
REM     echo [INFO] Generating protobuf files...
REM     protoc --version >nul 2>&1
REM     if errorlevel 1 (
REM         echo [WARNING] protoc not found. Skipping protobuf generation.
REM     ) else (
REM         cd framework\proto
REM         protoc --cpp_out=../src/common daf.proto
REM         if errorlevel 1 (
REM             echo [WARNING] Failed to generate protobuf files
REM         ) else (
REM             echo [SUCCESS] Protobuf files generated
REM         )
REM         cd ..\..
REM     )
REM )

REM Build Docker images (requires Docker Desktop for Windows)
if %SKIP_DOCKER%==0 (
    echo [INFO] Building Docker images for distributed deployment...
    
    REM Check if docker-compose.yml exists
    if exist framework\docker\docker-compose.yml (
        cd framework\docker
        
        REM Build coordinator image
        echo [INFO] Building coordinator image...
        docker build -f Dockerfile.coordinator -t daf/coordinator:latest .
        if errorlevel 1 (
            echo [ERROR] Failed to build coordinator Docker image
            cd ..\..
            goto :error
        )
        
        REM Build worker image
        echo [INFO] Building worker image...
        docker build -f Dockerfile.worker -t daf/worker:latest .
        if errorlevel 1 (
            echo [ERROR] Failed to build worker Docker image
            cd ..\..
            goto :error
        )
        
        REM Build complete docker-compose
        echo [INFO] Building complete Docker environment...
        docker-compose build
        if errorlevel 1 (
            echo [WARNING] Docker-compose build had issues, but individual images built successfully
        )
        
        cd ..\..
        echo [SUCCESS] Docker images built successfully
    ) else (
        echo [WARNING] Docker configuration not found at framework\docker\docker-compose.yml
        echo [INFO] Skipping Docker build
    )
) else (
    echo [INFO] Skipping Docker builds (Docker not available)
)

REM Copy binaries to main build directory
echo [INFO] Copying binaries to build directory...
if exist framework\build\daf_coordinator.exe copy framework\build\daf_coordinator.exe build\bin\ >nul 2>&1
if exist framework\build\daf_worker.exe copy framework\build\daf_worker.exe build\bin\ >nul 2>&1
if exist plugins\build\*.dll copy plugins\build\*.dll build\bin\ >nul 2>&1

echo ====================================================================
echo [SUCCESS] Build completed successfully!
echo ====================================================================
echo [INFO] Framework and plugins built for Windows with MinGW GCC 15.2.0
echo [INFO] Binaries available in: build\bin\
if %SKIP_DOCKER%==0 (
    echo [INFO] Docker images ready for deployment:
    echo        - daf/coordinator:latest
    echo        - daf/worker:latest
    echo [INFO] Deploy with: docker-compose -f framework\docker\docker-compose.yml up
    echo [INFO] Each container limited to 512MB RAM as specified
) else (
    echo [INFO] Docker images skipped - install Docker Desktop for container deployment
)
echo [INFO] Ready for distributed MapReduce processing!
goto :end

:error
echo [ERROR] Build failed!
exit /b 1

:end
