#include "production_coordinator.h"
#include <iostream>
#include <signal.h>
#include <memory>

// Global coordinator instance for signal handling
std::unique_ptr<daf::ProductionCoordinator> g_coordinator;

void SignalHandler(int signal) {
    std::cout << "\n[INFO] Received signal " << signal << ", shutting down gracefully..." << std::endl;
    if (g_coordinator) {
        g_coordinator->Stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    std::cout << "[INFO] Starting DAF Production Coordinator" << std::endl;
    std::cout << "[INFO] =================================" << std::endl;
    std::cout << "[INFO] Version: 1.0.0-production" << std::endl;
    std::cout << "[INFO] Built with: Real Redis + cpprest HTTP + gRPC" << std::endl;
    std::cout << "[INFO] Replacing all simulation/simplified components" << std::endl;
    std::cout << "[INFO] =================================" << std::endl;
    
    // Parse command line arguments
    int http_port = 8080;
    int grpc_port = 50051;
    std::string redis_host = "redis"; // Default for Docker
    int redis_port = 6379;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--http-port" && i + 1 < argc) {
            http_port = std::stoi(argv[++i]);
        } else if (arg == "--grpc-port" && i + 1 < argc) {
            grpc_port = std::stoi(argv[++i]);
        } else if (arg == "--redis-host" && i + 1 < argc) {
            redis_host = argv[++i];
        } else if (arg == "--redis-port" && i + 1 < argc) {
            redis_port = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --http-port PORT    HTTP API port (default: 8080)" << std::endl;
            std::cout << "  --grpc-port PORT    gRPC API port (default: 50051)" << std::endl;
            std::cout << "  --redis-host HOST   Redis host (default: redis)" << std::endl;
            std::cout << "  --redis-port PORT   Redis port (default: 6379)" << std::endl;
            std::cout << "  --help, -h          Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Override with environment variables if available
    const char* env_redis_host = std::getenv("REDIS_HOST");
    const char* env_redis_port = std::getenv("REDIS_PORT");
    const char* env_http_port = std::getenv("HTTP_PORT");
    const char* env_grpc_port = std::getenv("GRPC_PORT");
    
    if (env_redis_host) redis_host = env_redis_host;
    if (env_redis_port) redis_port = std::stoi(env_redis_port);
    if (env_http_port) http_port = std::stoi(env_http_port);
    if (env_grpc_port) grpc_port = std::stoi(env_grpc_port);
    
    std::cout << "[INFO] Configuration:" << std::endl;
    std::cout << "[INFO]   HTTP API: 0.0.0.0:" << http_port << std::endl;
    std::cout << "[INFO]   gRPC API: 0.0.0.0:" << grpc_port << std::endl;
    std::cout << "[INFO]   Redis: " << redis_host << ":" << redis_port << std::endl;
    
    // Install signal handlers for graceful shutdown
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    
    try {
        // Create and configure coordinator
        g_coordinator = std::make_unique<daf::ProductionCoordinator>(http_port, grpc_port);
        g_coordinator->SetRedisConnection(redis_host, redis_port);
        g_coordinator->SetWorkerTimeout(300); // 5 minutes
        g_coordinator->SetJobProcessingInterval(2); // 2 seconds
        
        // Initialize coordinator
        if (!g_coordinator->Initialize()) {
            std::cerr << "[ERROR] Failed to initialize coordinator" << std::endl;
            return 1;
        }
        
        // Start coordinator
        if (!g_coordinator->Start()) {
            std::cerr << "[ERROR] Failed to start coordinator" << std::endl;
            return 1;
        }
        
        std::cout << "[INFO] Production Coordinator is running..." << std::endl;
        std::cout << "[INFO] Press Ctrl+C to stop" << std::endl;
        
        // Keep the main thread alive
        while (g_coordinator->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Coordinator error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "[INFO] Production Coordinator stopped" << std::endl;
    return 0;
}