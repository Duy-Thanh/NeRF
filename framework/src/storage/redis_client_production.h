#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// Forward declare hiredis types
struct redisContext;
struct redisReply;

namespace daf {

/**
 * Production Redis Client using hiredis library
 * This replaces all simulation code with real Redis connectivity
 */
class RedisClientProduction {
public:
    RedisClientProduction();
    ~RedisClientProduction();
    
    // Disable copy constructor and assignment
    RedisClientProduction(const RedisClientProduction&) = delete;
    RedisClientProduction& operator=(const RedisClientProduction&) = delete;
    
    // Connection management
    bool Connect(const std::string& host = "localhost", int port = 6379);
    void Disconnect();
    bool IsConnected() const;
    bool Ping();
    bool Reconnect();
    
    // Basic string operations
    bool Set(const std::string& key, const std::string& value);
    bool Get(const std::string& key, std::string& value);
    bool Delete(const std::string& key);
    bool Exists(const std::string& key);
    bool SetExpire(const std::string& key, int seconds);
    
    // Hash operations
    bool SetHash(const std::string& key, const std::string& field, const std::string& value);
    bool GetHash(const std::string& key, const std::string& field, std::string& value);
    bool DeleteHashField(const std::string& key, const std::string& field);
    bool HashExists(const std::string& key, const std::string& field);
    std::vector<std::string> GetHashKeys(const std::string& key);
    std::unordered_map<std::string, std::string> GetAllHash(const std::string& key);
    
    // List operations (for task queues)
    bool PushLeft(const std::string& key, const std::string& value);
    bool PushRight(const std::string& key, const std::string& value);
    bool PopLeft(const std::string& key, std::string& value);
    bool PopRight(const std::string& key, std::string& value);
    int GetListLength(const std::string& key);
    std::vector<std::string> GetListRange(const std::string& key, int start, int stop);
    bool RemoveFromList(const std::string& key, int count, const std::string& value);
    
    // Set operations
    bool AddToSet(const std::string& key, const std::string& member);
    bool RemoveFromSet(const std::string& key, const std::string& member);
    bool IsMemberOfSet(const std::string& key, const std::string& member);
    std::vector<std::string> GetSetMembers(const std::string& key);
    int GetSetSize(const std::string& key);
    
    // Atomic operations
    int Increment(const std::string& key);
    int Decrement(const std::string& key);
    int IncrementBy(const std::string& key, int value);
    
    // Pub/Sub operations
    bool Publish(const std::string& channel, const std::string& message);
    bool Subscribe(const std::string& channel);
    bool Unsubscribe(const std::string& channel);
    
    // Transaction support
    bool StartTransaction();
    bool ExecuteTransaction();
    bool DiscardTransaction();
    
    // Key management
    std::vector<std::string> GetKeys(const std::string& pattern = "*");
    bool FlushDatabase();
    bool FlushAll();
    
    // Connection info
    std::string GetConnectionInfo() const;
    std::string GetServerInfo() const;
    
    // High-level DAF operations
    bool RegisterWorker(const std::string& worker_id, const std::string& host, int port);
    bool UpdateWorkerHeartbeat(const std::string& worker_id);
    std::vector<std::string> GetActiveWorkers();
    bool SubmitJob(const std::string& job_id, const std::string& job_config);
    bool AddTask(const std::string& job_id, const std::string& task_id, const std::string& task_data);
    bool GetNextTask(const std::string& worker_id, std::string& task_data);
    bool CompleteTask(const std::string& task_id, const std::string& result);
    bool FailTask(const std::string& task_id, const std::string& error);
    
private:
    redisContext* context_;
    std::string host_;
    int port_;
    bool connected_;
    
    // Helper methods
    redisReply* ExecuteCommand(const char* format, ...);
    void FreeReply(redisReply* reply);
    bool CheckReplyType(redisReply* reply, int expected_type);
    
    // Error handling
    void LogError(const std::string& operation, const std::string& error);
    bool HandleConnectionError();
};

} // namespace daf
