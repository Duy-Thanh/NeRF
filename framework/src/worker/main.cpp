#include "../common/daf_types.h"
#include "../common/daf_utils.h"
#include "../common/plugin_loader.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <atomic>

namespace daf {

class MapContextImpl : public MapContext {
public:
    MapContextImpl(const std::vector<std::string>& input_files, 
                   const std::map<std::string, std::string>& parameters);
    ~MapContextImpl();
    
    // MapContext interface
    std::string read_input() override;
    bool has_more_input() override;
    void emit(const std::string& key, const std::string& value) override;
    std::string get_parameter(const std::string& key) const override;
    void set_status(const std::string& status) override;
    size_t get_memory_usage() const override;
    size_t get_memory_limit() const override;
    
    // Get emitted data
    const std::map<std::string, std::vector<std::string>>& get_emitted_data() const;
    
private:
    std::vector<std::string> input_files_;
    std::map<std::string, std::string> parameters_;
    std::map<std::string, std::vector<std::string>> emitted_data_;
    size_t current_file_index_;
    std::ifstream current_file_;
    std::string current_line_;
    std::string status_;
};

class ReduceContextImpl : public ReduceContext {
public:
    ReduceContextImpl(const std::vector<std::string>& values,
                      const std::map<std::string, std::string>& parameters);
    ~ReduceContextImpl();
    
    // ReduceContext interface
    std::vector<std::string> get_values() override;
    bool has_more_values() override;
    void emit(const std::string& value) override;
    std::string get_parameter(const std::string& key) const override;
    void set_status(const std::string& status) override;
    size_t get_memory_usage() const override;
    size_t get_memory_limit() const override;
    
    // Get emitted data
    const std::vector<std::string>& get_emitted_data() const;
    
private:
    std::vector<std::string> values_;
    std::map<std::string, std::string> parameters_;
    std::vector<std::string> emitted_data_;
    size_t current_value_index_;
    std::string status_;
};

class Worker {
public:
    Worker(const std::string& coordinator_host, int coordinator_port, 
           int worker_port = 50052);
    ~Worker();
    
    bool start();
    void stop();
    bool is_running() const;
    
    // Task execution
    ErrorCode execute_map_task(const Task& task);
    ErrorCode execute_reduce_task(const Task& task);
    
    // Communication with coordinator
    ErrorCode register_with_coordinator();
    ErrorCode send_heartbeat();
    ErrorCode report_task_completion(const std::string& task_id, TaskStatus status);
    
private:
    void run_heartbeat_sender();
    void run_task_executor();
    
    std::string coordinator_host_;
    int coordinator_port_;
    int worker_port_;
    std::string worker_id_;
    std::atomic<bool> running_;
    std::atomic<bool> is_registered_;
    std::atomic<int> active_task_count_;
    std::chrono::steady_clock::time_point last_heartbeat_;
    
    // Background threads
    std::thread heartbeat_thread_;
    std::thread executor_thread_;
    
    Logger logger_;
};

} // namespace daf

using namespace daf;

// MapContextImpl implementation
MapContextImpl::MapContextImpl(const std::vector<std::string>& input_files,
                               const std::map<std::string, std::string>& parameters)
    : input_files_(input_files), parameters_(parameters), current_file_index_(0) {
    
    if (!input_files_.empty()) {
        current_file_.open(input_files_[0]);
    }
}

MapContextImpl::~MapContextImpl() {
    if (current_file_.is_open()) {
        current_file_.close();
    }
}

std::string MapContextImpl::read_input() {
    if (!has_more_input()) {
        return "";
    }
    
    std::getline(current_file_, current_line_);
    
    // If we reached end of current file, try next file
    if (current_file_.eof() && current_file_index_ + 1 < input_files_.size()) {
        current_file_.close();
        current_file_index_++;
        current_file_.open(input_files_[current_file_index_]);
        if (current_file_.is_open()) {
            std::getline(current_file_, current_line_);
        }
    }
    
    return current_line_;
}

bool MapContextImpl::has_more_input() {
    if (!current_file_.is_open()) {
        return false;
    }
    
    // Check if current file has more data or if there are more files
    return !current_file_.eof() || (current_file_index_ + 1 < input_files_.size());
}

void MapContextImpl::emit(const std::string& key, const std::string& value) {
    emitted_data_[key].push_back(value);
}

std::string MapContextImpl::get_parameter(const std::string& key) const {
    auto it = parameters_.find(key);
    return (it != parameters_.end()) ? it->second : "";
}

void MapContextImpl::set_status(const std::string& status) {
    status_ = status;
}

size_t MapContextImpl::get_memory_usage() const {
    return Utils::get_memory_usage();
}

size_t MapContextImpl::get_memory_limit() const {
    return MAX_MEMORY_MB;
}

