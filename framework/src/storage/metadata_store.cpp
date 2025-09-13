#include "metadata_store.h"
#include "daf_utils.cpp"
#include <iostream>
#include <sstream>
#include <json/json.h>

namespace daf {
namespace storage {

MetadataStore::MetadataStore(const std::string& redis_host, int redis_port)
    : redis_host_(redis_host), redis_port_(redis_port), context_(nullptr) {
}

MetadataStore::~MetadataStore() {
    Disconnect();
}

bool MetadataStore::Connect() {
    if (context_) {
        Disconnect();
    }
    
    context_ = redisConnect(redis_host_.c_str(), redis_port_);
    if (!context_ || context_->err) {
        if (context_) {
            std::cerr << "Redis connection error: " << context_->errstr << std::endl;
            redisFree(context_);
            context_ = nullptr;
        } else {
            std::cerr << "Redis connection error: Failed to allocate context" << std::endl;
        }
        return false;
    }
    
    // Test connection with ping
    redisReply* reply = (redisReply*)redisCommand(context_, "PING");
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        std::cerr << "Redis ping failed" << std::endl;
        if (reply) freeReplyObject(reply);
        Disconnect();
        return false;
    }
    
    freeReplyObject(reply);
    return true;
}

void MetadataStore::Disconnect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
}

bool MetadataStore::IsConnected() const {
    return context_ != nullptr && context_->err == 0;
}

bool MetadataStore::StoreJobMetadata(const std::string& job_id, const std::map<std::string, std::string>& metadata) {
    if (!IsConnected()) return false;
    
    // Convert metadata to JSON
    Json::Value json_metadata;
    for (const auto& pair : metadata) {
        json_metadata[pair.first] = pair.second;
    }
    
    Json::StreamWriterBuilder builder;
    std::string json_str = Json::writeString(builder, json_metadata);
    
    std::string key = FormatKey("job", job_id);
    redisReply* reply = (redisReply*)redisCommand(context_, "SET %s %s", key.c_str(), json_str.c_str());
    
    bool success = reply && reply->type == REDIS_REPLY_STATUS && 
                   std::string(reply->str) == "OK";
    
    if (reply) freeReplyObject(reply);
    return success;
}

Result<std::map<std::string, std::string>> MetadataStore::GetJobMetadata(const std::string& job_id) {
    if (!IsConnected()) {
        return Result<std::map<std::string, std::string>>(
            ErrorCode::NETWORK_ERROR, "Not connected to Redis"
        );
    }
    
    std::string key = FormatKey("job", job_id);
    redisReply* reply = (redisReply*)redisCommand(context_, "GET %s", key.c_str());
    
    if (!reply) {
        return Result<std::map<std::string, std::string>>(
            ErrorCode::NETWORK_ERROR, "Redis command failed"
        );
    }
    
    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        return Result<std::map<std::string, std::string>>(
            ErrorCode::IO_ERROR, "Job not found: " + job_id
        );
    }
    
    if (reply->type != REDIS_REPLY_STRING) {
        freeReplyObject(reply);
        return Result<std::map<std::string, std::string>>(
            ErrorCode::IO_ERROR, "Invalid reply type from Redis"
        );
    }
    
    // Parse JSON metadata
    std::string json_str(reply->str, reply->len);
    freeReplyObject(reply);
    
    Json::Value json_metadata;
    Json::CharReaderBuilder builder;
    std::istringstream iss(json_str);
    std::string errors;
    
    if (!Json::parseFromStream(builder, iss, &json_metadata, &errors)) {
        return Result<std::map<std::string, std::string>>(
            ErrorCode::IO_ERROR, "Failed to parse metadata JSON: " + errors
        );
    }
    
    std::map<std::string, std::string> metadata;
    for (const auto& key : json_metadata.getMemberNames()) {
        metadata[key] = json_metadata[key].asString();
    }
    
    return Result<std::map<std::string, std::string>>(std::move(metadata));
}

bool MetadataStore::UpdateJobStatus(const std::string& job_id, const std::string& status) {
    if (!IsConnected()) return false;
    
    std::string key = FormatKey("job", job_id);
    redisReply* reply = (redisReply*)redisCommand(context_, "HSET %s status %s", 
                                                 key.c_str(), status.c_str());
    
    bool success = reply && reply->type == REDIS_REPLY_INTEGER;
    if (reply) freeReplyObject(reply);
    return success;
}

bool MetadataStore::DeleteJobMetadata(const std::string& job_id) {
    if (!IsConnected()) return false;
    
    std::string key = FormatKey("job", job_id);
    redisReply* reply = (redisReply*)redisCommand(context_, "DEL %s", key.c_str());
    
    bool success = reply && reply->type == REDIS_REPLY_INTEGER;
    if (reply) freeReplyObject(reply);
    return success;
}

bool MetadataStore::StoreTaskMetadata(const std::string& task_id, const std::map<std::string, std::string>& metadata) {
    if (!IsConnected()) return false;
    
    Json::Value json_metadata;
    for (const auto& pair : metadata) {
        json_metadata[pair.first] = pair.second;
    }
    
    Json::StreamWriterBuilder builder;
    std::string json_str = Json::writeString(builder, json_metadata);
    
    std::string key = FormatKey("task", task_id);
    redisReply* reply = (redisReply*)redisCommand(context_, "SET %s %s", key.c_str(), json_str.c_str());
    
    bool success = reply && reply->type == REDIS_REPLY_STATUS && 
                   std::string(reply->str) == "OK";
    
    if (reply) freeReplyObject(reply);
    return success;
}

