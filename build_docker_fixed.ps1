# Docker Build Script for Windows
# This script builds and runs the complete DAF system in Docker

Write-Host "=== DAF Production System - Docker Build ===" -ForegroundColor Cyan

# Function to check if Docker is running
function Test-DockerRunning {
    try {
        docker info | Out-Null
        return $true
    } catch {
        return $false
    }
}

# Check Docker availability
Write-Host "Checking Docker availability..." -ForegroundColor Yellow
if (-not (Test-DockerRunning)) {
    Write-Host "ERROR: Docker is not running or not installed!" -ForegroundColor Red
    Write-Host "Please start Docker Desktop and try again." -ForegroundColor Red
    exit 1
}

Write-Host "Docker is running" -ForegroundColor Green

# Clean up any existing containers
Write-Host "Cleaning up existing containers..." -ForegroundColor Yellow
docker-compose -f docker-compose.simple.yml down --remove-orphans --volumes 2>$null
docker system prune -f 2>$null

Write-Host "Cleanup completed" -ForegroundColor Green

# Build the system
Write-Host "Building DAF system containers..." -ForegroundColor Yellow
docker-compose -f docker-compose.simple.yml build --no-cache

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Build completed successfully" -ForegroundColor Green

# Start the system
Write-Host "Starting DAF system..." -ForegroundColor Yellow
docker-compose -f docker-compose.simple.yml up -d

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to start system!" -ForegroundColor Red
    exit 1
}

Write-Host "System started successfully" -ForegroundColor Green

# Wait for services to be ready
Write-Host "Waiting for services to be ready..." -ForegroundColor Yellow
Start-Sleep -Seconds 10

# Check service status
Write-Host "Checking service status..." -ForegroundColor Yellow
docker-compose -f docker-compose.simple.yml ps

Write-Host ""
Write-Host "=== DAF System Successfully Deployed ===" -ForegroundColor Cyan
Write-Host "Redis: localhost:6379" -ForegroundColor White
Write-Host "HTTP API: http://localhost:8080" -ForegroundColor White
Write-Host "gRPC: localhost:50051" -ForegroundColor White
Write-Host ""
Write-Host "Available commands:" -ForegroundColor Yellow
Write-Host "  docker-compose -f docker-compose.simple.yml logs" -ForegroundColor White
Write-Host "  docker-compose -f docker-compose.simple.yml down" -ForegroundColor White
Write-Host "  docker-compose -f docker-compose.simple.yml ps" -ForegroundColor White