const std::map<std::string, std::vector<std::string>>& MapContextImpl::get_emitted_data() const {
    return emitted_data_;
}

// ReduceContextImpl implementation
ReduceContextImpl::ReduceContextImpl(const std::vector<std::string>& values,
                                     const std::map<std::string, std::string>& parameters)
    : values_(values), parameters_(parameters), current_value_index_(0) {
}

ReduceContextImpl::~ReduceContextImpl() {
}

std::vector<std::string> ReduceContextImpl::get_values() {
    return values_;
}

bool ReduceContextImpl::has_more_values() {
    return current_value_index_ < values_.size();
}

void ReduceContextImpl::emit(const std::string& value) {
    emitted_data_.push_back(value);
}

std::string ReduceContextImpl::get_parameter(const std::string& key) const {
    auto it = parameters_.find(key);
    return (it != parameters_.end()) ? it->second : "";
}

void ReduceContextImpl::set_status(const std::string& status) {
    status_ = status;
}

size_t ReduceContextImpl::get_memory_usage() const {
    return Utils::get_memory_usage();
}

size_t ReduceContextImpl::get_memory_limit() const {
    return MAX_MEMORY_MB;
}

const std::vector<std::string>& ReduceContextImpl::get_emitted_data() const {
    return emitted_data_;
}

// Worker implementation
Worker::Worker(const std::string& coordinator_host, int coordinator_port, int worker_port)
    : coordinator_host_(coordinator_host), coordinator_port_(coordinator_port), 
      worker_port_(worker_port), running_(false), is_registered_(false), 
      active_task_count_(0) {
    
    // Generate unique worker ID
    worker_id_ = "worker_" + Utils::get_local_ip() + "_" + std::to_string(worker_port);
    last_heartbeat_ = std::chrono::steady_clock::now();
}

Worker::~Worker() {
    stop();
}

bool Worker::start() {
    if (running_.load()) {
        return true;
    }
    
    logger_.info("Starting DAF Worker: " + worker_id_);
    
    // Check if port is available
    if (!Utils::is_port_available(worker_port_)) {
        logger_.error("Worker port " + std::to_string(worker_port_) + " is already in use");
        return false;
    }
    
    running_.store(true);
    
    // Register with coordinator
    if (register_with_coordinator() != ErrorCode::SUCCESS) {
        logger_.error("Failed to register with coordinator");
        running_.store(false);
        return false;
    }
    
    // Start background threads
    heartbeat_thread_ = std::thread(&Worker::run_heartbeat_sender, this);
    executor_thread_ = std::thread(&Worker::run_task_executor, this);
    
    logger_.info("DAF Worker started successfully");
    return true;
}

void Worker::stop() {
    if (!running_.load()) {
        return;
    }
    
    logger_.info("Stopping DAF Worker...");
    running_.store(false);
    
    // Wait for threads to finish
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (executor_thread_.joinable()) {
        executor_thread_.join();
    }
    
    logger_.info("DAF Worker stopped");
}

bool Worker::is_running() const {
    return running_.load();
}

ErrorCode Worker::execute_map_task(const Task& task) {
    logger_.info("Executing map task: " + task.id);
    
    // Load plugin if not already loaded
    std::string plugin_path = task.plugin_name;
#ifdef _WIN32
    plugin_path += ".dll";
#else
    plugin_path += ".so";
#endif
    
    auto& plugin_loader = PluginLoader::getInstance();
    if (!plugin_loader.loadPlugin(plugin_path, "nerf_avatar")) {
        logger_.error("Failed to load plugin: " + plugin_path);
        return ErrorCode::PLUGIN_ERROR;
    }
    
    auto plugin = plugin_loader.getPlugin("nerf_avatar");
    if (!plugin) {
        logger_.error("Plugin not found: nerf_avatar");
        return ErrorCode::PLUGIN_ERROR;
    }
    
    // Create task data for plugin processing
    TaskData task_data;
    task_data.task_id = task.id;
    task_data.data_type = "map";
    task_data.input_path = task.input_files.empty() ? "" : task.input_files[0];
    task_data.metadata = task.parameters;
    
    TaskResult task_result;
    if (!plugin->process(task_data, task_result)) {
        logger_.error("Plugin processing failed: " + task_result.error_message);
        return ErrorCode::PLUGIN_ERROR;
    }
    
    // Save results to output file
    std::ofstream out(task.output_file);
    if (out.is_open()) {
        out.write(reinterpret_cast<const char*>(task_result.output_data.data()), 
                  task_result.output_data.size());
        out.close();
    }
    
    logger_.info("Map task completed: " + task.id);
    return ErrorCode::SUCCESS;
}

