#include "checkpoint_manager.h"
#include <iostream>

template<typename KeyType>
CheckpointManager<KeyType>::CheckpointManager(WALManager<KeyType>* wal, PageCache<KeyType>* cache, 
                                              JobScheduler* scheduler,
                                              std::chrono::milliseconds interval,
                                              size_t wal_threshold, size_t dirty_threshold)
    : wal_manager(wal), page_cache(cache), job_scheduler(scheduler),
      checkpoint_interval(interval), wal_size_threshold(wal_threshold), 
      dirty_page_threshold(dirty_threshold),
      last_checkpoint_lsn(0), checkpoints_completed(0), checkpoints_failed(0),
      checkpoint_job_name("checkpoint_recurring"), cleanup_job_name("cleanup_recurring") {
    
    last_checkpoint_time.store(std::chrono::steady_clock::now());
    
    std::cout << "CheckpointManager: Initialized with " << interval.count() 
              << "ms interval, WAL threshold: " << wal_threshold 
              << " bytes, dirty page threshold: " << dirty_threshold << std::endl;
}

template<typename KeyType>
CheckpointManager<KeyType>::~CheckpointManager() {
    stop();
}

template<typename KeyType>
void CheckpointManager<KeyType>::start() {
    if (!job_scheduler || !job_scheduler->isRunning()) {
        std::cerr << "CheckpointManager: Job scheduler not running" << std::endl;
        return;
    }
    
    // Add recurring checkpoint job
    job_scheduler->addRecurringJob(
        checkpoint_job_name,
        checkpoint_interval,
        [this]() { return this->checkpointJobFunc(); },
        "Automatic Checkpoint",
        JobPriority::HIGH
    );
    
    // Add recurring cleanup job (run less frequently)
    job_scheduler->addRecurringJob(
        cleanup_job_name,
        checkpoint_interval * 4, // Run cleanup 4x less frequently
        [this]() { return this->cleanupJobFunc(); },
        "WAL Cleanup",
        JobPriority::NORMAL
    );
    
    std::cout << "CheckpointManager: Started with recurring jobs" << std::endl;
}

template<typename KeyType>
void CheckpointManager<KeyType>::stop() {
    if (job_scheduler) {
        job_scheduler->removeRecurringJob(checkpoint_job_name);
        job_scheduler->removeRecurringJob(cleanup_job_name);
    }
    
    std::cout << "CheckpointManager: Stopped" << std::endl;
}

template<typename KeyType>
bool CheckpointManager<KeyType>::performCheckpoint() {
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "CheckpointManager: Starting checkpoint..." << std::endl;
    
    try {
        // Step 1: Flush all dirty pages from cache
        page_cache->flushAll();
        
        // Step 2: Write checkpoint record to WAL
        uint64_t checkpoint_lsn = wal_manager->writeCheckpoint();
        
        // Step 3: Ensure WAL is synced to disk
        wal_manager->sync();
        
        // Step 4: Update checkpoint tracking
        last_checkpoint_lsn.store(checkpoint_lsn);
        last_checkpoint_time.store(std::chrono::steady_clock::now());
        checkpoints_completed.fetch_add(1);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "CheckpointManager: Checkpoint completed successfully (LSN: " 
                  << checkpoint_lsn << ", duration: " << duration.count() << "ms)" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        checkpoints_failed.fetch_add(1);
        std::cerr << "CheckpointManager: Checkpoint failed: " << e.what() << std::endl;
        return false;
    }
}

template<typename KeyType>
bool CheckpointManager<KeyType>::shouldCheckpoint() const {
    // Check time-based trigger
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = now - last_checkpoint_time.load();
    if (time_since_last >= checkpoint_interval) {
        return true;
    }
    
    // Check WAL size trigger
    size_t current_wal_size = wal_manager->getWALSize();
    if (current_wal_size >= wal_size_threshold) {
        std::cout << "CheckpointManager: WAL size (" << current_wal_size 
                  << " bytes) exceeds threshold (" << wal_size_threshold << " bytes)" << std::endl;
        return true;
    }
    
    // Could add dirty page count trigger here if we tracked it
    // but now, rely on time and WAL size triggers
    
    return false;
}

