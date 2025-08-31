#pragma once
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>
#include "page_manager.h"
#include "content_storage.h"
#include "page_cache.h"

template <typename KeyType>
struct WriteRequest {
    uint16_t page_id;
    std::shared_ptr<Page<KeyType>> page;
    std::chrono::steady_clock::time_point timestamp;
    
    WriteRequest(uint16_t id, std::shared_ptr<Page<KeyType>> p) 
        : page_id(id), page(p), timestamp(std::chrono::steady_clock::now()) {}
};

template <typename KeyType>
class WriterQueue {
private:
    // Queue
    std::queue<WriteRequest<KeyType>> write_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::condition_variable empty_cv;
    
    // Thread
    std::vector<std::thread> writer_threads;
    std::atomic<bool> running;
    size_t num_writer_threads;
    
    // References to other components
    ContentStorage<KeyType>* content_storage;
    PageCache<KeyType>* page_cache;
    
    // Configuration
    size_t max_queue_size;
    std::chrono::milliseconds batch_timeout;
    
    // Worker thread function
    void writerWorker(int worker_id);
    
    // Batch processing
    std::vector<WriteRequest<KeyType>> getBatch(size_t max_batch_size);
    void processBatch(const std::vector<WriteRequest<KeyType>>& batch, int worker_id);
    
public:
    WriterQueue(ContentStorage<KeyType>* storage, PageCache<KeyType>* cache, 
                size_t num_threads = 2, size_t max_queue = 1000);
    ~WriterQueue();
    
    // Queue operations
    bool enqueueWrite(uint16_t page_id, std::shared_ptr<Page<KeyType>> page);
    void start();
    void stop();
    void waitForEmpty();
};
