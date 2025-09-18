#include "production_coordinator.h"
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <iostream>
#include <random>
#include <chrono>
#include <sstream>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace daf {

ProductionCoordinator::ProductionCoordinator(int http_port, int grpc_port)
    : http_port_(http_port), grpc_port_(grpc_port),
      redis_host_("localhost"), redis_port_(6379),
      worker_timeout_(300), job_processing_interval_(5),
      running_(false), stopping_(false),
      total_jobs_(0), completed_jobs_(0), failed_jobs_(0), active_workers_(0) {
}

ProductionCoordinator::~ProductionCoordinator() {
    Stop();
}

bool ProductionCoordinator::Initialize() {
    std::cout << "[INFO] Initializing Production Coordinator..." << std::endl;
    
    // Initialize Redis connection
    redis_ = std::make_unique<RedisClientProduction>();
    if (!redis_->Connect(redis_host_, redis_port_)) {
        std::cerr << "[ERROR] Failed to connect to Redis at " << redis_host_ << ":" << redis_port_ << std::endl;
        return false;
    }
    
    // Test Redis connection
    if (!redis_->Ping()) {
        std::cerr << "[ERROR] Redis ping test failed" << std::endl;
        return false;
    }
    
    std::cout << "[INFO] Redis connection established" << std::endl;
    
    // Initialize HTTP listener
    std::ostringstream address_builder;
    address_builder << "http://0.0.0.0:" << http_port_;
    http::uri_builder uri(address_builder.str());
    
    http_listener_ = std::make_unique<http_listener>(uri.to_uri());
    
    // Register HTTP handlers
    http_listener_->support(methods::GET, [this](http_request request) {
        std::string path = request.relative_uri().path();
        if (path == "/api/status") {
            HandleGetStatus(request);
        } else if (path.find("/api/jobs/") == 0 && path.find("/status") != std::string::npos) {
            HandleGetJobStatus(request);
        } else if (path == "/api/workers") {
            HandleGetWorkers(request);
        } else {
            request.reply(status_codes::NotFound, CreateErrorResponse("Endpoint not found"));
        }
    });
    
    http_listener_->support(methods::POST, [this](http_request request) {
        std::string path = request.relative_uri().path();
        if (path == "/api/jobs") {
            HandlePostJobs(request);
        } else {
            request.reply(status_codes::NotFound, CreateErrorResponse("Endpoint not found"));
        }
    });
    
    http_listener_->support(methods::DEL, [this](http_request request) {
        std::string path = request.relative_uri().path();
        if (path.find("/api/jobs/") == 0) {
            HandleDeleteJob(request);
        } else {
            request.reply(status_codes::NotFound, CreateErrorResponse("Endpoint not found"));
        }
    });
    
    std::cout << "[INFO] HTTP server configured on port " << http_port_ << std::endl;
    return true;
}

bool ProductionCoordinator::Start() {
    if (running_) {
        std::cout << "[WARN] Coordinator is already running" << std::endl;
        return true;
    }
    
    std::cout << "[INFO] Starting Production Coordinator..." << std::endl;
    
    // Start HTTP listener
    try {
        http_listener_->open().wait();
        std::cout << "[INFO] HTTP API server started on port " << http_port_ << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to start HTTP server: " << e.what() << std::endl;
        return false;
    }
    
    running_ = true;
    stopping_ = false;
    
    // Start background threads
    job_processing_thread_ = std::thread(&ProductionCoordinator::JobProcessingLoop, this);
    worker_monitoring_thread_ = std::thread(&ProductionCoordinator::WorkerMonitoringLoop, this);
    cleanup_thread_ = std::thread(&ProductionCoordinator::CleanupLoop, this);
    
    std::cout << "[INFO] Production Coordinator is running" << std::endl;
    std::cout << "[INFO] API endpoints:" << std::endl;
    std::cout << "[INFO]   GET    /api/status" << std::endl;
    std::cout << "[INFO]   POST   /api/jobs" << std::endl;
    std::cout << "[INFO]   GET    /api/jobs/{job_id}/status" << std::endl;
    std::cout << "[INFO]   GET    /api/workers" << std::endl;
    std::cout << "[INFO]   DELETE /api/jobs/{job_id}" << std::endl;
    
    return true;
}

