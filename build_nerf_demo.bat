@echo off
echo 🚀 Building NeRF-Enhanced Distributed Framework...

REM Set up build environment
set CXX=g++
set CXXFLAGS=-std=c++17 -O2 -static

REM Create build directory
if not exist framework\build mkdir framework\build
cd framework\build

REM Build the NeRF-enhanced coordinator
echo 📦 Building NeRF coordinator...
%CXX% %CXXFLAGS% -I..\src ^
    ..\src\coordinator\nerf_main.cpp ^
    ..\src\storage\redis_client.cpp ^
    ..\src\common\daf_utils.cpp ^
    -o coordinator_nerf.exe -lws2_32

REM Copy to docker directory for containerization
copy coordinator_nerf.exe ..\docker\

REM Build regular worker (unchanged)
echo 📦 Building worker...
%CXX% %CXXFLAGS% -I..\src ^
    ..\src\worker\main.cpp ^
    ..\src\storage\redis_client.cpp ^
    ..\src\common\daf_utils.cpp ^
    -o worker.exe

copy worker.exe ..\docker\

echo ✅ Build complete!
echo 📁 Binaries available in framework\build\
echo 🐳 Docker-ready binaries in framework\docker\

REM Show binary sizes
dir coordinator_nerf.exe worker.exe