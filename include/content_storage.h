#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include "page_manager.h"

template <typename KeyType>
class ContentStorage {
private:
    // Map content hash to actual page data
    std::unordered_map<std::string, std::shared_ptr<Page<KeyType>>> content_map;
    
    // Map page ID to content hash for reverse lookup
    std::unordered_map<uint16_t, std::string> page_to_hash;
    
    // Next available page ID
    uint16_t next_page_id = 1;

public:
    // Store a page and return its page ID
    uint16_t storePage(const Page<KeyType>& page) {
        // Update the page's content hash
        Page<KeyType> page_copy = page;
        page_copy.updateContentHash();
        std::string content_hash = page_copy.getContentHash();
        
        // Check if we already have this content
        auto it = content_map.find(content_hash);
        if (it != content_map.end()) {
            // Content already exists, return existing page ID
            std::cout << "Deduplication: Found existing content with hash " << content_hash 
                      << ", reusing page ID " << it->second->header.page_id << std::endl;
            return it->second->header.page_id;
        }
        
        // New content, assign a new page ID
        page_copy.header.page_id = next_page_id++;
        page_to_hash[page_copy.header.page_id] = content_hash;
        content_map[content_hash] = std::make_shared<Page<KeyType>>(page_copy);
        
        std::cout << "Stored new content with hash " << content_hash 
                  << " as page ID " << page_copy.header.page_id << std::endl;
        return page_copy.header.page_id;
    }
    
    // Retrieve a page by its page ID
    std::shared_ptr<Page<KeyType>> getPage(uint16_t page_id) {
        auto hash_it = page_to_hash.find(page_id);
        if (hash_it == page_to_hash.end()) {
            return nullptr; // Page not found
        }
        
        auto content_it = content_map.find(hash_it->second);
        if (content_it == content_map.end()) {
            return nullptr; // Content not found (shouldn't happen)
        }
        
        return content_it->second;
    }
    
    // Get statistics about storage usage
    void printStats() const {
        std::cout << "\n=== Content Storage Statistics ===" << std::endl;
        std::cout << "Total unique content blocks: " << content_map.size() << std::endl;
        std::cout << "Total page IDs assigned: " << page_to_hash.size() << std::endl;
        std::cout << "Next available page ID: " << next_page_id << std::endl;
        
        if (content_map.size() > 0) {
            size_t total_keys = 0;
            size_t total_data = 0;
            for (const auto& pair : content_map) {
                total_keys += pair.second->keys.size();
                total_data += pair.second->data.size();
            }
            std::cout << "Total keys stored: " << total_keys << std::endl;
            std::cout << "Total data bytes: " << total_data << std::endl;
        }
        std::cout << "===================================" << std::endl;
    }
    
    // Check if a page with given content already exists
    bool hasContent(const Page<KeyType>& page) {
        Page<KeyType> page_copy = page;
        page_copy.updateContentHash();
        return content_map.find(page_copy.getContentHash()) != content_map.end();
    }
    
    // Get the page ID for existing content
    uint16_t getPageIdForContent(const Page<KeyType>& page) {
        Page<KeyType> page_copy = page;
        page_copy.updateContentHash();
        auto it = content_map.find(page_copy.getContentHash());
        if (it != content_map.end()) {
            return it->second->header.page_id;
        }
        return 0;
    }
};
