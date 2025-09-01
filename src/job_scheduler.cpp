#include "job_scheduler.h"
#include <iostream>
#include <algorithm>

JobScheduler::JobScheduler(size_t num_threads) 
    : num_workers(num_threads), running(false), next_job_id(1),
      total_jobs_executed(0), failed_jobs(0), successful_jobs(0),
      last_health_check(std::chrono::steady_clock::now()) {
    
    std::cout << "JobScheduler: Initialized with " << num_workers << " worker threads" << std::endl;
}

JobScheduler::~JobScheduler() {
    stop();
}

void JobScheduler::start() {
    if (running.load()) {
        return;
    }
    
    running.store(true);
    
    // Start the worker threads
    for (size_t i = 0; i < num_workers; ++i) {
        worker_threads.emplace_back(&JobScheduler::workerThread, this, i);
    }
    
    worker_threads.emplace_back(&JobScheduler::schedulerThread, this);
    
    std::cout << "JobScheduler: Started with " << num_workers << " workers + 1 scheduler thread" << std::endl;
}

void JobScheduler::stop() {
    if (!running.load()) {
        return; // Already stopped
    }
    
    std::cout << "JobScheduler: Stopping" << std::endl;
    
    // Signal all threads to stop
    running.store(false);
    queue_cv.notify_all();
    
    // Wait for all threads to finish
    for (auto& thread : worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads.clear();
    std::cout << "JobScheduler: All threads stopped" << std::endl;
}

uint64_t JobScheduler::scheduleJob(JobType type, JobPriority priority, 
                                  std::function<bool()> job_func, const std::string& description,
                                  std::chrono::milliseconds delay, std::chrono::milliseconds timeout) {
    uint64_t job_id = next_job_id.fetch_add(1);
    
    auto job = std::make_shared<Job>(job_id, type, priority, job_func, description, timeout);
    job->scheduled_at = std::chrono::steady_clock::now() + delay;
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        job_queue.push(job);
    }
    
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        active_jobs[job_id] = job;
    }
    
    queue_cv.notify_one();
    
    std::cout << "JobScheduler: Scheduled " << description << " (ID: " << job_id << ")" << std::endl;
    return job_id;
}

uint64_t JobScheduler::scheduleCheckpoint(std::function<bool()> checkpoint_func, 
                                          std::chrono::milliseconds delay) {
    return scheduleJob(JobType::CHECKPOINT, JobPriority::HIGH, checkpoint_func,
                      "WAL Checkpoint", delay, std::chrono::minutes(10));
}

uint64_t JobScheduler::scheduleVersionPrune(std::function<bool()> prune_func,
                                            std::chrono::milliseconds delay) {
    return scheduleJob(JobType::VERSION_PRUNE, JobPriority::NORMAL, prune_func,
                      "Version Pruning", delay, std::chrono::minutes(15));
}

bool JobScheduler::addRecurringJob(const std::string& name, std::chrono::milliseconds interval,
                                  std::function<bool()> job_func, const std::string& description,
                                  JobPriority priority) {
    std::lock_guard<std::mutex> lock(recurring_jobs_mutex);
    
    if (recurring_jobs.find(name) != recurring_jobs.end()) {
        std::cout << "JobScheduler: Recurring job '" << name << "' already exists" << std::endl;
        return false;
    }
    
    RecurringJobInfo info;
    info.interval = interval;
    info.next_execution = std::chrono::steady_clock::now() + interval;
    info.job_func = job_func;
    info.description = description;
    info.priority = priority;
    info.enabled = true;
    
    recurring_jobs[name] = info;
    
    std::cout << "JobScheduler: Added recurring job '" << name << "' with " 
              << interval.count() << "ms interval" << std::endl;
    return true;
}

bool JobScheduler::removeRecurringJob(const std::string& name) {
    std::lock_guard<std::mutex> lock(recurring_jobs_mutex);
    
    auto it = recurring_jobs.find(name);
    if (it == recurring_jobs.end()) {
        return false;
    }
    
    recurring_jobs.erase(it);
    std::cout << "JobScheduler: Removed recurring job '" << name << "'" << std::endl;
    return true;
}

bool JobScheduler::enableRecurringJob(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(recurring_jobs_mutex);
    
    auto it = recurring_jobs.find(name);
    if (it == recurring_jobs.end()) {
        return false;
    }
    
    it->second.enabled = enabled;
    std::cout << "JobScheduler: " << (enabled ? "Enabled" : "Disabled") 
              << " recurring job '" << name << "'" << std::endl;
    return true;
}

void JobScheduler::workerThread(int worker_id) {
    std::cout << "JobScheduler: Worker " << worker_id << " started" << std::endl;
    
    while (running.load()) {
        std::shared_ptr<Job> job;
        
        // Get next job from queue
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            // Wait for a job or shutdown signal
            queue_cv.wait(lock, [this] { 
                return !job_queue.empty() || !running.load(); 
            });
            
            if (!running.load() && job_queue.empty()) {
                break; // Shutdown
            }
            
            if (!job_queue.empty()) {
                job = job_queue.top();
                
                // Check if job is ready to run
                auto now = std::chrono::steady_clock::now();
                if (job->scheduled_at > now) {
                    // Not ready yet put it back and wait
                    continue;
                }
                
                job_queue.pop();
                job->status = JobStatus::RUNNING;
            }
        }
        
        if (job) {
            executeJob(job);
        }
    }
    
    std::cout << "JobScheduler: Worker " << worker_id << " finished" << std::endl;
}

