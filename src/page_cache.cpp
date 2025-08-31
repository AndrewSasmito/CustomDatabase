#include "page_cache.h"
#include <iostream>

template <typename KeyType>
PageCache<KeyType>::PageCache(ContentStorage<KeyType>* storage, size_t max_size) 
    : content_storage(storage), max_cache_size(max_size) {
    if (!storage) { // Just include descriptive error messages
        throw std::invalid_argument("ContentStorage cannot be null");
    }
}

template <typename KeyType>
PageCache<KeyType>::~PageCache() {
    flushAll();
}

template <typename KeyType>
void PageCache<KeyType>::updateLRU(uint16_t page_id) {
    auto it = lru_iterators.find(page_id);
    if (it != lru_iterators.end()) {
        lru_order.erase(it->second);
    }
    
    lru_order.push_front(page_id);
    lru_iterators[page_id] = lru_order.begin();
}

template <typename KeyType>
void PageCache<KeyType>::evictLRU() {
    if (lru_order.empty()) return;
    
    uint16_t lru_page_id = lru_order.back();
    
    auto cache_it = cache.find(lru_page_id);
    if (cache_it != cache.end() && cache_it->second.is_dirty) {
        // Write back to content storage
        content_storage->storePage(*(cache_it->second.page));
        std::cout << "Cache: Writing back dirty page " << lru_page_id << " during eviction" << std::endl;
    }
    
    // Remove from cache and LRU tracking
    cache.erase(lru_page_id);
    lru_order.pop_back();
    lru_iterators.erase(lru_page_id);
}

template <typename KeyType>
void PageCache<KeyType>::evictIfNeeded() {
    while (cache.size() >= max_cache_size) {
        evictLRU();
    }
}

template <typename KeyType>
std::shared_ptr<Page<KeyType>> PageCache<KeyType>::getPage(uint16_t page_id) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    // Check if page is in cache
    auto it = cache.find(page_id);
    if (it != cache.end()) {
        // Update LRU since cache got hit
        updateLRU(page_id);
        it->second.last_accessed = std::chrono::steady_clock::now();
        return it->second.page;
    }
    
    // Cache miss so load from content storage
    auto page = content_storage->getPage(page_id);
    if (!page) {
        return nullptr;
    }
    
    // Add to cache
    evictIfNeeded();
    cache.emplace(page_id, CachedPage<KeyType>(page, false));
    updateLRU(page_id);
    
    std::cout << "Cache: Loaded page " << page_id << " from storage" << std::endl;
    return page;
}

template <typename KeyType>
void PageCache<KeyType>::putPage(uint16_t page_id, std::shared_ptr<Page<KeyType>> page) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    evictIfNeeded();
    
    // Add or update page in cache
    auto it = cache.find(page_id);
    if (it != cache.end()) {
        // Update existing entry
        it->second.page = page;
        it->second.is_dirty = true;
        it->second.last_accessed = std::chrono::steady_clock::now();
    } else {
        // Add new entry
        cache.emplace(page_id, CachedPage<KeyType>(page, true));
    }
    
    updateLRU(page_id);
    std::cout << "Cache: Stored page " << page_id << " (marked as dirty)" << std::endl;
}

template <typename KeyType>
void PageCache<KeyType>::markDirty(uint16_t page_id) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    auto it = cache.find(page_id);
    if (it != cache.end()) {
        it->second.is_dirty = true;
        updateLRU(page_id);
        std::cout << "Cache: Marked page " << page_id << " as dirty" << std::endl;
    }
}

template <typename KeyType>
std::vector<std::pair<uint16_t, std::shared_ptr<Page<KeyType>>>> PageCache<KeyType>::getDirtyPages() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    std::vector<std::pair<uint16_t, std::shared_ptr<Page<KeyType>>>> dirty_pages;
    
    for (const auto& entry : cache) {
        if (entry.second.is_dirty) {
            dirty_pages.emplace_back(entry.first, entry.second.page);
        }
    }
    
    return dirty_pages;
}

template <typename KeyType>
void PageCache<KeyType>::clearDirtyFlag(uint16_t page_id) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    auto it = cache.find(page_id);
    if (it != cache.end()) {
        it->second.is_dirty = false;
    }
}

template <typename KeyType>
void PageCache<KeyType>::flushAll() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    std::cout << "Cache: Flushing all dirty pages" << std::endl;
    size_t flushed = 0;
    
    for (auto& entry : cache) {
        if (entry.second.is_dirty) {
            content_storage->storePage(*(entry.second.page));
            entry.second.is_dirty = false;
            flushed++;
        }
    }
    
    std::cout << "Cache: Flushed " << flushed << " dirty pages" << std::endl;
}



template class PageCache<int>;
template class PageCache<std::string>;
