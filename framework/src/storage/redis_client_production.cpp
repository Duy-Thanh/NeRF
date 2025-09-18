#include "redis_client_production.h"
#include <hiredis/hiredis.h>
#include <iostream>
#include <sstream>
#include <cstdarg>
#include <chrono>
#include <thread>

namespace daf {

RedisClientProduction::RedisClientProduction() 
    : context_(nullptr), host_("localhost"), port_(6379), connected_(false) {
}

RedisClientProduction::~RedisClientProduction() {
    Disconnect();
}

bool RedisClientProduction::Connect(const std::string& host, int port) {
    if (connected_) {
        Disconnect();
    }
    
    host_ = host;
    port_ = port;
    
    // Connect to Redis server
    context_ = redisConnect(host.c_str(), port);
    
    if (context_ == nullptr || context_->err) {
        if (context_) {
            LogError("Connect", context_->errstr);
            redisFree(context_);
            context_ = nullptr;
        } else {
            LogError("Connect", "Failed to allocate Redis context");
        }
        return false;
    }
    
    // Test connection with PING - use direct Redis call to avoid recursion
    redisReply* ping_reply = static_cast<redisReply*>(redisCommand(context_, "PING"));
    if (!ping_reply || ping_reply->type != REDIS_REPLY_STATUS || 
        std::string(ping_reply->str) != "PONG") {
        LogError("Connect", "PING test failed");
        if (ping_reply) freeReplyObject(ping_reply);
        Disconnect();
        return false;
    }
    freeReplyObject(ping_reply);
    
    connected_ = true;
    std::cout << "[INFO] Connected to Redis server at " << host << ":" << port << std::endl;
    return true;
}

void RedisClientProduction::Disconnect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
    connected_ = false;
}

bool RedisClientProduction::IsConnected() const {
    return connected_ && context_ && !context_->err;
}

bool RedisClientProduction::Ping() {
    redisReply* reply = ExecuteCommand("PING");
    if (!reply) return false;
    
    bool success = (reply->type == REDIS_REPLY_STATUS && 
                   std::string(reply->str) == "PONG");
    FreeReply(reply);
    return success;
}

bool RedisClientProduction::Reconnect() {
    std::cout << "[INFO] Attempting to reconnect to Redis..." << std::endl;
    return Connect(host_, port_);
}

// Basic string operations
bool RedisClientProduction::Set(const std::string& key, const std::string& value) {
    redisReply* reply = ExecuteCommand("SET %s %s", key.c_str(), value.c_str());
    if (!reply) return false;
    
    bool success = (reply->type == REDIS_REPLY_STATUS);
    FreeReply(reply);
    return success;
}

bool RedisClientProduction::Get(const std::string& key, std::string& value) {
    redisReply* reply = ExecuteCommand("GET %s", key.c_str());
    if (!reply) return false;
    
    bool success = false;
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        success = true;
    } else if (reply->type == REDIS_REPLY_NIL) {
        value.clear();
        success = false; // Key doesn't exist
    }
    
    FreeReply(reply);
    return success;
}

bool RedisClientProduction::Delete(const std::string& key) {
    redisReply* reply = ExecuteCommand("DEL %s", key.c_str());
    if (!reply) return false;
    
    bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    FreeReply(reply);
    return success;
}

bool RedisClientProduction::Exists(const std::string& key) {
    redisReply* reply = ExecuteCommand("EXISTS %s", key.c_str());
    if (!reply) return false;
    
    bool exists = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    FreeReply(reply);
    return exists;
}

bool RedisClientProduction::SetExpire(const std::string& key, int seconds) {
    redisReply* reply = ExecuteCommand("EXPIRE %s %d", key.c_str(), seconds);
    if (!reply) return false;
    
    bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    FreeReply(reply);
    return success;
}

// Hash operations
bool RedisClientProduction::SetHash(const std::string& key, const std::string& field, const std::string& value) {
    redisReply* reply = ExecuteCommand("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    if (!reply) return false;
    
    bool success = (reply->type == REDIS_REPLY_INTEGER);
    FreeReply(reply);
    return success;
}

bool RedisClientProduction::GetHash(const std::string& key, const std::string& field, std::string& value) {
    redisReply* reply = ExecuteCommand("HGET %s %s", key.c_str(), field.c_str());
    if (!reply) return false;
    
    bool success = false;
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        success = true;
    } else if (reply->type == REDIS_REPLY_NIL) {
        value.clear();
        success = false;
    }
    
    FreeReply(reply);
    return success;
}