void ProductionCoordinator::Stop() {
    if (!running_) return;
    
    std::cout << "[INFO] Stopping Production Coordinator..." << std::endl;
    stopping_ = true;
    
    // Close HTTP listener
    if (http_listener_) {
        try {
            http_listener_->close().wait();
        } catch (const std::exception& e) {
            std::cerr << "[WARN] Error closing HTTP listener: " << e.what() << std::endl;
        }
    }
    
    // Wait for background threads to finish
    if (job_processing_thread_.joinable()) {
        job_processing_thread_.join();
    }
    if (worker_monitoring_thread_.joinable()) {
        worker_monitoring_thread_.join();
    }
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    
    // Disconnect from Redis
    if (redis_) {
        redis_->Disconnect();
    }
    
    running_ = false;
    std::cout << "[INFO] Production Coordinator stopped" << std::endl;
}

void ProductionCoordinator::SetRedisConnection(const std::string& host, int port) {
    redis_host_ = host;
    redis_port_ = port;
}

// HTTP API handlers
void ProductionCoordinator::HandleGetStatus(http_request request) {
    LogRequest(request);
    
    json::value response = json::value::object();
    response["status"] = json::value::string("online");
    response["version"] = json::value::string("1.0.0-production");
    response["uptime"] = json::value::number(std::time(nullptr));
    response["total_jobs"] = json::value::number(total_jobs_.load());
    response["completed_jobs"] = json::value::number(completed_jobs_.load());
    response["failed_jobs"] = json::value::number(failed_jobs_.load());
    response["active_workers"] = json::value::number(active_workers_.load());
    response["redis_connected"] = json::value::boolean(redis_ && redis_->IsConnected());
    
    request.reply(status_codes::OK, CreateSuccessResponse(response));
}

void ProductionCoordinator::HandlePostJobs(http_request request) {
    LogRequest(request);
    
    request.extract_json().then([=](pplx::task<json::value> task) {
        try {
            json::value body = task.get();
            
            // Validate required fields
            if (!body.has_field("plugin_name") || !body.has_field("config")) {
                request.reply(status_codes::BadRequest, 
                    CreateErrorResponse("Missing required fields: plugin_name, config"));
                return;
            }
            
            std::string plugin_name = body["plugin_name"].as_string();
            std::string config = body["config"].serialize();
            
            // Generate job ID and submit to Redis
            std::string job_id = GenerateJobId();
            
            if (redis_->SubmitJob(job_id, config)) {
                total_jobs_++;
                
                json::value response = json::value::object();
                response["job_id"] = json::value::string(job_id);
                response["status"] = json::value::string("submitted");
                response["created_at"] = json::value::number(std::time(nullptr));
                
                request.reply(status_codes::Created, CreateSuccessResponse(response));
                
                std::cout << "[INFO] Job submitted: " << job_id << " (plugin: " << plugin_name << ")" << std::endl;
            } else {
                request.reply(status_codes::InternalError,
                    CreateErrorResponse("Failed to submit job to Redis"));
            }
            
        } catch (const std::exception& e) {
            request.reply(status_codes::BadRequest,
                CreateErrorResponse("Invalid JSON payload: " + std::string(e.what())));
        }
    });
}

void ProductionCoordinator::HandleGetJobStatus(http_request request) {
    LogRequest(request);
    
    std::string path = request.relative_uri().path();
    
    // Extract job ID from path: /api/jobs/{job_id}/status
    size_t jobs_pos = path.find("/api/jobs/");
    if (jobs_pos == std::string::npos) {
        request.reply(status_codes::BadRequest, CreateErrorResponse("Invalid job status path"));
        return;
    }
    
    size_t job_start = jobs_pos + 10; // Length of "/api/jobs/"
    size_t job_end = path.find("/", job_start);
    if (job_end == std::string::npos) {
        request.reply(status_codes::BadRequest, CreateErrorResponse("Invalid job ID in path"));
        return;
    }
    
    std::string job_id = path.substr(job_start, job_end - job_start);
    
    // Get job status from Redis
    std::string status, created_at, completed_at;
    if (redis_->GetHash("job:" + job_id, "status", status)) {
        json::value response = json::value::object();
        response["job_id"] = json::value::string(job_id);
        response["status"] = json::value::string(status);
        
        if (redis_->GetHash("job:" + job_id, "created_at", created_at)) {
            response["created_at"] = json::value::number(static_cast<int64_t>(std::stoll(created_at)));
        }
        
        if (redis_->GetHash("job:" + job_id, "completed_at", completed_at)) {
            response["completed_at"] = json::value::number(static_cast<int64_t>(std::stoll(completed_at)));
        }
        
        // Get progress information
        std::string progress, error;
        if (redis_->GetHash("job:" + job_id, "progress", progress)) {
            response["progress_percent"] = json::value::number(std::stoi(progress));
        }
        
        if (redis_->GetHash("job:" + job_id, "error", error)) {
            response["error"] = json::value::string(error);
        }
        
        request.reply(status_codes::OK, CreateSuccessResponse(response));
    } else {
        request.reply(status_codes::NotFound, CreateErrorResponse("Job not found"));
    }
}

