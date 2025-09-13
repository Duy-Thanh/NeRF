#pragma once

#include "daf_types.h"
#include <hiredis/hiredis.h>
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace daf {
namespace storage {

// Metadata store using Redis for job/task metadata
class MetadataStore {
public:
    explicit MetadataStore(const std::string& redis_host = "localhost", int redis_port = 6379);
    ~MetadataStore();
    
    // Connection management
    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    
    // Job metadata operations
    bool StoreJobMetadata(const std::string& job_id, const std::map<std::string, std::string>& metadata);
    Result<std::map<std::string, std::string>> GetJobMetadata(const std::string& job_id);
    bool UpdateJobStatus(const std::string& job_id, const std::string& status);
    bool DeleteJobMetadata(const std::string& job_id);
    
    // Task metadata operations
    bool StoreTaskMetadata(const std::string& task_id, const std::map<std::string, std::string>& metadata);
    Result<std::map<std::string, std::string>> GetTaskMetadata(const std::string& task_id);
    bool UpdateTaskStatus(const std::string& task_id, const std::string& status);
    bool DeleteTaskMetadata(const std::string& task_id);
    
    // Worker registration
    bool RegisterWorker(const std::string& worker_id, const std::map<std::string, std::string>& info);
    bool UpdateWorkerHeartbeat(const std::string& worker_id, int64_t timestamp);
    std::vector<std::string> GetActiveWorkers(int64_t timeout_ms = 30000);
    bool UnregisterWorker(const std::string& worker_id);
    
    // Task queue operations
    bool EnqueueTask(const std::string& queue_name, const std::string& task_data);
    Result<std::string> DequeueTask(const std::string& queue_name, int timeout_seconds = 10);
    size_t GetQueueSize(const std::string& queue_name);
    
    // Key-value operations
    bool Set(const std::string& key, const std::string& value, int ttl_seconds = 0);
    Result<std::string> Get(const std::string& key);
    bool Delete(const std::string& key);
    bool Exists(const std::string& key);
    
    // Atomic operations
    bool IncrementCounter(const std::string& key, int64_t increment = 1);
    Result<int64_t> GetCounter(const std::string& key);

private:
    std::string redis_host_;
    int redis_port_;
    redisContext* context_;
    
    // Helper methods
    bool ExecuteCommand(const std::string& command);
    Result<std::string> ExecuteCommandWithReply(const std::string& command);
    std::string FormatKey(const std::string& prefix, const std::string& id);
};

} // namespace storage
} // namespace daf