template<typename KeyType>
void CheckpointManager<KeyType>::scheduleCheckpointIfNeeded() {
    if (shouldCheckpoint()) {
        job_scheduler->scheduleCheckpoint([this]() { 
            return this->performCheckpoint(); 
        });
    }
}

template<typename KeyType>
bool CheckpointManager<KeyType>::checkpointJobFunc() {
    // This is called by the job scheduler for recurring checkpoints
    if (shouldCheckpoint()) {
        return performCheckpoint();
    }
    
    // No checkpoint needed, but job succeeded
    return true;
}

template<typename KeyType>
bool CheckpointManager<KeyType>::cleanupJobFunc() {
    std::cout << "CheckpointManager: Running WAL cleanup..." << std::endl;
    
    try {
        // Get the last successful checkpoint LSN
        uint64_t checkpoint_lsn = last_checkpoint_lsn.load();
        
        if (checkpoint_lsn > 0) {
            // Truncate WAL up to the checkpoint (keeping some buffer)
            uint64_t truncate_lsn = checkpoint_lsn > 100 ? checkpoint_lsn - 100 : 0;
            
            if (truncate_lsn > 0) {
                wal_manager->truncate(truncate_lsn);
                std::cout << "CheckpointManager: Truncated WAL up to LSN " << truncate_lsn << std::endl;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "CheckpointManager: Cleanup failed: " << e.what() << std::endl;
        return false;
    }
}

template<typename KeyType>
void CheckpointManager<KeyType>::setCheckpointInterval(std::chrono::milliseconds interval) {
    checkpoint_interval = interval;
    
    // Update recurring job if it exists
    if (job_scheduler) {
        job_scheduler->removeRecurringJob(checkpoint_job_name);
        job_scheduler->addRecurringJob(
            checkpoint_job_name,
            interval,
            [this]() { return this->checkpointJobFunc(); },
            "Automatic Checkpoint",
            JobPriority::HIGH
        );
    }
}

template<typename KeyType>
void CheckpointManager<KeyType>::setWALSizeThreshold(size_t threshold) {
    wal_size_threshold = threshold;
    std::cout << "CheckpointManager: Updated WAL size threshold to " << threshold << " bytes" << std::endl;
}

template<typename KeyType>
void CheckpointManager<KeyType>::setDirtyPageThreshold(size_t threshold) {
    dirty_page_threshold = threshold;
    std::cout << "CheckpointManager: Updated dirty page threshold to " << threshold << " pages" << std::endl;
}

template<typename KeyType>
typename CheckpointManager<KeyType>::CheckpointStats CheckpointManager<KeyType>::getStats() const {
    size_t total = checkpoints_completed.load() + checkpoints_failed.load();
    double success_rate = total > 0 ? (double)checkpoints_completed.load() / total * 100.0 : 100.0;
    
    return {
        checkpoints_completed.load(),
        checkpoints_failed.load(),
        success_rate,
        last_checkpoint_lsn.load(),
        last_checkpoint_time.load(),
        wal_manager->getWALSize(),
        success_rate >= 99.0 // Consider healthy if 99%+ success rate
    };
}

template<typename KeyType>
void CheckpointManager<KeyType>::printStats() const {
    auto stats = getStats();
    
    auto time_since_last = std::chrono::steady_clock::now() - stats.last_checkpoint_time;
    auto minutes_since = std::chrono::duration_cast<std::chrono::minutes>(time_since_last).count();
    
    std::cout << "\n=== Checkpoint Manager Statistics ===" << std::endl;
    std::cout << "Total checkpoints: " << stats.total_checkpoints << std::endl;
    std::cout << "Failed checkpoints: " << stats.failed_checkpoints << std::endl;
    std::cout << "Success rate: " << stats.success_rate << "%" << std::endl;
    std::cout << "Last checkpoint LSN: " << stats.last_checkpoint_lsn << std::endl;
    std::cout << "Minutes since last checkpoint: " << minutes_since << std::endl;
    std::cout << "Current WAL size: " << stats.current_wal_size << " bytes" << std::endl;
    std::cout << "Health status: " << (stats.is_healthy ? "HEALTHY" : "UNHEALTHY") << std::endl;
    std::cout << "=====================================" << std::endl;
}

template class CheckpointManager<int>;
template class CheckpointManager<std::string>;
