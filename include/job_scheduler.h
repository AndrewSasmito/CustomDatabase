#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

enum class JobType {
    CHECKPOINT,
    VERSION_PRUNE,
    HEALTH_CHECK,
    CUSTOM
};

enum class JobStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED
};

enum class JobPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// Base job class
class Job {
public:
    uint64_t job_id;
    JobType type;
    JobPriority priority;
    JobStatus status;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point scheduled_at;
    std::chrono::milliseconds timeout;
    std::function<bool()> execute_func;
    std::string description;
    
    Job(uint64_t id, JobType t, JobPriority p, std::function<bool()> func, 
        const std::string& desc, std::chrono::milliseconds to = std::chrono::minutes(5))
        : job_id(id), type(t), priority(p), status(JobStatus::PENDING),
          created_at(std::chrono::steady_clock::now()),
          scheduled_at(std::chrono::steady_clock::now()),
          timeout(to), execute_func(func), description(desc) {}
          
    // For priority queue ordering
    bool operator<(const Job& other) const {
        if (priority != other.priority) {
            return priority < other.priority;
        }
        return scheduled_at > other.scheduled_at;
    }
};

// Job scheduler class
class JobScheduler {
private:
    // Thread pool
    std::vector<std::thread> worker_threads;
    size_t num_workers;
    std::atomic<bool> running;
    
    // Job queue with priority
    std::priority_queue<std::shared_ptr<Job>> job_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::condition_variable shutdown_cv;
    
    // Job tracking
    std::unordered_map<uint64_t, std::shared_ptr<Job>> active_jobs;
    std::unordered_map<uint64_t, std::shared_ptr<Job>> completed_jobs;
    mutable std::mutex jobs_mutex;
    
    // Job ID generation
    std::atomic<uint64_t> next_job_id;
    
    // Health monitoring
    std::atomic<size_t> total_jobs_executed;
    std::atomic<size_t> failed_jobs;
    std::atomic<size_t> successful_jobs;
    std::chrono::steady_clock::time_point last_health_check;
    
    // Recurring job management
    struct RecurringJobInfo {
        std::chrono::milliseconds interval;
        std::chrono::steady_clock::time_point next_execution;
        std::function<bool()> job_func;
        std::string description;
        JobPriority priority;
        bool enabled;
    };
    std::unordered_map<std::string, RecurringJobInfo> recurring_jobs;
    std::mutex recurring_jobs_mutex;
    
    // Worker thread functions
    void workerThread(int worker_id);
    void schedulerThread();
    
    // Job execution
    bool executeJob(std::shared_ptr<Job> job);
    void handleJobTimeout(std::shared_ptr<Job> job);
    
    // Recurring job management
    void scheduleRecurringJobs();
    
public:
    JobScheduler(size_t num_threads = 4);
    ~JobScheduler();
    
    // Lifecycle
    void start();
    void stop();
    bool isRunning() const { return running.load(); }
    
    // Job submission
    uint64_t scheduleJob(JobType type, JobPriority priority, 
                        std::function<bool()> job_func, const std::string& description,
                        std::chrono::milliseconds delay = std::chrono::milliseconds(0),
                        std::chrono::milliseconds timeout = std::chrono::minutes(5));
    
    uint64_t scheduleCheckpoint(std::function<bool()> checkpoint_func, 
                               std::chrono::milliseconds delay = std::chrono::milliseconds(0));
    
    uint64_t scheduleVersionPrune(std::function<bool()> prune_func,
                                 std::chrono::milliseconds delay = std::chrono::milliseconds(0));
    
    // Recurring jobs
    bool addRecurringJob(const std::string& name, std::chrono::milliseconds interval,
                        std::function<bool()> job_func, const std::string& description,
                        JobPriority priority = JobPriority::NORMAL);
    
    bool removeRecurringJob(const std::string& name);
    bool enableRecurringJob(const std::string& name, bool enabled);
    
    // Job management
    bool cancelJob(uint64_t job_id);
    JobStatus getJobStatus(uint64_t job_id);
    std::shared_ptr<Job> getJob(uint64_t job_id);
    
    // Health and monitoring
    struct SchedulerStats {
        size_t pending_jobs;
        size_t active_jobs;
        size_t total_executed;
        size_t successful;
        size_t failed;
        double success_rate;
        size_t worker_threads;
        bool is_healthy;
    };
    
    SchedulerStats getStats() const;
    void printStats() const;
    bool isHealthy() const;
    
    // Maintenance
    void cleanupCompletedJobs(std::chrono::hours max_age = std::chrono::hours(24));
};