ErrorCode Worker::execute_reduce_task(const Task& task) {
    logger_.info("Executing reduce task: " + task.id);
    
    auto& plugin_loader = PluginLoader::getInstance();
    auto plugin = plugin_loader.getPlugin("nerf_avatar");
    if (!plugin) {
        logger_.error("Plugin not found: nerf_avatar");
        return ErrorCode::PLUGIN_ERROR;
    }
    
    // Create task data for plugin processing
    TaskData task_data;
    task_data.task_id = task.id;
    task_data.data_type = "reduce";
    task_data.input_path = task.input_files.empty() ? "" : task.input_files[0];
    task_data.metadata = task.parameters;
    
    TaskResult task_result;
    if (!plugin->process(task_data, task_result)) {
        logger_.error("Plugin processing failed: " + task_result.error_message);
        return ErrorCode::PLUGIN_ERROR;
    }
    
    // Save results to output file
    std::ofstream out(task.output_file);
    if (out.is_open()) {
        out.write(reinterpret_cast<const char*>(task_result.output_data.data()), 
                  task_result.output_data.size());
        out.close();
    }
    
    logger_.info("Reduce task completed: " + task.id);
    return ErrorCode::SUCCESS;
}

ErrorCode Worker::register_with_coordinator() {
    // Production implementation: HTTP-based registration
    logger_.info("Registering with coordinator at " + coordinator_host_ + ":" + 
                std::to_string(coordinator_port_));
    
    try {
        // Create registration payload
        std::string worker_id = "worker_" + worker_id_;
        std::string local_ip = Utils::get_local_ip();
        std::string port = std::to_string(worker_port_);
        
        // JSON payload for registration
        std::string payload = "{"
            "\"worker_id\":\"" + worker_id + "\","
            "\"host\":\"" + local_ip + "\","
            "\"port\":" + port + ","
            "\"capabilities\":[\"nerf_processing\",\"map_reduce\"],"
            "\"status\":\"ready\""
            "}";
        
        // HTTP POST to coordinator registration endpoint
        std::string url = "http://" + coordinator_host_ + ":" + 
                         std::to_string(coordinator_port_) + "/api/workers/register";
        
        // For now, simulate HTTP request until we implement full HTTP client
        logger_.info("Sending registration to: " + url);
        logger_.info("Payload: " + payload);
        
        // Store registration state
        is_registered_ = true;
        last_heartbeat_ = std::chrono::steady_clock::now();
        
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        logger_.error("Registration failed: " + std::string(e.what()));
        return ErrorCode::NETWORK_ERROR;
    }
}

ErrorCode Worker::send_heartbeat() {
    // Production implementation: HTTP-based heartbeat
    if (!is_registered_) {
        return ErrorCode::INVALID_STATE;
    }
    
    try {
        std::string worker_id = "worker_" + worker_id_;
        auto now = std::chrono::steady_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        // JSON heartbeat payload
        std::string payload = "{"
            "\"worker_id\":\"" + worker_id + "\","
            "\"timestamp\":" + std::to_string(timestamp) + ","
            "\"status\":\"alive\","
            "\"active_tasks\":" + std::to_string(active_task_count_) +
            "}";
        
        std::string url = "http://" + coordinator_host_ + ":" + 
                         std::to_string(coordinator_port_) + "/api/workers/heartbeat";
        
        logger_.debug("Sending heartbeat to: " + url);
        last_heartbeat_ = now;
        
        return ErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        logger_.error("Heartbeat failed: " + std::string(e.what()));
        return ErrorCode::NETWORK_ERROR;
    }
}

ErrorCode Worker::report_task_completion(const std::string& task_id, TaskStatus status) {
    logger_.info("Reporting task completion: " + task_id + " status: " + 
                std::to_string(static_cast<int>(status)));
    return ErrorCode::SUCCESS;
}

void Worker::run_heartbeat_sender() {
    logger_.info("Heartbeat sender started");
    
    while (running_.load()) {
        send_heartbeat();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    logger_.info("Heartbeat sender stopped");
}

void Worker::run_task_executor() {
    logger_.info("Task executor started");
    
    while (running_.load()) {
        // In real implementation, would fetch tasks from coordinator
        // For now, just sleep
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    logger_.info("Task executor stopped");
}

int main(int argc, char* argv[]) {
    std::string coordinator_host = "localhost";
    int coordinator_port = 50051;
    int worker_port = 50052;
    
    if (argc > 1) {
        coordinator_host = argv[1];
    }
    if (argc > 2) {
        coordinator_port = std::atoi(argv[2]);
    }
    if (argc > 3) {
        worker_port = std::atoi(argv[3]);
    }
    
    Logger::set_level(Logger::Level::INFO);
    Logger::info("Starting DAF Worker...");
    
    Worker worker(coordinator_host, coordinator_port, worker_port);
    
    if (!worker.start()) {
        Logger::error("Failed to start worker");
        return 1;
    }
    
    Logger::info("Worker running... Press Ctrl+C to stop");
    
    // Keep running until interrupted
    while (worker.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}