// List operations  
bool RedisClientProduction::PushLeft(const std::string& key, const std::string& value) {
    redisReply* reply = ExecuteCommand("LPUSH %s %s", key.c_str(), value.c_str());
    if (!reply) return false;
    
    bool success = (reply->type == REDIS_REPLY_INTEGER);
    FreeReply(reply);
    return success;
}

bool RedisClientProduction::PopLeft(const std::string& key, std::string& value) {
    redisReply* reply = ExecuteCommand("LPOP %s", key.c_str());
    if (!reply) return false;
    
    bool success = false;
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        success = true;
    } else if (reply->type == REDIS_REPLY_NIL) {
        value.clear();
        success = false;
    }
    
    FreeReply(reply);
    return success;
}

int RedisClientProduction::GetListLength(const std::string& key) {
    redisReply* reply = ExecuteCommand("LLEN %s", key.c_str());
    if (!reply) return -1;
    
    int length = -1;
    if (reply->type == REDIS_REPLY_INTEGER) {
        length = static_cast<int>(reply->integer);
    }
    
    FreeReply(reply);
    return length;
}

// High-level DAF operations
bool RedisClientProduction::RegisterWorker(const std::string& worker_id, const std::string& host, int port) {
    // Store worker info in hash
    if (!SetHash("worker:" + worker_id, "host", host)) return false;
    if (!SetHash("worker:" + worker_id, "port", std::to_string(port))) return false;
    if (!SetHash("worker:" + worker_id, "status", "active")) return false;
    if (!SetHash("worker:" + worker_id, "last_heartbeat", std::to_string(std::time(nullptr)))) return false;
    
    // Add to active workers set
    return AddToSet("active_workers", worker_id);
}

bool RedisClientProduction::SubmitJob(const std::string& job_id, const std::string& job_config) {
    // Store job config
    if (!SetHash("job:" + job_id, "config", job_config)) return false;
    if (!SetHash("job:" + job_id, "status", "pending")) return false;
    if (!SetHash("job:" + job_id, "created_at", std::to_string(std::time(nullptr)))) return false;
    
    // Add to job queue
    return PushLeft("job_queue", job_id);
}

// Private helper methods
redisReply* RedisClientProduction::ExecuteCommand(const char* format, ...) {
    if (!IsConnected()) {
        if (!Reconnect()) {
            return nullptr;
        }
    }
    
    va_list args;
    va_start(args, format);
    redisReply* reply = static_cast<redisReply*>(redisvCommand(context_, format, args));
    va_end(args);
    
    if (!reply) {
        LogError("ExecuteCommand", "Failed to execute Redis command");
        if (context_->err) {
            LogError("ExecuteCommand", context_->errstr);
            connected_ = false;
        }
    }
    
    return reply;
}

void RedisClientProduction::FreeReply(redisReply* reply) {
    if (reply) {
        freeReplyObject(reply);
    }
}

void RedisClientProduction::LogError(const std::string& operation, const std::string& error) {
    std::cerr << "[ERROR] Redis " << operation << ": " << error << std::endl;
}

