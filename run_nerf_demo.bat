@echo off
echo 🎭 NeRF Avatar Processing Demo
echo ================================

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Python not found. Please install Python 3.7+ 
    exit /b 1
)

REM Install required Python packages
echo 📦 Installing required packages...
pip install requests pillow numpy >nul 2>&1

REM Build the NeRF-enhanced framework
echo 🔨 Building NeRF framework...
call build_nerf_demo.bat

if errorlevel 1 (
    echo ❌ Build failed!
    exit /b 1
)

REM Start the distributed system with Docker Compose
echo 🐳 Starting distributed system...
cd framework\docker
docker-compose up -d

REM Wait for services to start
echo ⏳ Waiting for services to start...
timeout /t 10 /nobreak >nul

REM Check if coordinator is responding
echo 🔍 Checking system status...
curl -s http://localhost:8080/api/status >nul
if errorlevel 1 (
    echo ⚠️ System not ready, starting coordinator manually...
    start /B ..\build\coordinator_nerf.exe
    timeout /t 5 /nobreak >nul
)

REM Go back to project root
cd ..\..

REM Run the NeRF demo
echo 🎬 Starting NeRF avatar processing demo...
python nerf_demo.py --num-faces 5 --resolution 256

echo.
echo 🎉 Demo complete!
echo 📊 Check the results in .\demo_avatars\
echo 🔧 To stop the system: cd framework\docker && docker-compose down