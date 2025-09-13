#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>

namespace daf {

// Forward declarations
class MapContext;
class ReduceContext;

// Plugin interface - must be implemented by each plugin
class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // Plugin metadata
    virtual std::string GetName() const = 0;
    virtual std::string GetVersion() const = 0;
    virtual std::vector<std::string> GetDependencies() const = 0;
    
    // Initialize plugin with configuration
    virtual bool Initialize(const std::map<std::string, std::string>& config) = 0;
    
    // Cleanup resources
    virtual void Shutdown() = 0;
    
    // Execute map operation
    virtual bool ExecuteMap(MapContext* context) = 0;
    
    // Execute reduce operation  
    virtual bool ExecuteReduce(const std::string& key, ReduceContext* context) = 0;
};

// Context for map operations
class MapContext {
public:
    virtual ~MapContext() = default;
    
    // Input data access
    virtual bool HasMoreInput() const = 0;
    virtual std::string ReadInputLine() = 0;
    virtual std::vector<uint8_t> ReadInputChunk(size_t max_size) = 0;
    
    // Output emission
    virtual void Emit(const std::string& key, const std::string& value) = 0;
    virtual void EmitBinary(const std::string& key, const std::vector<uint8_t>& value) = 0;
    
    // Configuration access
    virtual std::string GetConfig(const std::string& key, const std::string& default_value = "") const = 0;
    
    // Progress reporting
    virtual void ReportProgress(float progress, const std::string& message = "") = 0;
    
    // Logging
    virtual void LogInfo(const std::string& message) = 0;
    virtual void LogError(const std::string& message) = 0;
    
    // Resource management
    virtual size_t GetAvailableMemoryMB() const = 0;
    virtual std::string GetTempDirectory() const = 0;
};

// Context for reduce operations
class ReduceContext {
public:
    virtual ~ReduceContext() = default;
    
    // Input values for a specific key
    virtual bool HasMoreValues() const = 0;
    virtual std::string ReadNextValue() = 0;
    virtual std::vector<uint8_t> ReadNextBinaryValue() = 0;
    
    // Output
    virtual void WriteOutput(const std::string& value) = 0;
    virtual void WriteBinaryOutput(const std::vector<uint8_t>& data) = 0;
    
    // Configuration access
    virtual std::string GetConfig(const std::string& key, const std::string& default_value = "") const = 0;
    
    // Progress reporting
    virtual void ReportProgress(float progress, const std::string& message = "") = 0;
    
    // Logging
    virtual void LogInfo(const std::string& message) = 0;
    virtual void LogError(const std::string& message) = 0;
    
    // Resource management
    virtual size_t GetAvailableMemoryMB() const = 0;
    virtual std::string GetTempDirectory() const = 0;
};

// Plugin factory function signature
using PluginCreateFunc = std::function<std::unique_ptr<IPlugin>()>;

// Error codes for framework operations
enum class ErrorCode {
    SUCCESS = 0,
    MEMORY_ERROR = 1,
    IO_ERROR = 2,
    NETWORK_ERROR = 3,
    PLUGIN_ERROR = 4,
    CONFIG_ERROR = 5,
    TIMEOUT_ERROR = 6,
    RESOURCE_EXHAUSTED = 7,
    INVALID_STATE = 8
};

// Result wrapper for operations
template<typename T>
class Result {
private:
    bool success_;
    T value_;
    ErrorCode error_code_;
    std::string error_message_;

public:
    Result(T&& value) : success_(true), value_(std::move(value)), error_code_(ErrorCode::SUCCESS) {}
    
    Result(ErrorCode code, const std::string& message) 
        : success_(false), error_code_(code), error_message_(message) {}
    
    bool IsSuccess() const { return success_; }
    bool IsError() const { return !success_; }
    
    const T& Value() const { return value_; }
    T&& TakeValue() { return std::move(value_); }
    
    ErrorCode Code() const { return error_code_; }
    const std::string& Message() const { return error_message_; }
};

// Utility functions
namespace utils {
    // Memory management
    size_t GetCurrentMemoryUsageMB();
    size_t GetAvailableMemoryMB();
    bool IsMemoryPressure();
    
    // File operations
    bool FileExists(const std::string& path);
    size_t GetFileSize(const std::string& path);
    std::vector<std::string> ListFiles(const std::string& directory);
    
    // String utilities
    std::vector<std::string> Split(const std::string& str, char delimiter);
    std::string Join(const std::vector<std::string>& parts, const std::string& delimiter);
    std::string Trim(const std::string& str);
    
    // Time utilities
    int64_t GetCurrentTimestamp();
    std::string FormatTimestamp(int64_t timestamp);
    
    // Hash utilities
    std::string ComputeHash(const std::string& data);
    std::string ComputeFileHash(const std::string& path);
}

} // namespace daf

// Plugin export macros for dynamic loading
#define DAF_EXPORT extern "C" __attribute__((visibility("default")))

#define DAF_PLUGIN_EXPORT(PluginClass) \
    DAF_EXPORT daf::IPlugin* CreatePlugin() { \
        return new PluginClass(); \
    } \
    \
    DAF_EXPORT void DestroyPlugin(daf::IPlugin* plugin) { \
        delete plugin; \
    } \
    \
    DAF_EXPORT const char* GetPluginName() { \
        static PluginClass instance; \
        return instance.GetName().c_str(); \
    } \
    \
    DAF_EXPORT const char* GetPluginVersion() { \
        static PluginClass instance; \
        return instance.GetVersion().c_str(); \
    }