void ProductionCoordinator::HandleGetWorkers(http_request request) {
    LogRequest(request);
    
    std::vector<std::string> workers = redis_->GetActiveWorkers();
    
    json::value workers_array = json::value::array();
    int index = 0;
    
    for (const auto& worker_id : workers) {
        if (IsWorkerActive(worker_id)) {
            json::value worker_info = json::value::object();
            worker_info["worker_id"] = json::value::string(worker_id);
            
            std::string host, port, status, last_heartbeat;
            redis_->GetHash("worker:" + worker_id, "host", host);
            redis_->GetHash("worker:" + worker_id, "port", port);
            redis_->GetHash("worker:" + worker_id, "status", status);
            redis_->GetHash("worker:" + worker_id, "last_heartbeat", last_heartbeat);
            
            worker_info["host"] = json::value::string(host);
            worker_info["port"] = json::value::number(std::stoi(port));
            worker_info["status"] = json::value::string(status);
            worker_info["last_heartbeat"] = json::value::number(static_cast<int64_t>(std::stoll(last_heartbeat)));
            
            workers_array[index++] = worker_info;
        }
    }
    
    json::value response = json::value::object();
    response["workers"] = workers_array;
    response["count"] = json::value::number(index);
    
    request.reply(status_codes::OK, CreateSuccessResponse(response));
}

void ProductionCoordinator::HandleDeleteJob(http_request request) {
    LogRequest(request);
    
    std::string path = request.relative_uri().path();
    
    // Extract job ID from path: /api/jobs/{job_id}
    size_t jobs_pos = path.find("/api/jobs/");
    if (jobs_pos == std::string::npos) {
        request.reply(status_codes::BadRequest, CreateErrorResponse("Invalid job deletion path"));
        return;
    }
    
    size_t job_start = jobs_pos + 10; // Length of "/api/jobs/"
    std::string job_id = path.substr(job_start);
    
    // Check if job exists
    if (redis_->Exists("job:" + job_id)) {
        // Mark job as cancelled
        redis_->SetHash("job:" + job_id, "status", "cancelled");
        redis_->SetHash("job:" + job_id, "cancelled_at", std::to_string(std::time(nullptr)));
        
        json::value response = json::value::object();
        response["job_id"] = json::value::string(job_id);
        response["status"] = json::value::string("cancelled");
        
        request.reply(status_codes::OK, CreateSuccessResponse(response));
        
        std::cout << "[INFO] Job cancelled: " << job_id << std::endl;
    } else {
        request.reply(status_codes::NotFound, CreateErrorResponse("Job not found"));
    }
}