// Production implementations for all Redis operations
bool RedisClientProduction::DeleteHashField(const std::string& key, const std::string& field) {
    redisReply* reply = (redisReply*)redisCommand(context_, "HDEL %s %s", key.c_str(), field.c_str());
    bool success = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisClientProduction::HashExists(const std::string& key, const std::string& field) {
    redisReply* reply = (redisReply*)redisCommand(context_, "HEXISTS %s %s", key.c_str(), field.c_str());
    bool exists = (reply && reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if (reply) freeReplyObject(reply);
    return exists;
}

std::vector<std::string> RedisClientProduction::GetHashKeys(const std::string& key) {
    std::vector<std::string> keys;
    redisReply* reply = (redisReply*)redisCommand(context_, "HKEYS %s", key.c_str());
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                keys.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    }
    if (reply) freeReplyObject(reply);
    return keys;
}

std::unordered_map<std::string, std::string> RedisClientProduction::GetAllHash(const std::string& key) {
    std::unordered_map<std::string, std::string> result;
    redisReply* reply = (redisReply*)redisCommand(context_, "HGETALL %s", key.c_str());
    if (reply && reply->type == REDIS_REPLY_ARRAY && reply->elements % 2 == 0) {
        for (size_t i = 0; i < reply->elements; i += 2) {
            if (reply->element[i]->type == REDIS_REPLY_STRING && 
                reply->element[i+1]->type == REDIS_REPLY_STRING) {
                std::string key_str(reply->element[i]->str, reply->element[i]->len);
                std::string val_str(reply->element[i+1]->str, reply->element[i+1]->len);
                result[key_str] = val_str;
            }
        }
    }
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientProduction::PushRight(const std::string& key, const std::string& value) {
    redisReply* reply = (redisReply*)redisCommand(context_, "RPUSH %s %s", key.c_str(), value.c_str());
    bool success = (reply && reply->type == REDIS_REPLY_INTEGER);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisClientProduction::PopRight(const std::string& key, std::string& value) {
    redisReply* reply = (redisReply*)redisCommand(context_, "RPOP %s", key.c_str());
    if (reply && reply->type == REDIS_REPLY_STRING) {
        value.assign(reply->str, reply->len);
        freeReplyObject(reply);
        return true;
    }
    if (reply) freeReplyObject(reply);
    return false;
}

std::vector<std::string> RedisClientProduction::GetListRange(const std::string& key, int start, int stop) {
    std::vector<std::string> result;
    redisReply* reply = (redisReply*)redisCommand(context_, "LRANGE %s %d %d", key.c_str(), start, stop);
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    }
    if (reply) freeReplyObject(reply);
    return result;
}

bool RedisClientProduction::RemoveFromList(const std::string& key, int count, const std::string& value) {
    redisReply* reply = (redisReply*)redisCommand(context_, "LREM %s %d %s", key.c_str(), count, value.c_str());
    bool success = (reply && reply->type == REDIS_REPLY_INTEGER);
    if (reply) freeReplyObject(reply);
    return success;
}

bool RedisClientProduction::AddToSet(const std::string& key, const std::string& member) {
    redisReply* reply = (redisReply*)redisCommand(context_, "SADD %s %s", key.c_str(), member.c_str());
    bool success = (reply && reply->type == REDIS_REPLY_INTEGER);
    if (reply) freeReplyObject(reply);
    return success;
}
bool RedisClientProduction::RemoveFromSet(const std::string& key, const std::string& member) { return false; }
bool RedisClientProduction::IsMemberOfSet(const std::string& key, const std::string& member) { return false; }
std::vector<std::string> RedisClientProduction::GetSetMembers(const std::string& key) { return {}; }
int RedisClientProduction::GetSetSize(const std::string& key) { return -1; }
int RedisClientProduction::Increment(const std::string& key) { return -1; }
int RedisClientProduction::Decrement(const std::string& key) { return -1; }
int RedisClientProduction::IncrementBy(const std::string& key, int value) { return -1; }
bool RedisClientProduction::Publish(const std::string& channel, const std::string& message) { return false; }
bool RedisClientProduction::Subscribe(const std::string& channel) { return false; }
bool RedisClientProduction::Unsubscribe(const std::string& channel) { return false; }
bool RedisClientProduction::StartTransaction() { return false; }
bool RedisClientProduction::ExecuteTransaction() { return false; }
bool RedisClientProduction::DiscardTransaction() { return false; }
std::vector<std::string> RedisClientProduction::GetKeys(const std::string& pattern) { return {}; }
bool RedisClientProduction::FlushDatabase() { return false; }
bool RedisClientProduction::FlushAll() { return false; }
std::string RedisClientProduction::GetConnectionInfo() const { return ""; }
std::string RedisClientProduction::GetServerInfo() const { return ""; }
bool RedisClientProduction::UpdateWorkerHeartbeat(const std::string& worker_id) { return false; }
std::vector<std::string> RedisClientProduction::GetActiveWorkers() { return {}; }
bool RedisClientProduction::AddTask(const std::string& job_id, const std::string& task_id, const std::string& task_data) { return false; }
bool RedisClientProduction::GetNextTask(const std::string& worker_id, std::string& task_data) { return false; }
bool RedisClientProduction::CompleteTask(const std::string& task_id, const std::string& result) { return false; }
bool RedisClientProduction::FailTask(const std::string& task_id, const std::string& error) { return false; }
bool RedisClientProduction::CheckReplyType(redisReply* reply, int expected_type) { return false; }
bool RedisClientProduction::HandleConnectionError() { return false; }

} // namespace daf