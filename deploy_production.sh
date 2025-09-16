#!/bin/bash

# Production Deployment Script for DAF Distributed System
# This script replaces all simulation/simplified components with production implementations

set -e

echo "🚀 DAF Production Deployment"
echo "=========================="
echo "Deploying real Redis + cpprest HTTP + gRPC system"
echo "Removing ALL simulation and simplified components"
echo "=========================="

# Configuration
REDIS_HOST=${REDIS_HOST:-redis}
REDIS_PORT=${REDIS_PORT:-6379}
HTTP_PORT=${HTTP_PORT:-8080}
GRPC_PORT=${GRPC_PORT:-50051}
DOCKER_COMPOSE_FILE="framework/docker/docker-compose.production.yml"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed"
        exit 1
    fi
    
    if ! command -v docker-compose &> /dev/null; then
        log_error "Docker Compose is not installed"
        exit 1
    fi
    
    log_info "Prerequisites check passed"
}

# Build production Docker images
build_production_images() {
    log_info "Building production Docker images..."
    
    # Build coordinator image
    log_info "Building production coordinator..."
    docker build -f Dockerfile.production -t daf-coordinator:production .
    
    # Build worker image  
    log_info "Building production worker..."
    docker build -f Dockerfile.production.worker -t daf-worker:production .
    
    log_info "Production images built successfully"
}

# Deploy Redis cluster
deploy_redis() {
    log_info "Deploying Redis production cluster..."
    
    # Start Redis with persistence and clustering
    docker-compose -f ${DOCKER_COMPOSE_FILE} up -d redis
    
    # Wait for Redis to be ready
    log_info "Waiting for Redis to be ready..."
    timeout 60 bash -c "until docker-compose -f ${DOCKER_COMPOSE_FILE} exec redis redis-cli ping; do sleep 1; done"
    
    log_info "Redis cluster deployed successfully"
}

# Deploy coordinator
deploy_coordinator() {
    log_info "Deploying production coordinator..."
    
    # Start coordinator with real HTTP and gRPC
    docker-compose -f ${DOCKER_COMPOSE_FILE} up -d coordinator
    
    # Wait for coordinator to be ready
    log_info "Waiting for coordinator to be ready..."
    timeout 120 bash -c "until curl -f http://localhost:${HTTP_PORT}/api/status; do sleep 2; done"
    
    log_info "Production coordinator deployed successfully"
}

# Deploy workers
deploy_workers() {
    log_info "Deploying production workers..."
    
    # Start all workers
    docker-compose -f ${DOCKER_COMPOSE_FILE} up -d worker1 worker2 worker3
    
    # Wait for workers to register
    log_info "Waiting for workers to register..."
    sleep 10
    
    # Check worker registration
    WORKER_COUNT=$(curl -s http://localhost:${HTTP_PORT}/api/workers | jq '.data.count' 2>/dev/null || echo "0")
    log_info "Registered workers: ${WORKER_COUNT}"
    
    log_info "Production workers deployed successfully"
}

# Deploy monitoring
deploy_monitoring() {
    log_info "Deploying monitoring stack..."
    
    # Start Redis Commander for Redis monitoring
    docker-compose -f ${DOCKER_COMPOSE_FILE} up -d redis-commander
    
    # Start Nginx reverse proxy
    docker-compose -f ${DOCKER_COMPOSE_FILE} up -d nginx
    
    log_info "Monitoring stack deployed successfully"
}

# Validate deployment
validate_deployment() {
    log_info "Validating production deployment..."
    
    # Check all services are running
    local failed=0
    
    # Check Redis
    if ! docker-compose -f ${DOCKER_COMPOSE_FILE} exec redis redis-cli ping &>/dev/null; then
        log_error "Redis is not responding"
        failed=1
    fi
    
    # Check coordinator API
    if ! curl -f http://localhost:${HTTP_PORT}/api/status &>/dev/null; then
        log_error "Coordinator HTTP API is not responding"
        failed=1
    fi
    
    # Check worker count
    WORKER_COUNT=$(curl -s http://localhost:${HTTP_PORT}/api/workers | jq '.data.count' 2>/dev/null || echo "0")
    if [ "${WORKER_COUNT}" -lt 3 ]; then
        log_error "Expected 3 workers, found ${WORKER_COUNT}"
        failed=1
    fi
    
    if [ ${failed} -eq 0 ]; then
        log_info "✅ Production deployment validation PASSED"
        return 0
    else
        log_error "❌ Production deployment validation FAILED"
        return 1
    fi
}

# Show deployment status
show_status() {
    log_info "Production Deployment Status"
    echo "============================"
    
    # Service status
    docker-compose -f ${DOCKER_COMPOSE_FILE} ps
    
    echo ""
    log_info "Service URLs:"
    echo "📊 Coordinator API: http://localhost:${HTTP_PORT}"
    echo "🔍 Redis Commander: http://localhost:8081"
    echo "🌐 Nginx Proxy: http://localhost:80"
    
    echo ""
    log_info "Health Checks:"
    
    # API status
    API_STATUS=$(curl -s http://localhost:${HTTP_PORT}/api/status | jq -r '.data.status' 2>/dev/null || echo "unknown")
    echo "📡 Coordinator: ${API_STATUS}"
    
    # Worker count
    WORKER_COUNT=$(curl -s http://localhost:${HTTP_PORT}/api/workers | jq '.data.count' 2>/dev/null || echo "0")
    echo "👷 Active Workers: ${WORKER_COUNT}"
    
    # Redis status
    REDIS_STATUS=$(docker-compose -f ${DOCKER_COMPOSE_FILE} exec redis redis-cli ping 2>/dev/null || echo "DOWN")
    echo "🗄️  Redis: ${REDIS_STATUS}"
    
    echo ""
    log_info "Resources:"
    docker stats --no-stream --format "table {{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}" \
        $(docker-compose -f ${DOCKER_COMPOSE_FILE} ps -q)
}

# Cleanup function
cleanup() {
    log_warn "Cleaning up previous deployment..."
    docker-compose -f ${DOCKER_COMPOSE_FILE} down -v
    docker system prune -f
}

# Main deployment function
main() {
    local action=${1:-deploy}
    
    case ${action} in
        "deploy")
            log_info "Starting full production deployment..."
            check_prerequisites
            cleanup
            build_production_images
            deploy_redis
            deploy_coordinator
            deploy_workers
            deploy_monitoring
            
            if validate_deployment; then
                show_status
                log_info "🎉 Production deployment completed successfully!"
            else
                log_error "💥 Production deployment failed validation"
                exit 1
            fi
            ;;
            
        "status")
            show_status
            ;;
            
        "cleanup")
            cleanup
            log_info "Cleanup completed"
            ;;
            
        "validate")
            validate_deployment
            ;;
            
        *)
            echo "Usage: $0 {deploy|status|cleanup|validate}"
            echo ""
            echo "Commands:"
            echo "  deploy   - Full production deployment (default)"
            echo "  status   - Show deployment status"
            echo "  cleanup  - Clean up deployment"
            echo "  validate - Validate deployment health"
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"