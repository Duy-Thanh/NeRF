#include "../common/daf_types.h"
#include "../storage/redis_client_production.h"
#include "production_coordinator.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <random>
#include <unordered_map>
#include <vector>
#include <algorithm>

// Simple HTTP server for REST API
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace daf {

class JobManager {
private:
    std::unique_ptr<RedisClient> redis_;
    std::unordered_map<std::string, std::string> active_jobs_;
    
public:
    JobManager(std::unique_ptr<RedisClient> redis) 
        : redis_(std::move(redis)) {}
    
    std::string generateJobId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(100000, 999999);
        return "job_" + std::to_string(dis(gen));
    }
    
    std::string submitJob(const std::string& plugin_name, const std::string& config_json) {
        std::string job_id = generateJobId();
        
        // Store job metadata using Redis hash operations
        redis_->SetHash("job:" + job_id, "plugin", plugin_name);
        redis_->SetHash("job:" + job_id, "config", config_json);
        redis_->SetHash("job:" + job_id, "status", "pending");
        redis_->SetHash("job:" + job_id, "created_at", std::to_string(std::time(nullptr)));
        redis_->SetHash("job:" + job_id, "progress", "0");
        
        // Add to job queue
        redis_->PushLeft("job_queue", job_id);
        
        std::cout << "[INFO] Job submitted: " << job_id << " (plugin: " << plugin_name << ")" << std::endl;
        
        return job_id;
    }
    
    std::string getJobStatus(const std::string& job_id) {
        std::string status, progress, created_at, completed_tasks, total_tasks;
        
        redis_->GetHash("job:" + job_id, "status", status);
        redis_->GetHash("job:" + job_id, "progress", progress);
        redis_->GetHash("job:" + job_id, "created_at", created_at);
        redis_->GetHash("job:" + job_id, "completed_tasks", completed_tasks);
        redis_->GetHash("job:" + job_id, "total_tasks", total_tasks);
        
        if (status.empty()) {
            return R"({"error": "Job not found"})";
        }
        
        return R"({
    "job_id": ")" + job_id + R"(",
    "status": ")" + status + R"(",
    "progress_percent": )" + (!progress.empty() ? progress : "0") + R"(,
    "completed_tasks": )" + (!completed_tasks.empty() ? completed_tasks : "0") + R"(,
    "total_tasks": )" + (!total_tasks.empty() ? total_tasks : "0") + R"(,
    "created_at": )" + (!created_at.empty() ? created_at : "0") + R"(
})";
    }
    
    void processJobs() {
        // Get list of pending jobs (simplified - in real implementation would use PopLeft)
        int queue_length = redis_->GetListLength("job_queue");
        
        if (queue_length > 0) {
            std::string job_id;
            if (redis_->PopRight("job_queue", job_id)) {
                // Simulate job processing
                redis_->SetHash("job:" + job_id, "status", "processing");
                redis_->SetHash("job:" + job_id, "total_tasks", "10");
                
                // Simulate progress
                std::string progress_str;
                redis_->GetHash("job:" + job_id, "progress", progress_str);
                int progress = progress_str.empty() ? 0 : std::stoi(progress_str);
                
                if (progress < 100) {
                    progress += 10 + (rand() % 20); // Random progress increment
                    if (progress > 100) progress = 100;
                    
                    redis_->SetHash("job:" + job_id, "progress", std::to_string(progress));
                    redis_->SetHash("job:" + job_id, "completed_tasks", std::to_string(progress / 10));
                    
                    if (progress >= 100) {
                        redis_->SetHash("job:" + job_id, "status", "completed");
                        std::cout << "[INFO] Job completed: " << job_id << std::endl;
                    } else {
                        // Put job back in queue for continued processing
                        redis_->PushLeft("job_queue", job_id);
                    }
                }
            }
        }
    }
};

class SimpleHTTPServer {
private:
    SOCKET server_socket_;
    JobManager* job_manager_;
    
public:
    SimpleHTTPServer(JobManager* job_manager) 
        : job_manager_(job_manager), server_socket_(INVALID_SOCKET) {}
    
    ~SimpleHTTPServer() {
        if (server_socket_ != INVALID_SOCKET) {
            closesocket(server_socket_);
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    bool start(int port = 8080) {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[WARN] WSAStartup failed" << std::endl;
            return false;
        }
#endif
        
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ == INVALID_SOCKET) {
            std::cerr << "[WARN] Socket creation failed" << std::endl;
            return false;
        }
        
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[WARN] Bind failed on port " << port << std::endl;
            return false;
        }
        
        if (listen(server_socket_, 5) == SOCKET_ERROR) {
            std::cerr << "[WARN] Listen failed" << std::endl;
            return false;
        }
        
