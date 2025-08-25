#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "content_hash.h"
// #include "hash_util.h"  // Commented out to avoid Botan dependency

struct PageHeader {
    uint16_t page_id;
    uint16_t num_slots; // number of records
    uint16_t free_space_offset;  // Start of free space
    uint16_t free_space_size;    // Bytes of free space
    std::string checksum;           // SHA hash
    std::string content_hash;       // Content-addressable hash
    uint8_t flags;               // e.g, for dirty, deleted, etc
};

struct SlotEntry {
    uint16_t id;
    uint16_t offset;  // Offset from start of page
    uint16_t length;  // Length of the record
    uint8_t is_deleted;  // Logical deletion flag
};

template <typename KeyType> 
struct Page {
    PageHeader header;
    bool is_leaf;

    // Internal-node-only
    std::vector<KeyType> keys;
    std::vector<uint16_t> children; // Page IDs of child pages

    std::vector<SlotEntry> slot_directory;
    std::vector<uint8_t> data; // Raw bytes of PAGE_SIZE - header/slots
    
    // Content-addressable storage methods
    void updateContentHash() {
        std::vector<uint8_t> content;
        
        // Add keys to content
        for (const auto& key : keys) {
            const uint8_t* key_bytes = reinterpret_cast<const uint8_t*>(&key);
            content.insert(content.end(), key_bytes, key_bytes + sizeof(KeyType));
        }
        
        // Add data to content
        content.insert(content.end(), data.begin(), data.end());
        
        header.content_hash = ContentHash::computeHash(content);
    }
    
    std::string getContentHash() const {
        return header.content_hash;
    }
    
    bool hasSameContent(const Page<KeyType>& other) const {
        return header.content_hash == other.header.content_hash;
    }
};

template <typename KeyType>
Page<KeyType> createPage(bool is_leaf);

template <typename KeyType>
bool insertRecord(Page<KeyType> *page, const std::vector<uint8_t>& record);

template <typename KeyType>
bool deleteRecord(Page<KeyType> *page, uint16_t slot_id);

template <typename KeyType>
void updatePageChecksum(Page<KeyType> *page);

