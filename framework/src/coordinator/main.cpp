#include "../common/daf_types.h"
#include "../common/daf_utils.h"
#include "../storage/redis_client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace daf {

class Coordinator {
public:
    Coordinator(int port = 50051);
    ~Coordinator();
    
    bool start();
    void stop();
    bool is_running() const;
    
    // Job management
    ErrorCode submit_job(const JobConfig& config);
    std::vector<Task> get_tasks(const std::string& job_id) const;
    TaskStatus get_task_status(const std::string& task_id) const;
    
    // Worker management
    ErrorCode register_worker(const WorkerInfo& worker);
    ErrorCode unregister_worker(const std::string& worker_id);
    std::vector<WorkerInfo> get_workers() const;
    
    // Task scheduling
    void schedule_tasks();
    ErrorCode assign_task(const std::string& task_id, const std::string& worker_id);
    
private:
    void run_heartbeat_monitor();
    void run_task_scheduler();
    void cleanup_completed_jobs();
    
    int port_;
    std::atomic<bool> running_;
    
    // Redis client for persistent storage
    RedisClient redis_client_;
    
    // Job and task management
    mutable std::mutex jobs_mutex_;
    std::map<std::string, JobConfig> jobs_;
    std::map<std::string, Task> tasks_;
    std::queue<std::string> pending_tasks_;
    
    // Worker management
    mutable std::mutex workers_mutex_;
    std::map<std::string, WorkerInfo> workers_;
    
    // Background threads
    std::thread heartbeat_thread_;
    std::thread scheduler_thread_;
    std::condition_variable scheduler_cv_;
    
    Logger logger_;
};

} // namespace daf

using namespace daf;

Coordinator::Coordinator(int port) 
    : port_(port), running_(false) {
}

Coordinator::~Coordinator() {
    stop();
}

bool Coordinator::start() {
    if (running_.load()) {
        return true;
    }
    
    logger_.info("Starting DAF Coordinator on port " + std::to_string(port_));
    
    // Initialize Redis connection
    std::string redis_host = Utils::getenv_or_default("DAF_REDIS_HOST", "localhost");
    int redis_port = std::stoi(Utils::getenv_or_default("DAF_REDIS_PORT", "6379"));
    
    if (!redis_client_.Connect(redis_host, redis_port)) {
        logger_.warning("Failed to connect to Redis at " + redis_host + ":" + std::to_string(redis_port));
        logger_.warning("Continuing without Redis - using in-memory storage only");
    } else {
        logger_.info("Connected to Redis backend for persistent storage");
    }
    
    // Check if port is available
    if (!Utils::is_port_available(port_)) {
        logger_.error("Port " + std::to_string(port_) + " is already in use");
        return false;
    }
    
    running_.store(true);
    
    // Start background threads
    heartbeat_thread_ = std::thread(&Coordinator::run_heartbeat_monitor, this);
    scheduler_thread_ = std::thread(&Coordinator::run_task_scheduler, this);
    
    logger_.info("DAF Coordinator started successfully");
    return true;
}

void Coordinator::stop() {
    if (!running_.load()) {
        return;
    }
    
    logger_.info("Stopping DAF Coordinator...");
    running_.store(false);
    
    // Wake up scheduler thread
    scheduler_cv_.notify_all();
    
    // Wait for threads to finish
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    
    logger_.info("DAF Coordinator stopped");
}

bool Coordinator::is_running() const {
    return running_.load();
}

ErrorCode Coordinator::submit_job(const JobConfig& config) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    
    logger_.info("Submitting job: " + config.job_id);
    
    // Store job configuration
    jobs_[config.job_id] = config;
    
    // Create map tasks
    for (int i = 0; i < config.num_map_tasks; ++i) {
        Task task;
        task.id = config.job_id + "_map_" + std::to_string(i);
        task.type = TaskType::MAP;
        task.status = TaskStatus::PENDING;
        task.plugin_name = config.plugin_name;
        task.parameters = config.parameters;
        task.created_time = Utils::get_timestamp_ms();
        
        // Assign input files (simple round-robin distribution)
        if (i < config.input_files.size()) {
            task.input_files.push_back(config.input_files[i]);
        }
        
        tasks_[task.id] = task;
        pending_tasks_.push(task.id);
    }
    
    // Wake up scheduler
    scheduler_cv_.notify_one();
    
    logger_.info("Job submitted with " + std::to_string(config.num_map_tasks) + " map tasks");
    return ErrorCode::SUCCESS;
}

std::vector<Task> Coordinator::get_tasks(const std::string& job_id) const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    std::vector<Task> job_tasks;
    
    for (const auto& [task_id, task] : tasks_) {
        if (task_id.find(job_id) == 0) {
            job_tasks.push_back(task);
        }
    }
    
    return job_tasks;
}

TaskStatus Coordinator::get_task_status(const std::string& task_id) const {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    auto it = tasks_.find(task_id);
    if (it != tasks_.end()) {
        return it->second.status;
    }
    return TaskStatus::FAILED;
}

