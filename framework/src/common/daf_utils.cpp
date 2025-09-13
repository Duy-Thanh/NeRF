#include "daf_types.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/resource.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <iomanip>
#include <chrono>

namespace daf {
namespace utils {

size_t GetCurrentMemoryUsageMB() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        // ru_maxrss is in KB on Linux
        return usage.ru_maxrss / 1024;
    }
    return 0;
}

size_t GetAvailableMemoryMB() {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return (pages * page_size) / (1024 * 1024);
}

bool IsMemoryPressure() {
    size_t current = GetCurrentMemoryUsageMB();
    size_t available = GetAvailableMemoryMB();
    
    // Memory pressure if using more than 80% of available memory
    return current > (available * 0.8);
}

bool FileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

size_t GetFileSize(const std::string& path) {
    try {
        return std::filesystem::file_size(path);
    } catch (const std::filesystem::filesystem_error&) {
        return 0;
    }
}

std::vector<std::string> ListFiles(const std::string& directory) {
    std::vector<std::string> files;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Return empty vector on error
    }
    return files;
}

std::vector<std::string> Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string Join(const std::vector<std::string>& parts, const std::string& delimiter) {
    if (parts.empty()) return "";
    
    std::stringstream ss;
    ss << parts[0];
    
    for (size_t i = 1; i < parts.size(); ++i) {
        ss << delimiter << parts[i];
    }
    
    return ss.str();
}

std::string Trim(const std::string& str) {
    auto start = str.begin();
    auto end = str.end();
    
    // Trim leading whitespace
    start = std::find_if(start, end, [](unsigned char ch) {
        return !std::isspace(ch);
    });
    
    // Trim trailing whitespace
    end = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();
    
    return std::string(start, end);
}

int64_t GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string FormatTimestamp(int64_t timestamp) {
    auto time_point = std::chrono::system_clock::from_time_t(timestamp / 1000);
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    // Add milliseconds
    int ms = timestamp % 1000;
    ss << "." << std::setfill('0') << std::setw(3) << ms;
    
    return ss.str();
}

std::string ComputeHash(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.c_str(), data.size());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

std::string ComputeFileHash(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

} // namespace utils
} // namespace daf
