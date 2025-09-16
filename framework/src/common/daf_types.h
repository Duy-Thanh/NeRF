#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

// Cross-platform compatibility
#ifdef _WIN32
    #include <windows.h>
    #undef ERROR  // Avoid Windows macro conflict
    #define DAF_EXPORT __declspec(dllexport)
    #define DAF_IMPORT __declspec(dllimport)
    #define DAF_API_CALL __cdecl
    typedef HMODULE PluginHandle;
#else
    #include <dlfcn.h>
    #define DAF_EXPORT __attribute__((visibility("default")))
    #define DAF_IMPORT
    #define DAF_API_CALL
    typedef void* PluginHandle;
#endif

namespace daf {

// Forward declarations
class MapContext;
class ReduceContext;

// Plugin function signatures
typedef void (DAF_API_CALL *MapFunction)(MapContext* context);
typedef void (DAF_API_CALL *ReduceFunction)(const char* key, ReduceContext* context);

// Error codes
enum class ErrorCode {
    SUCCESS = 0,
    MEMORY_ERROR = 1,
    IO_ERROR = 2,
    NETWORK_ERROR = 3,
    PLUGIN_ERROR = 4,
    INVALID_ARGUMENT = 5,
    INVALID_STATE = 6,
    TIMEOUT = 7,
    UNKNOWN_ERROR = 99
};

// Task status
enum class TaskStatus {
    PENDING = 0,
    RUNNING = 1,
    COMPLETED = 2,
    FAILED = 3,
    CANCELLED = 4
};

// Task types
enum class TaskType {
    MAP = 0,
    REDUCE = 1,
    SHUFFLE = 2
};

// Basic data structures
struct Task {
    std::string id;
    TaskType type;
    TaskStatus status;
    std::string plugin_name;
    std::vector<std::string> input_files;
    std::string output_file;
    std::map<std::string, std::string> parameters;
    int64_t created_time;
    int64_t started_time;
    int64_t completed_time;
};

struct WorkerInfo {
    std::string id;
    std::string host;
    int port;
    bool is_available;
    int64_t last_heartbeat;
    int memory_usage_mb;
    int cpu_usage_percent;
};

struct JobConfig {
    std::string job_id;
    std::string plugin_name;
    std::vector<std::string> input_files;
    std::string output_directory;
    int num_map_tasks;
    int num_reduce_tasks;
    std::map<std::string, std::string> parameters;
};

// Task data structures for plugin processing
struct TaskData {
    std::string task_id;
    std::string data_type;
    std::vector<uint8_t> binary_data;
    std::map<std::string, std::string> metadata;
    std::string input_path;
    size_t data_size;
};

struct TaskResult {
    std::string task_id;
    bool success;
    std::string error_message;
    std::vector<uint8_t> output_data;
    std::map<std::string, std::string> result_metadata;
    std::string output_path;
    double processing_time_ms;
};

// Memory management for 512MB constraint
constexpr size_t MAX_MEMORY_MB = 400; // Leave 112MB for system overhead
constexpr size_t MAX_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB max buffer
constexpr size_t DEFAULT_BUFFER_SIZE = 4 * 1024 * 1024; // 4MB default buffer

} // namespace daf