void JobScheduler::schedulerThread() {
    std::cout << "JobScheduler: Scheduler thread started" << std::endl;
    
    while (running.load()) {
        scheduleRecurringJobs();
        
        // Sleep for a short interval
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "JobScheduler: Scheduler thread finished" << std::endl;
}

bool JobScheduler::executeJob(std::shared_ptr<Job> job) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "JobScheduler: Executing " << job->description 
              << " (ID: " << job->job_id << ")" << std::endl;
    
    bool success = false;
    try {
        success = job->execute_func();
        
        job->status = success ? JobStatus::COMPLETED : JobStatus::FAILED;
        
        if (success) {
            successful_jobs.fetch_add(1);
        } else {
            failed_jobs.fetch_add(1);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "JobScheduler: Job " << job->job_id 
                  << " threw exception: " << e.what() << std::endl;
        job->status = JobStatus::FAILED;
        failed_jobs.fetch_add(1);
    }
    
    total_jobs_executed.fetch_add(1);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "JobScheduler: " << (success ? "Completed" : "Failed") 
              << " " << job->description << " in " << duration.count() << "ms" << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(jobs_mutex);
        active_jobs.erase(job->job_id);
        completed_jobs[job->job_id] = job;
    }
    
    return success;
}

void JobScheduler::scheduleRecurringJobs() {
    std::lock_guard<std::mutex> lock(recurring_jobs_mutex);
    auto now = std::chrono::steady_clock::now();
    
    for (auto& [name, info] : recurring_jobs) {
        if (!info.enabled || now < info.next_execution) {
            continue;
        }
        
        // Schedule the recurring job
        scheduleJob(JobType::CUSTOM, info.priority, info.job_func, 
                   info.description + " (recurring)");
        
        info.next_execution = now + info.interval;
    }
}

JobStatus JobScheduler::getJobStatus(uint64_t job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    
    auto active_it = active_jobs.find(job_id);
    if (active_it != active_jobs.end()) {
        return active_it->second->status;
    }
    
    auto completed_it = completed_jobs.find(job_id);
    if (completed_it != completed_jobs.end()) {
        return completed_it->second->status;
    }
    
    return JobStatus::CANCELLED; // Job not found
}

std::shared_ptr<Job> JobScheduler::getJob(uint64_t job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    
    auto active_it = active_jobs.find(job_id);
    if (active_it != active_jobs.end()) {
        return active_it->second;
    }
    
    auto completed_it = completed_jobs.find(job_id);
    if (completed_it != completed_jobs.end()) {
        return completed_it->second;
    }
    
    return nullptr;
}

JobScheduler::SchedulerStats JobScheduler::getStats() const {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    
    size_t pending = job_queue.size();
    size_t active = active_jobs.size();
    size_t total = total_jobs_executed.load();
    size_t successful = successful_jobs.load();
    size_t failed = failed_jobs.load();
    
    double success_rate = total > 0 ? (double)successful / total * 100.0 : 0.0;
    bool healthy = success_rate >= 99.98; // 99.98% uptime target
    
    return {pending, active, total, successful, failed, success_rate, num_workers, healthy};
}

void JobScheduler::printStats() const {
    auto stats = getStats();
    
    std::cout << "\n=== Job Scheduler Statistics ===" << std::endl;
    std::cout << "Pending jobs: " << stats.pending_jobs << std::endl;
    std::cout << "Active jobs: " << stats.active_jobs << std::endl;
    std::cout << "Total executed: " << stats.total_executed << std::endl;
    std::cout << "Successful: " << stats.successful << std::endl;
    std::cout << "Failed: " << stats.failed << std::endl;
    std::cout << "Success rate: " << stats.success_rate << "%" << std::endl;
    std::cout << "Worker threads: " << stats.worker_threads << std::endl;
    std::cout << "Health status: " << (stats.is_healthy ? "HEALTHY" : "UNHEALTHY") << std::endl;
    std::cout << "================================" << std::endl;
}

bool JobScheduler::isHealthy() const {
    auto stats = getStats();
    return stats.is_healthy;
}

void JobScheduler::cleanupCompletedJobs(std::chrono::hours max_age) {
    std::lock_guard<std::mutex> lock(jobs_mutex);
    auto cutoff = std::chrono::steady_clock::now() - max_age;
    
    auto it = completed_jobs.begin();
    size_t cleaned = 0;
    
    while (it != completed_jobs.end()) {
        if (it->second->created_at < cutoff) {
            it = completed_jobs.erase(it);
            cleaned++;
        } else {
            ++it;
        }
    }
    
    if (cleaned > 0) {
        std::cout << "JobScheduler: Cleaned up " << cleaned << " old completed jobs" << std::endl;
    }
}
