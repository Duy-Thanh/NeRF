@echo off
REM Windows build script for C++ MapReduce Framework (No Fortran)

echo [INFO] Building C++ MapReduce framework...

REM Create build directories
if not exist build\bin mkdir build\bin
if not exist build\obj mkdir build\obj

REM Build core framework (using CMake)
cd framework
if exist build rmdir /s /q build
mkdir build
cd build
cmake -G "NMake Makefiles" ..
if errorlevel 1 goto :error
nmake
if errorlevel 1 goto :error
cd ..\..

REM Build plugins
cd plugins
if exist build rmdir /s /q build
mkdir build
cd build
cmake -G "NMake Makefiles" ..
if errorlevel 1 goto :error
nmake
if errorlevel 1 goto :error
cd ..\..

REM Build Docker images (requires Docker Desktop for Windows)
echo [INFO] Building Docker images...
docker-compose -f framework\docker\docker-compose.yml build
if errorlevel 1 goto :error

echo [SUCCESS] Build completed successfully!
echo [INFO] Framework and plugins built. Docker images are ready.
goto :end

:error
echo [ERROR] Build failed!
exit /b 1

:end
