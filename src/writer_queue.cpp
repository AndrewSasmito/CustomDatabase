#include "writer_queue.h"
#include <iostream>
#include <chrono>

template <typename KeyType>
WriterQueue<KeyType>::WriterQueue(ContentStorage<KeyType>* storage, PageCache<KeyType>* cache, 
                                  size_t num_threads, size_t max_queue)
    : content_storage(storage), page_cache(cache), running(false), 
      num_writer_threads(num_threads), max_queue_size(max_queue),
      batch_timeout(std::chrono::milliseconds(10)) {
    
    if (!storage || !cache) {
        throw std::invalid_argument("ContentStorage and PageCache cannot be null");
    }
}

template <typename KeyType>
WriterQueue<KeyType>::~WriterQueue() {
    stop();
}

template <typename KeyType>
void WriterQueue<KeyType>::start() {
    if (running.load()) {
        return; // Already running
    }
    
    running.store(true);
    
    // Start writer threads
    for (size_t i = 0; i < num_writer_threads; ++i) {
        writer_threads.emplace_back(&WriterQueue::writerWorker, this, i);
    }
    
    std::cout << "WriterQueue: Started " << num_writer_threads << " writer threads" << std::endl;
}

template <typename KeyType>
void WriterQueue<KeyType>::stop() {
    if (!running.load()) {
        return; // Already stopped
    }
    
    std::cout << "WriterQueue: Stopping writer threads" << std::endl;
    
    // Signal threads to stop
    running.store(false);
    queue_cv.notify_all();
    
    // Wait for all threads to finish
    for (auto& thread : writer_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    writer_threads.clear();
    std::cout << "WriterQueue: All writer threads stopped" << std::endl;
}

template <typename KeyType>
bool WriterQueue<KeyType>::enqueueWrite(uint16_t page_id, std::shared_ptr<Page<KeyType>> page) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    
    // Check if queue is full
    if (write_queue.size() >= max_queue_size) {
        std::cout << "WriterQueue: Queue overflow, dropping write request for page " << page_id << std::endl;
        return false;
    }
    
    write_queue.emplace(page_id, page);
    
    // Notify writer thread
    queue_cv.notify_one();
    
    return true;
}

template <typename KeyType>
void WriterQueue<KeyType>::waitForEmpty() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    empty_cv.wait(lock, [this] { return write_queue.empty(); });
}

template <typename KeyType>
std::vector<WriteRequest<KeyType>> WriterQueue<KeyType>::getBatch(size_t max_batch_size) {
    std::vector<WriteRequest<KeyType>> batch;
    std::unique_lock<std::mutex> lock(queue_mutex);
    
    auto timeout = std::chrono::steady_clock::now() + batch_timeout;
    queue_cv.wait_until(lock, timeout, [this] { return !write_queue.empty() || !running.load(); });
    
    // Collect batch
    while (!write_queue.empty() && batch.size() < max_batch_size) {
        batch.push_back(std::move(write_queue.front()));
        write_queue.pop();
    }
    
    if (write_queue.empty()) {
        empty_cv.notify_all();
    }
    
    return batch;
}

template <typename KeyType>
void WriterQueue<KeyType>::processBatch(const std::vector<WriteRequest<KeyType>>& batch, int worker_id) {
    if (batch.empty()) return;
    
    std::cout << "WriterQueue: Worker " << worker_id << " processing batch of " << batch.size() << " pages" << std::endl;
    
    for (const auto& request : batch) {
        try {
            // Write to content storage (this is where deduplication happens yayy)
            uint16_t stored_page_id = content_storage->storePage(*(request.page));
            
            // Clear dirty flag in cache since weve written it
            page_cache->clearDirtyFlag(request.page_id);
            
        } catch (const std::exception& e) {
            std::cerr << "WriterQueue: Error writing page " << request.page_id << ": " << e.what() << std::endl;
        }
    }
}

template <typename KeyType>
void WriterQueue<KeyType>::writerWorker(int worker_id) {
    std::cout << "WriterQueue: Worker " << worker_id << " started" << std::endl;
    
    const size_t max_batch_size = 10;
    
    while (running.load() || !write_queue.empty()) {
        auto batch = getBatch(max_batch_size);
        
        if (!batch.empty()) {
            processBatch(batch, worker_id);
        }
        
        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    std::cout << "WriterQueue: Worker " << worker_id << " finished" << std::endl;
}



template class WriterQueue<int>;
template class WriterQueue<std::string>;
