#include "../common/daf_types.h"
#include "../storage/redis_client_production.h"
#include "production_coordinator.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>

namespace daf {

// Global coordinator instance for signal handling
std::unique_ptr<ProductionCoordinator> g_coordinator;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[INFO] Shutting down coordinator..." << std::endl;
        if (g_coordinator) {
            g_coordinator->Stop();
        }
        exit(0);
    }
}

} // namespace daf

int main() {
    std::cout << "[INFO] Starting DAF Production Coordinator" << std::endl;
    std::cout << "[INFO] *** ALL SIMULATION COMPONENTS REPLACED ***" << std::endl;
    
    // Set up signal handling
    std::signal(SIGINT, daf::signal_handler);
    std::signal(SIGTERM, daf::signal_handler);
    
    // Get Redis connection details from environment
    const char* redis_host = std::getenv("REDIS_HOST");
    const char* redis_port_str = std::getenv("REDIS_PORT");
    
    std::string host = redis_host ? redis_host : "localhost";
    int port = redis_port_str ? std::stoi(redis_port_str) : 6379;
    
    std::cout << "[INFO] Redis Backend: " << host << ":" << port << std::endl;
    std::cout << "[INFO] Starting production coordinator with real components..." << std::endl;
    
    // Start production coordinator
    try {
        daf::g_coordinator = std::make_unique<daf::ProductionCoordinator>(host, port);
        
        if (!daf::g_coordinator->Start()) {
            std::cerr << "[ERROR] Failed to start production coordinator" << std::endl;
            return 1;
        }
        
        std::cout << "[SUCCESS] Production coordinator started!" << std::endl;
        std::cout << "[INFO] Services available:" << std::endl;
        std::cout << "[INFO]   • HTTP API: http://localhost:8080" << std::endl;
        std::cout << "[INFO]   • gRPC API: localhost:50051" << std::endl; 
        std::cout << "[INFO]   • Redis Backend: " << host << ":" << port << std::endl;
        std::cout << "[INFO]   • Real hiredis connectivity" << std::endl;
        std::cout << "[INFO]   • Microsoft cpprest HTTP server" << std::endl;
        std::cout << "[INFO]   • Production job management" << std::endl;
        std::cout << "\n[INFO] NeRF avatar processing system ready!" << std::endl;
        
        // Keep running until shutdown signal
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Coordinator failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}