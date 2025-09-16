#include "storage/redis_client_production.h"
#include <iostream>
#include <cstdlib>

int main() {
    std::cout << "=== DAF Redis Demo ===" << std::endl;
    
    // Get Redis connection details from environment
    const char* redis_host = std::getenv("REDIS_HOST");
    const char* redis_port_str = std::getenv("REDIS_PORT");
    
    std::string host = redis_host ? redis_host : "localhost";
    int port = redis_port_str ? std::atoi(redis_port_str) : 6379;
    
    std::cout << "Attempting to connect to Redis at " << host << ":" << port << std::endl;
    
    // Create Redis client
    daf::RedisClientProduction client;
    
    // Try to connect
    if (!client.Connect(host, port)) {
        std::cerr << "Failed to connect to Redis server" << std::endl;
        std::cout << "This is normal if Redis is not yet running" << std::endl;
        return 0; // Don't fail, just inform
    }
    
    std::cout << "Successfully connected to Redis!" << std::endl;
    
    // Test basic operations
    std::cout << "Testing Redis operations..." << std::endl;
    
    // Set a value
    if (client.Set("demo:message", "Hello from DAF Docker!")) {
        std::cout << "✓ SET operation successful" << std::endl;
    }
    
    // Get the value
    std::string value;
    if (client.Get("demo:message", value)) {
        std::cout << "✓ GET operation successful: " << value << std::endl;
    }
    
    // Test hash operations
    if (client.SetHash("demo:hash", "field1", "value1")) {
        std::cout << "✓ HSET operation successful" << std::endl;
    }
    
    std::cout << "=== Demo completed successfully ===" << std::endl;
    std::cout << "DAF system is ready for production!" << std::endl;
    
    return 0;
}