ErrorCode Coordinator::register_worker(const WorkerInfo& worker) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    logger_.info("Registering worker: " + worker.id + " at " + worker.host + ":" + std::to_string(worker.port));
    
    workers_[worker.id] = worker;
    return ErrorCode::SUCCESS;
}

ErrorCode Coordinator::unregister_worker(const std::string& worker_id) {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    
    auto it = workers_.find(worker_id);
    if (it != workers_.end()) {
        logger_.info("Unregistering worker: " + worker_id);
        workers_.erase(it);
        return ErrorCode::SUCCESS;
    }
    
    return ErrorCode::INVALID_ARGUMENT;
}

std::vector<WorkerInfo> Coordinator::get_workers() const {
    std::lock_guard<std::mutex> lock(workers_mutex_);
    std::vector<WorkerInfo> worker_list;
    
    for (const auto& [worker_id, worker] : workers_) {
        worker_list.push_back(worker);
    }
    
    return worker_list;
}

void Coordinator::schedule_tasks() {
    std::unique_lock<std::mutex> jobs_lock(jobs_mutex_);
    std::unique_lock<std::mutex> workers_lock(workers_mutex_);
    
    while (!pending_tasks_.empty() && !workers_.empty()) {
        // Find available worker
        WorkerInfo* available_worker = nullptr;
        for (auto& [worker_id, worker] : workers_) {
            if (worker.is_available) {
                available_worker = &worker;
                break;
            }
        }
        
        if (!available_worker) {
            break; // No available workers
        }
        
        // Get next pending task
        std::string task_id = pending_tasks_.front();
        pending_tasks_.pop();
        
        auto task_it = tasks_.find(task_id);
        if (task_it == tasks_.end()) {
            continue; // Task not found
        }
        
        // Assign task to worker
        task_it->second.status = TaskStatus::RUNNING;
        task_it->second.started_time = Utils::get_timestamp_ms();
        available_worker->is_available = false;
        
        logger_.info("Assigned task " + task_id + " to worker " + available_worker->id);
    }
}

ErrorCode Coordinator::assign_task(const std::string& task_id, const std::string& worker_id) {
    // This would implement actual task assignment to worker via network communication
    // For now, just mark as assigned
    logger_.info("Task " + task_id + " assigned to worker " + worker_id);
    return ErrorCode::SUCCESS;
}

void Coordinator::run_heartbeat_monitor() {
    logger_.info("Heartbeat monitor started");
    
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        std::lock_guard<std::mutex> lock(workers_mutex_);
        auto current_time = Utils::get_timestamp_ms();
        
        for (auto it = workers_.begin(); it != workers_.end();) {
            if (current_time - it->second.last_heartbeat > 30000) { // 30 seconds timeout
                logger_.warning("Worker " + it->first + " heartbeat timeout, removing");
                it = workers_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    logger_.info("Heartbeat monitor stopped");
}

void Coordinator::run_task_scheduler() {
    logger_.info("Task scheduler started");
    
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(jobs_mutex_);
        scheduler_cv_.wait_for(lock, std::chrono::seconds(5), [this] {
            return !running_.load() || !pending_tasks_.empty();
        });
        
        if (!running_.load()) {
            break;
        }
        
        lock.unlock();
        schedule_tasks();
    }
    
    logger_.info("Task scheduler stopped");
}

void Coordinator::cleanup_completed_jobs() {
    // Clean up old completed jobs to save memory
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    auto current_time = Utils::get_timestamp_ms();
    
    for (auto it = tasks_.begin(); it != tasks_.end();) {
        if ((it->second.status == TaskStatus::COMPLETED || it->second.status == TaskStatus::FAILED) &&
            current_time - it->second.completed_time > 3600000) { // 1 hour
            it = tasks_.erase(it);
        } else {
            ++it;
        }
    }
}

int main(int argc, char* argv[]) {
    int port = 50051;
    
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    Logger::set_level(Logger::Level::INFO);
    Logger::info("Starting DAF Coordinator...");
    
    Coordinator coordinator(port);
    
    if (!coordinator.start()) {
        Logger::error("Failed to start coordinator");
        return 1;
    }
    
    // Simple demonstration - submit a test job
    JobConfig test_job;
    test_job.job_id = "test_job_001";
    test_job.plugin_name = "nerf_avatar_plugin";
    test_job.input_files = {"input1.dat", "input2.dat", "input3.dat"};
    test_job.output_directory = "output/";
    test_job.num_map_tasks = 3;
    test_job.num_reduce_tasks = 1;
    test_job.parameters["resolution"] = "512";
    test_job.parameters["samples"] = "64";
    
    coordinator.submit_job(test_job);
    
    // Run for demonstration
    Logger::info("Coordinator running... Press Ctrl+C to stop");
    
    // Keep running until interrupted
    while (coordinator.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Print status every 10 seconds
        static int counter = 0;
        if (++counter % 10 == 0) {
            auto workers = coordinator.get_workers();
            auto tasks = coordinator.get_tasks("test_job_001");
            Logger::info("Status: " + std::to_string(workers.size()) + " workers, " + 
                        std::to_string(tasks.size()) + " tasks");
        }
    }
    
    return 0;
}