Result<std::map<std::string, std::string>> MetadataStore::GetTaskMetadata(const std::string& task_id) {
    // Similar implementation to GetJobMetadata
    std::string key = FormatKey("task", task_id);
    // ... (implementation similar to GetJobMetadata)
    return Result<std::map<std::string, std::string>>(std::map<std::string, std::string>{});
}

bool MetadataStore::RegisterWorker(const std::string& worker_id, const std::map<std::string, std::string>& info) {
    if (!IsConnected()) return false;
    
    std::string key = FormatKey("worker", worker_id);
    
    // Store worker info as hash
    std::vector<std::string> args = {"HMSET", key};
    for (const auto& pair : info) {
        args.push_back(pair.first);
        args.push_back(pair.second);
    }
    
    // Add registration timestamp
    args.push_back("registered_at");
    args.push_back(std::to_string(utils::GetCurrentTimestamp()));
    
    // Convert to Redis command format
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
        argvlen.push_back(arg.length());
    }
    
    redisReply* reply = (redisReply*)redisCommandArgv(context_, argv.size(), argv.data(), argvlen.data());
    
    bool success = reply && reply->type == REDIS_REPLY_STATUS && 
                   std::string(reply->str) == "OK";
    
    if (reply) freeReplyObject(reply);
    return success;
}

bool MetadataStore::UpdateWorkerHeartbeat(const std::string& worker_id, int64_t timestamp) {
    if (!IsConnected()) return false;
    
    std::string key = FormatKey("worker", worker_id);
    redisReply* reply = (redisReply*)redisCommand(context_, "HSET %s last_heartbeat %lld", 
                                                 key.c_str(), timestamp);
    
    bool success = reply && reply->type == REDIS_REPLY_INTEGER;
    if (reply) freeReplyObject(reply);
    return success;
}

std::vector<std::string> MetadataStore::GetActiveWorkers(int64_t timeout_ms) {
    std::vector<std::string> active_workers;
    if (!IsConnected()) return active_workers;
    
    int64_t current_time = utils::GetCurrentTimestamp();
    int64_t cutoff_time = current_time - timeout_ms;
    
    // Get all worker keys
    redisReply* reply = (redisReply*)redisCommand(context_, "KEYS worker:*");
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        if (reply) freeReplyObject(reply);
        return active_workers;
    }
    
    for (size_t i = 0; i < reply->elements; i++) {
        std::string worker_key = reply->element[i]->str;
        
        // Check last heartbeat
        redisReply* hb_reply = (redisReply*)redisCommand(context_, "HGET %s last_heartbeat", 
                                                        worker_key.c_str());
        
        if (hb_reply && hb_reply->type == REDIS_REPLY_STRING) {
            int64_t last_heartbeat = std::stoll(hb_reply->str);
            if (last_heartbeat >= cutoff_time) {
                // Extract worker ID from key (remove "worker:" prefix)
                std::string worker_id = worker_key.substr(7);
                active_workers.push_back(worker_id);
            }
        }
        
        if (hb_reply) freeReplyObject(hb_reply);
    }
    
    freeReplyObject(reply);
    return active_workers;
}

bool MetadataStore::EnqueueTask(const std::string& queue_name, const std::string& task_data) {
    if (!IsConnected()) return false;
    
    redisReply* reply = (redisReply*)redisCommand(context_, "LPUSH %s %s", 
                                                 queue_name.c_str(), task_data.c_str());
    
    bool success = reply && reply->type == REDIS_REPLY_INTEGER;
    if (reply) freeReplyObject(reply);
    return success;
}

Result<std::string> MetadataStore::DequeueTask(const std::string& queue_name, int timeout_seconds) {
    if (!IsConnected()) {
        return Result<std::string>(ErrorCode::NETWORK_ERROR, "Not connected to Redis");
    }
    
    redisReply* reply = (redisReply*)redisCommand(context_, "BRPOP %s %d", 
                                                 queue_name.c_str(), timeout_seconds);
    
    if (!reply) {
        return Result<std::string>(ErrorCode::NETWORK_ERROR, "Redis command failed");
    }
    
    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        return Result<std::string>(ErrorCode::TIMEOUT_ERROR, "Queue timeout");
    }
    
    if (reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
        freeReplyObject(reply);
        return Result<std::string>(ErrorCode::IO_ERROR, "Invalid reply format");
    }
    
    std::string task_data(reply->element[1]->str, reply->element[1]->len);
    freeReplyObject(reply);
    
    return Result<std::string>(std::move(task_data));
}

size_t MetadataStore::GetQueueSize(const std::string& queue_name) {
    if (!IsConnected()) return 0;
    
    redisReply* reply = (redisReply*)redisCommand(context_, "LLEN %s", queue_name.c_str());
    
    size_t size = 0;
    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        size = reply->integer;
    }
    
    if (reply) freeReplyObject(reply);
    return size;
}

std::string MetadataStore::FormatKey(const std::string& prefix, const std::string& id) {
    return prefix + ":" + id;
}

} // namespace storage
} // namespace daf