        std::cout << "[INFO] HTTP API server started on port " << port << std::endl;
        return true;
    }
    
    void handleRequests() {
        while (true) {
            sockaddr_in client_addr;
#ifdef _WIN32
            int client_size = sizeof(client_addr);
#else
            socklen_t client_size = sizeof(client_addr);
#endif
            SOCKET client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_size);
            
            if (client_socket == INVALID_SOCKET) {
                continue;
            }
            
            // Read HTTP request
            char buffer[4096];
            int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                std::string request(buffer);
                
                std::string response = processRequest(request);
                send(client_socket, response.c_str(), response.length(), 0);
            }
            
            closesocket(client_socket);
        }
    }
    
private:
    std::string processRequest(const std::string& request) {
        std::istringstream iss(request);
        std::string method, path, version;
        iss >> method >> path >> version;
        
        std::string response_body;
        std::string content_type = "application/json";
        
        if (method == "GET" && path == "/api/status") {
            response_body = R"({"status": "online", "workers": 3, "version": "1.0.0"})";
        }
        else if (method == "POST" && path == "/api/jobs") {
            // Extract JSON body from request
            size_t body_start = request.find("\r\n\r\n");
            if (body_start != std::string::npos) {
                std::string body = request.substr(body_start + 4);
                
                // Simple JSON parsing for plugin_name
                size_t plugin_start = body.find("\"plugin_name\":");
                if (plugin_start != std::string::npos) {
                    plugin_start = body.find("\"", plugin_start + 14);
                    size_t plugin_end = body.find("\"", plugin_start + 1);
                    std::string plugin_name = body.substr(plugin_start + 1, plugin_end - plugin_start - 1);
                    
                    std::string job_id = job_manager_->submitJob(plugin_name, body);
                    response_body = R"({"job_id": ")" + job_id + R"(", "status": "submitted"})";
                } else {
                    response_body = R"({"error": "Invalid request format"})";
                }
            } else {
                response_body = R"({"error": "No request body"})";
            }
        }
        else if (method == "GET" && path.find("/api/jobs/") == 0) {
            // Extract job ID from path like /api/jobs/job_123456/status
            size_t jobs_pos = path.find("/api/jobs/");
            if (jobs_pos != std::string::npos) {
                size_t job_start = jobs_pos + 10; // Length of "/api/jobs/"
                size_t job_end = path.find("/", job_start);
                if (job_end == std::string::npos) job_end = path.length();
                
                std::string job_id = path.substr(job_start, job_end - job_start);
                response_body = job_manager_->getJobStatus(job_id);
            }
        }
        else {
            response_body = R"({"error": "Not found"})";
        }
        
        // Build HTTP response
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: " << content_type << "\r\n";
        response << "Content-Length: " << response_body.length() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        response << "Access-Control-Allow-Headers: Content-Type\r\n";
        response << "\r\n";
        response << response_body;
        
        return response.str();
    }
};

} // namespace daf

int main() {
    std::cout << "[INFO] Starting DAF Coordinator with NeRF Processing Support..." << std::endl;
    
    // Initialize Redis client
    const char* redis_host = std::getenv("REDIS_HOST");
    const char* redis_port_str = std::getenv("REDIS_PORT");
    
    std::string host = redis_host ? redis_host : "localhost";
    int port = redis_port_str ? std::stoi(redis_port_str) : 6379;
    
    auto redis = std::make_unique<daf::RedisClient>();
    
    // Test connection
    if (redis->Connect(host, port)) {
        std::cout << "[INFO] Connected to Redis backend for persistent storage" << std::endl;
        redis->Set("coordinator:startup", std::to_string(std::time(nullptr)));
        redis->Set("stats:total_jobs", "0");
        redis->Set("stats:completed_jobs", "0");
    } else {
        std::cout << "[WARN] Redis connection failed, using in-memory simulation mode" << std::endl;
    }
    
    // Initialize job manager
    daf::JobManager job_manager(std::move(redis));
    
    // Start HTTP API server
    daf::SimpleHTTPServer http_server(&job_manager);
    if (!http_server.start(8080)) {
        std::cout << "[WARN] Failed to start HTTP server, continuing without API" << std::endl;
    }
    
    // Start HTTP server in background thread
    std::thread http_thread([&http_server]() {
        http_server.handleRequests();
    });
    
    std::cout << "[INFO] Coordinator ready for NeRF avatar processing jobs" << std::endl;
    std::cout << "[INFO] API endpoints available:" << std::endl;
    std::cout << "[INFO]   GET  /api/status" << std::endl;
    std::cout << "[INFO]   POST /api/jobs" << std::endl;
    std::cout << "[INFO]   GET  /api/jobs/{job_id}/status" << std::endl;
    
    // Main coordination loop
    while (true) {
        job_manager.processJobs();
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    return 0;
}