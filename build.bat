@echo off
REM Windows build script for NeRF project

echo [INFO] Setting up Intel oneAPI environment...
call "C:\Program Files (x86)\Intel\oneAPI\setvars.bat" intel64 > nul 2>&1

if errorlevel 1 (
    echo [ERROR] Failed to initialize Intel oneAPI environment
    exit /b 1
)

echo [INFO] Building NeRF project...

REM Compile Fortran modules
ifx /c /O3 /QxHost /module:build\obj src\nerf_types.f90 /object:build\obj\nerf_types.obj
if errorlevel 1 goto :error

ifx /c /O3 /QxHost /module:build\obj src\nerf_utils.f90 /object:build\obj\nerf_utils.obj
if errorlevel 1 goto :error

ifx /c /O3 /QxHost /module:build\obj src\nerf_neural_network.f90 /object:build\obj\nerf_neural_network.obj
if errorlevel 1 goto :error

ifx /c /O3 /QxHost /module:build\obj src\nerf_face_processor.f90 /object:build\obj\nerf_face_processor.obj
if errorlevel 1 goto :error

ifx /c /O3 /QxHost /module:build\obj src\nerf_volume_renderer.f90 /object:build\obj\nerf_volume_renderer.obj
if errorlevel 1 goto :error

ifx /c /O3 /QxHost /module:build\obj src\nerf_mapreduce.f90 /object:build\obj\nerf_mapreduce.obj
if errorlevel 1 goto :error

ifx /c /O3 /QxHost /module:build\obj src\nerf_hadoop_interface.f90 /object:build\obj\nerf_hadoop_interface.obj
if errorlevel 1 goto :error

REM Link main executable
ifx /O3 /QxHost /module:build\obj src\main.f90 build\obj\*.obj /exe:build\bin\nerf_bigdata.exe
if errorlevel 1 goto :error

echo [SUCCESS] Build completed successfully!
echo [INFO] Executable created: build\bin\nerf_bigdata.exe
goto :end

:error
echo [ERROR] Build failed!
exit /b 1

:end
