#include "daf_utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <io.h>
    #include <direct.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "psapi.lib")
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <netdb.h>
    #include <sys/resource.h>
    #include <cstring>
#endif

namespace daf {

// PluginLoader implementation
PluginLoader::PluginLoader() 
    : handle_(nullptr), map_function_(nullptr), reduce_function_(nullptr), is_loaded_(false) {
}

PluginLoader::~PluginLoader() {
    unload_plugin();
}

ErrorCode PluginLoader::load_plugin(const std::string& plugin_path) {
    unload_plugin();
    
#ifdef _WIN32
    handle_ = LoadLibraryA(plugin_path.c_str());
    if (!handle_) {
        DWORD error = GetLastError();
        last_error_ = "Failed to load plugin: " + std::to_string(error);
        return ErrorCode::PLUGIN_ERROR;
    }
    
    map_function_ = reinterpret_cast<MapFunction>(GetProcAddress(handle_, "MapMain"));
    reduce_function_ = reinterpret_cast<ReduceFunction>(GetProcAddress(handle_, "ReduceMain"));
#else
    handle_ = dlopen(plugin_path.c_str(), RTLD_LAZY);
    if (!handle_) {
        last_error_ = "Failed to load plugin: " + std::string(dlerror());
        return ErrorCode::PLUGIN_ERROR;
    }
    
    map_function_ = reinterpret_cast<MapFunction>(dlsym(handle_, "MapMain"));
    reduce_function_ = reinterpret_cast<ReduceFunction>(dlsym(handle_, "ReduceMain"));
#endif
    
    is_loaded_ = true;
    return ErrorCode::SUCCESS;
}

void PluginLoader::unload_plugin() {
    if (handle_) {
#ifdef _WIN32
        FreeLibrary(handle_);
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
    }
    
    map_function_ = nullptr;
    reduce_function_ = nullptr;
    is_loaded_ = false;
}

MapFunction PluginLoader::get_map_function() const {
    return map_function_;
}

ReduceFunction PluginLoader::get_reduce_function() const {
    return reduce_function_;
}

bool PluginLoader::is_loaded() const {
    return is_loaded_;
}

std::string PluginLoader::get_last_error() const {
    return last_error_;
}

// Utils implementation
bool Utils::file_exists(const std::string& path) {
#ifdef _WIN32
    return _access(path.c_str(), 0) == 0;
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

bool Utils::create_directory(const std::string& path) {
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool Utils::delete_file(const std::string& path) {
    return std::remove(path.c_str()) == 0;
}

size_t Utils::get_file_size(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return 0;
    }
    return static_cast<size_t>(file.tellg());
}

std::vector<std::string> Utils::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string Utils::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string Utils::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

int64_t Utils::get_timestamp_ms() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string Utils::format_timestamp(int64_t timestamp_ms) {
    auto time_point = std::chrono::system_clock::from_time_t(timestamp_ms / 1000);
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

size_t Utils::get_memory_usage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024 * 1024); // Convert to MB
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss / 1024; // Convert to MB (Linux reports in KB)
    }
    return 0;
#endif
}

size_t Utils::get_available_memory() {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullAvailPhys / (1024 * 1024); // Convert to MB
    }
    return 0;
#else
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return (pages * page_size) / (1024 * 1024); // Convert to MB
#endif
}

bool Utils::is_port_available(int port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    bool available = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0;
    
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    
    return available;
}

std::string Utils::get_local_ip() {
    // Production implementation: Get actual local IP address
#ifdef _WIN32
    // Windows implementation
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return "127.0.0.1"; // Fallback
    }
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* host_entry = gethostbyname(hostname);
        if (host_entry != nullptr && host_entry->h_addr_list[0] != nullptr) {
            struct in_addr addr;
            memcpy(&addr, host_entry->h_addr_list[0], sizeof(struct in_addr));
            std::string ip = inet_ntoa(addr);
            WSACleanup();
            return ip;
        }
    }
    WSACleanup();
    return "127.0.0.1"; // Fallback
#else
    // Linux implementation
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    
    if (getifaddrs(&ifaddr) == -1) {
        return "127.0.0.1"; // Fallback
    }
    
    // Look for first non-loopback IPv4 address
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        
        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                          host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            if (s == 0 && strcmp(host, "127.0.0.1") != 0) {
                freeifaddrs(ifaddr);
                return std::string(host);
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return "127.0.0.1"; // Fallback
#endif
}

std::string Utils::getenv_or_default(const std::string& var_name, const std::string& default_value) {
    const char* env_value = std::getenv(var_name.c_str());
    return (env_value != nullptr) ? std::string(env_value) : default_value;
}

// Logger implementation
Logger::Level Logger::current_level_ = Logger::Level::INFO;

void Logger::set_level(Level level) {
    current_level_ = level;
}

void Logger::log(Level level, const std::string& message) {
    if (level < current_level_) {
        return;
    }
    
    auto timestamp = Utils::get_timestamp_ms();
    std::cout << "[" << Utils::format_timestamp(timestamp) << "] "
              << "[" << level_to_string(level) << "] "
              << message << std::endl;
}

void Logger::debug(const std::string& message) {
    log(Level::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(Level::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(Level::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(Level::ERR, message);
}

std::string Logger::level_to_string(Level level) {
    switch (level) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO: return "INFO";
        case Level::WARNING: return "WARN";
        case Level::ERR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace daf