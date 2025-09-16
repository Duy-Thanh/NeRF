#pragma once

#include "../storage/redis_client_production.h"
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>

namespace daf {

/**
 * Production Coordinator with real HTTP server and Redis backend
 * Replaces all simulation/simplified components with production-grade implementations
 */
class ProductionCoordinator {
public:
    ProductionCoordinator(int http_port = 8080, int grpc_port = 50051);
    ~ProductionCoordinator();
    
    // Lifecycle management
    bool Initialize();
    bool Start();
    void Stop();
    bool IsRunning() const { return running_; }
    
    // Configuration
    void SetRedisConnection(const std::string& host, int port);
    void SetWorkerTimeout(int seconds) { worker_timeout_ = seconds; }
    void SetJobProcessingInterval(int seconds) { job_processing_interval_ = seconds; }
    
private:
    // HTTP API handlers
    void HandleGetStatus(web::http::http_request request);
    void HandlePostJobs(web::http::http_request request);
    void HandleGetJobStatus(web::http::http_request request);
    void HandleGetWorkers(web::http::http_request request);
    void HandleDeleteJob(web::http::http_request request);
    
    // Background processing
    void JobProcessingLoop();
    void WorkerMonitoringLoop();
    void CleanupLoop();
    
    // Job management
    std::string GenerateJobId();
    bool ProcessPendingJobs();
    bool DistributeTask(const std::string& job_id, const std::string& task_data);
    bool CheckJobCompletion(const std::string& job_id);
    
    // Worker management
    bool IsWorkerActive(const std::string& worker_id);
    void RemoveInactiveWorkers();
    std::vector<std::string> GetAvailableWorkers();
    
    // Utility methods
    web::json::value CreateErrorResponse(const std::string& message);
    web::json::value CreateSuccessResponse(const web::json::value& data);
    void LogRequest(const web::http::http_request& request);
    
    // Configuration
    int http_port_;
    int grpc_port_;
    std::string redis_host_;
    int redis_port_;
    int worker_timeout_;
    int job_processing_interval_;
    
    // Core components
    std::unique_ptr<RedisClientProduction> redis_;
    std::unique_ptr<web::http::experimental::listener::http_listener> http_listener_;
    
    // Background threads
    std::thread job_processing_thread_;
    std::thread worker_monitoring_thread_;
    std::thread cleanup_thread_;
    
    // State management
    std::atomic<bool> running_;
    std::atomic<bool> stopping_;
    mutable std::mutex state_mutex_;
    
    // Statistics
    std::atomic<int> total_jobs_;
    std::atomic<int> completed_jobs_;
    std::atomic<int> failed_jobs_;
    std::atomic<int> active_workers_;
};

} // namespace daf