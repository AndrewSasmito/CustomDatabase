#pragma once
#include <unordered_map>
#include <list>
#include <memory>
#include <mutex>
#include <chrono>
#include "page_manager.h"
#include "content_storage.h"

// with metadata
template <typename KeyType>
struct CachedPage {
    std::shared_ptr<Page<KeyType>> page;
    bool is_dirty;
    std::chrono::steady_clock::time_point last_accessed;
    
    CachedPage(std::shared_ptr<Page<KeyType>> p, bool dirty = false) 
        : page(p), is_dirty(dirty), last_accessed(std::chrono::steady_clock::now()) {}
};

template <typename KeyType>
class PageCache {
private:
    // Core cache storage
    std::unordered_map<uint16_t, CachedPage<KeyType>> cache;
    
    // LRU tracking
    std::list<uint16_t> lru_order;
    std::unordered_map<uint16_t, typename std::list<uint16_t>::iterator> lru_iterators;
    
    // For thread safety
    mutable std::mutex cache_mutex;
    
    size_t max_cache_size;
    
    // Reference to content storage
    ContentStorage<KeyType>* content_storage;
    
    void updateLRU(uint16_t page_id);
    void evictLRU();
    void evictIfNeeded();
    
public:
    PageCache(ContentStorage<KeyType>* storage, size_t max_size = 100);
    ~PageCache();
    
    // These are the main cache operations
    std::shared_ptr<Page<KeyType>> getPage(uint16_t page_id);
    void putPage(uint16_t page_id, std::shared_ptr<Page<KeyType>> page);
    void markDirty(uint16_t page_id);
    
    // Cache management
    std::vector<std::pair<uint16_t, std::shared_ptr<Page<KeyType>>>> getDirtyPages();
    void clearDirtyFlag(uint16_t page_id);
    void flushAll();
};