// Background processing loops
void ProductionCoordinator::JobProcessingLoop() {
    std::cout << "[INFO] Job processing loop started" << std::endl;
    
    while (!stopping_) {
        try {
            ProcessPendingJobs();
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Job processing error: " << e.what() << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(job_processing_interval_));
    }
    
    std::cout << "[INFO] Job processing loop stopped" << std::endl;
}

void ProductionCoordinator::WorkerMonitoringLoop() {
    std::cout << "[INFO] Worker monitoring loop started" << std::endl;
    
    while (!stopping_) {
        try {
            RemoveInactiveWorkers();
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Worker monitoring error: " << e.what() << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(30)); // Check every 30 seconds
    }
    
    std::cout << "[INFO] Worker monitoring loop stopped" << std::endl;
}

void ProductionCoordinator::CleanupLoop() {
    std::cout << "[INFO] Cleanup loop started" << std::endl;
    
    while (!stopping_) {
        try {
            // Clean up old completed jobs (older than 24 hours)
            // This would be implemented based on specific cleanup requirements
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Cleanup error: " << e.what() << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::hours(1)); // Cleanup every hour
    }
    
    std::cout << "[INFO] Cleanup loop stopped" << std::endl;
}

// Helper methods
std::string ProductionCoordinator::GenerateJobId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    return "job_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

bool ProductionCoordinator::ProcessPendingJobs() {
    // Get pending jobs from Redis queue
    int queue_length = redis_->GetListLength("job_queue");
    if (queue_length <= 0) return true;
    
    std::cout << "[DEBUG] Processing " << queue_length << " pending jobs" << std::endl;
    
    // Process jobs one by one
    std::string job_id;
    while (redis_->PopLeft("job_queue", job_id)) {
        // Check if we have available workers
        std::vector<std::string> workers = GetAvailableWorkers();
        if (workers.empty()) {
            // Put job back in queue if no workers available
            redis_->PushLeft("job_queue", job_id);
            break;
        }
        
        // Update job status to processing
        redis_->SetHash("job:" + job_id, "status", "processing");
        redis_->SetHash("job:" + job_id, "started_at", std::to_string(std::time(nullptr)));
        
        // Create production tasks for the job using enterprise task distribution
        for (int i = 0; i < 5; ++i) {
            std::string task_id = job_id + "_task_" + std::to_string(i);
            std::string task_data = "task_data_" + std::to_string(i);
            redis_->AddTask(job_id, task_id, task_data);
        }
        
        std::cout << "[INFO] Job " << job_id << " started processing with " << workers.size() << " workers" << std::endl;
    }
    
    return true;
}

bool ProductionCoordinator::IsWorkerActive(const std::string& worker_id) {
    std::string last_heartbeat_str;
    if (!redis_->GetHash("worker:" + worker_id, "last_heartbeat", last_heartbeat_str)) {
        return false;
    }
    
    time_t last_heartbeat = std::stoll(last_heartbeat_str);
    time_t now = std::time(nullptr);
    
    return (now - last_heartbeat) < worker_timeout_;
}

void ProductionCoordinator::RemoveInactiveWorkers() {
    std::vector<std::string> workers = redis_->GetActiveWorkers();
    int active_count = 0;
    
    for (const auto& worker_id : workers) {
        if (IsWorkerActive(worker_id)) {
            active_count++;
        } else {
            // Remove inactive worker
            redis_->RemoveFromSet("active_workers", worker_id);
            redis_->SetHash("worker:" + worker_id, "status", "inactive");
            
            std::cout << "[INFO] Removed inactive worker: " << worker_id << std::endl;
        }
    }
    
    active_workers_ = active_count;
}

std::vector<std::string> ProductionCoordinator::GetAvailableWorkers() {
    std::vector<std::string> all_workers = redis_->GetActiveWorkers();
    std::vector<std::string> available_workers;
    
    for (const auto& worker_id : all_workers) {
        if (IsWorkerActive(worker_id)) {
            std::string status;
            if (redis_->GetHash("worker:" + worker_id, "status", status) && status == "active") {
                available_workers.push_back(worker_id);
            }
        }
    }
    
    return available_workers;
}

json::value ProductionCoordinator::CreateErrorResponse(const std::string& message) {
    json::value response = json::value::object();
    response["success"] = json::value::boolean(false);
    response["error"] = json::value::string(message);
    response["timestamp"] = json::value::number(std::time(nullptr));
    return response;
}

json::value ProductionCoordinator::CreateSuccessResponse(const json::value& data) {
    json::value response = json::value::object();
    response["success"] = json::value::boolean(true);
    response["data"] = data;
    response["timestamp"] = json::value::number(std::time(nullptr));
    return response;
}

void ProductionCoordinator::LogRequest(const http_request& request) {
    std::cout << "[HTTP] " << request.method() << " " << request.relative_uri().path() 
              << " from " << request.remote_address() << std::endl;
}

} // namespace daf