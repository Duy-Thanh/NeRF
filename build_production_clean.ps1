# Production Docker Build Script - Complete DAF System
# Builds and deploys the complete production system with all components

Write-Host "=== DAF PRODUCTION SYSTEM - COMPLETE BUILD ===" -ForegroundColor Cyan

# Check Docker availability
Write-Host "Checking Docker availability..." -ForegroundColor Yellow
$dockerRunning = $true
try {
    docker info | Out-Null
} catch {
    Write-Host "ERROR: Docker is not running!" -ForegroundColor Red
    exit 1
}
Write-Host "Docker is running" -ForegroundColor Green

# Clean up existing containers
Write-Host "Cleaning up existing containers..." -ForegroundColor Yellow
docker-compose -f docker-compose.production.complete.yml down --remove-orphans --volumes 2>$null
docker system prune -f 2>$null
Write-Host "Cleanup completed" -ForegroundColor Green

# Build the complete production system
Write-Host "Building complete DAF production system..." -ForegroundColor Yellow
Write-Host "  - Production Redis client with hiredis" -ForegroundColor White
Write-Host "  - Production HTTP coordinator with cpprest" -ForegroundColor White
Write-Host "  - Production workers with NeRF plugin" -ForegroundColor White
Write-Host "  - Complete plugin system" -ForegroundColor White

docker-compose -f docker-compose.production.complete.yml build --no-cache

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    exit 1
}
Write-Host "Production system built successfully" -ForegroundColor Green

# Start the complete production system
Write-Host "Starting production DAF system..." -ForegroundColor Yellow
docker-compose -f docker-compose.production.complete.yml up -d

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to start system!" -ForegroundColor Red
    exit 1
}
Write-Host "Production system started successfully" -ForegroundColor Green

# Wait for services to be ready
Write-Host "Waiting for services to initialize..." -ForegroundColor Yellow
Start-Sleep -Seconds 15

# Check system status
Write-Host "Checking production system status..." -ForegroundColor Yellow
docker-compose -f docker-compose.production.complete.yml ps

Write-Host ""
Write-Host "=== PRODUCTION DAF SYSTEM DEPLOYED SUCCESSFULLY ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "PRODUCTION SERVICES:" -ForegroundColor Yellow
Write-Host "  Redis Cluster: localhost:6379" -ForegroundColor White
Write-Host "  HTTP API: http://localhost:8080" -ForegroundColor White
Write-Host "  gRPC: localhost:50051" -ForegroundColor White
Write-Host "  Redis Admin: http://localhost:8081" -ForegroundColor White
Write-Host ""
Write-Host "PRODUCTION CAPABILITIES:" -ForegroundColor Yellow
Write-Host "  Real Redis with persistence" -ForegroundColor White
Write-Host "  Production HTTP server (cpprest)" -ForegroundColor White
Write-Host "  Scalable worker nodes" -ForegroundColor White
Write-Host "  NeRF processing plugin" -ForegroundColor White
Write-Host "  Health monitoring" -ForegroundColor White
Write-Host ""
Write-Host "MANAGEMENT COMMANDS:" -ForegroundColor Yellow
Write-Host "  docker-compose -f docker-compose.production.complete.yml logs" -ForegroundColor White
Write-Host "  docker-compose -f docker-compose.production.complete.yml down" -ForegroundColor White
Write-Host "  docker-compose -f docker-compose.production.complete.yml ps" -ForegroundColor White
Write-Host ""
Write-Host "TEST THE SYSTEM:" -ForegroundColor Yellow
Write-Host "  curl http://localhost:8080/api/status" -ForegroundColor White
Write-Host "  curl http://localhost:8080/api/workers" -ForegroundColor White