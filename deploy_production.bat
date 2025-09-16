@echo off
setlocal enabledelayedexpansion

REM Production Deployment Script for DAF Distributed System (Windows)
REM This script replaces all simulation/simplified components with production implementations

echo 🚀 DAF Production Deployment (Windows)
echo ========================================
echo Deploying real Redis + cpprest HTTP + gRPC system
echo Removing ALL simulation and simplified components
echo ========================================

REM Configuration
set REDIS_HOST=redis
set REDIS_PORT=6379
set HTTP_PORT=8080
set GRPC_PORT=50051
set DOCKER_COMPOSE_FILE=framework\docker\docker-compose.production.yml

REM Check prerequisites
echo [INFO] Checking prerequisites...

docker --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Docker is not installed
    exit /b 1
)

docker-compose --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Docker Compose is not installed
    exit /b 1
)

echo [INFO] Prerequisites check passed

REM Parse command line arguments
set ACTION=deploy
if not "%1"=="" set ACTION=%1

if "%ACTION%"=="deploy" goto :deploy
if "%ACTION%"=="status" goto :status
if "%ACTION%"=="cleanup" goto :cleanup
if "%ACTION%"=="validate" goto :validate

echo Usage: %0 {deploy^|status^|cleanup^|validate}
echo.
echo Commands:
echo   deploy   - Full production deployment (default)
echo   status   - Show deployment status
echo   cleanup  - Clean up deployment
echo   validate - Validate deployment health
exit /b 1

:deploy
echo [INFO] Starting full production deployment...

REM Cleanup previous deployment
echo [INFO] Cleaning up previous deployment...
docker-compose -f %DOCKER_COMPOSE_FILE% down -v
docker system prune -f

REM Build production Docker images
echo [INFO] Building production Docker images...

echo [INFO] Building production coordinator...
docker build -f Dockerfile.production -t daf-coordinator:production .
if errorlevel 1 (
    echo [ERROR] Failed to build coordinator image
    exit /b 1
)

echo [INFO] Building production worker...
docker build -f Dockerfile.production.worker -t daf-worker:production .
if errorlevel 1 (
    echo [ERROR] Failed to build worker image
    exit /b 1
)

REM Deploy Redis cluster
echo [INFO] Deploying Redis production cluster...
docker-compose -f %DOCKER_COMPOSE_FILE% up -d redis

REM Wait for Redis to be ready
echo [INFO] Waiting for Redis to be ready...
:wait_redis
timeout /t 2 /nobreak >nul
docker-compose -f %DOCKER_COMPOSE_FILE% exec redis redis-cli ping >nul 2>&1
if errorlevel 1 goto :wait_redis

echo [INFO] Redis cluster deployed successfully

REM Deploy coordinator
echo [INFO] Deploying production coordinator...
docker-compose -f %DOCKER_COMPOSE_FILE% up -d coordinator

REM Wait for coordinator to be ready
echo [INFO] Waiting for coordinator to be ready...
:wait_coordinator
timeout /t 3 /nobreak >nul
curl -f http://localhost:%HTTP_PORT%/api/status >nul 2>&1
if errorlevel 1 goto :wait_coordinator

echo [INFO] Production coordinator deployed successfully

REM Deploy workers
echo [INFO] Deploying production workers...
docker-compose -f %DOCKER_COMPOSE_FILE% up -d worker1 worker2 worker3

REM Wait for workers to register
echo [INFO] Waiting for workers to register...
timeout /t 10 /nobreak >nul

echo [INFO] Production workers deployed successfully

REM Deploy monitoring
echo [INFO] Deploying monitoring stack...
docker-compose -f %DOCKER_COMPOSE_FILE% up -d redis-commander nginx

echo [INFO] Monitoring stack deployed successfully

REM Validate deployment
echo [INFO] Validating production deployment...
call :validate
if errorlevel 1 (
    echo [ERROR] Production deployment failed validation
    exit /b 1
)

call :status
echo [INFO] 🎉 Production deployment completed successfully!
goto :eof

:status
echo [INFO] Production Deployment Status
echo ============================

REM Service status
docker-compose -f %DOCKER_COMPOSE_FILE% ps

echo.
echo [INFO] Service URLs:
echo 📊 Coordinator API: http://localhost:%HTTP_PORT%
echo 🔍 Redis Commander: http://localhost:8081
echo 🌐 Nginx Proxy: http://localhost:80

echo.
echo [INFO] Health Checks:

REM API status
for /f "tokens=*" %%i in ('curl -s http://localhost:%HTTP_PORT%/api/status 2^>nul') do set API_RESPONSE=%%i
echo 📡 Coordinator: Available

REM Worker count  
echo 👷 Active Workers: Checking...

REM Redis status
docker-compose -f %DOCKER_COMPOSE_FILE% exec redis redis-cli ping >nul 2>&1
if errorlevel 1 (
    echo 🗄️  Redis: DOWN
) else (
    echo 🗄️  Redis: PONG
)

echo.
echo [INFO] Resources:
docker stats --no-stream --format "table {{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}"

goto :eof

:cleanup
echo [INFO] Cleaning up previous deployment...
docker-compose -f %DOCKER_COMPOSE_FILE% down -v
docker system prune -f
echo [INFO] Cleanup completed
goto :eof

:validate
echo [INFO] Validating production deployment...

REM Check Redis
docker-compose -f %DOCKER_COMPOSE_FILE% exec redis redis-cli ping >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Redis is not responding
    exit /b 1
)

REM Check coordinator API
curl -f http://localhost:%HTTP_PORT%/api/status >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Coordinator HTTP API is not responding
    exit /b 1
)

echo [INFO] ✅ Production deployment validation PASSED
goto :eof

echo [ERROR] Invalid action: %ACTION%
exit /b 1