#pragma once
#include <chrono>
#include <atomic>
#include <string>
#include <memory>
#include "wal.h"
#include "page_cache.h"
#include "job_scheduler.h"

template<typename KeyType>
class CheckpointManager {
private:
    WALManager<KeyType>* wal_manager;
    PageCache<KeyType>* page_cache;
    JobScheduler* job_scheduler;
    
    // Checkpoint configuration
    std::chrono::milliseconds checkpoint_interval;
    size_t wal_size_threshold;  // Trigger checkpoint when WAL exceeds this size
    size_t dirty_page_threshold; // Trigger checkpoint when dirty pages exceed this
    
    // Checkpoint tracking
    std::atomic<uint64_t> last_checkpoint_lsn;
    std::atomic<std::chrono::steady_clock::time_point> last_checkpoint_time;
    std::atomic<size_t> checkpoints_completed;
    std::atomic<size_t> checkpoints_failed;
    
    // Job IDs for recurring jobs
    std::string checkpoint_job_name;
    std::string cleanup_job_name;
    
public:
    CheckpointManager(WALManager<KeyType>* wal, PageCache<KeyType>* cache, 
                     JobScheduler* scheduler,
                     std::chrono::milliseconds interval = std::chrono::minutes(5),
                     size_t wal_threshold = 1024 * 1024,  // 1MB
                     size_t dirty_threshold = 100);       // 100 pages
    
    ~CheckpointManager();
    
    // Lifecycle
    void start();
    void stop();
    
    // Manual checkpoint
    bool performCheckpoint();
    
    // Automatic checkpoint triggers
    bool shouldCheckpoint() const;
    void scheduleCheckpointIfNeeded();
    
    // Configuration
    void setCheckpointInterval(std::chrono::milliseconds interval);
    void setWALSizeThreshold(size_t threshold);
    void setDirtyPageThreshold(size_t threshold);
    
    // Statistics
    struct CheckpointStats {
        size_t total_checkpoints;
        size_t failed_checkpoints;
        double success_rate;
        uint64_t last_checkpoint_lsn;
        std::chrono::steady_clock::time_point last_checkpoint_time;
        size_t current_wal_size;
        bool is_healthy;
    };
    
    CheckpointStats getStats() const;
    void printStats() const;
    
private:
    // Job functions for scheduler
    bool checkpointJobFunc();
    bool cleanupJobFunc();
};
