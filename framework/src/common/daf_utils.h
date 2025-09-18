#pragma once

#include "daf_types.h"
#include <vector>
#include <string>
#include <cstdint>

namespace daf {

// Context classes for Map and Reduce operations
class MapContext {
public:
    virtual ~MapContext() = default;
    
    // Input operations
    virtual std::string read_input() = 0;
    virtual bool has_more_input() = 0;
    
    // Output operations
    virtual void emit(const std::string& key, const std::string& value) = 0;
    
    // Configuration
    virtual std::string get_parameter(const std::string& key) const = 0;
    virtual void set_status(const std::string& status) = 0;
    
    // Memory management
    virtual size_t get_memory_usage() const = 0;
    virtual size_t get_memory_limit() const = 0;
};

class ReduceContext {
public:
    virtual ~ReduceContext() = default;
    
    // Input operations
    virtual std::vector<std::string> get_values() = 0;
    virtual bool has_more_values() = 0;
    
    // Output operations
    virtual void emit(const std::string& value) = 0;
    
    // Configuration
    virtual std::string get_parameter(const std::string& key) const = 0;
    virtual void set_status(const std::string& status) = 0;
    
    // Memory management
    virtual size_t get_memory_usage() const = 0;
    virtual size_t get_memory_limit() const = 0;
};

// Utility functions
class Utils {
public:
    // File operations
    static bool file_exists(const std::string& path);
    static bool create_directory(const std::string& path);
    static bool delete_file(const std::string& path);
    static size_t get_file_size(const std::string& path);
    
    // String operations
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string trim(const std::string& str);
    static std::string to_lower(const std::string& str);
    
    // Time operations
    static int64_t get_timestamp_ms();
    static std::string format_timestamp(int64_t timestamp_ms);
    
    // Memory operations
    static size_t get_memory_usage();
    static size_t get_available_memory();
    
    // Production network operations
    static bool is_port_available(int port);
    static std::string get_local_ip();
    
    // Environment operations
    static std::string getenv_or_default(const std::string& var_name, const std::string& default_value);
};

// Simple logging
class Logger {
public:
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERR = 3  // Changed from ERROR to avoid Windows macro conflict
    };
    
    static void set_level(Level level);
    static void log(Level level, const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    
private:
    static Level current_level_;
    static std::string level_to_string(Level level);
};

} // namespace daf