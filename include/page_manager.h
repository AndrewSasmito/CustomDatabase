#pragma once
#include <cstdint>
#include <vector>
#include "hash_util.h"

struct PageHeader {
    uint16_t page_id;
    uint16_t num_slots; // number of records
    uint16_t free_space_offset;  // Start of free space
    uint16_t free_space_size;    // Bytes of free space
    std::string checksum;           // SHA hash
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
};

template <typename KeyType>
Page<KeyType> createPage(bool is_leaf);

template <typename KeyType>
bool insertRecord(Page<KeyType> *page, const std::vector<uint8_t>& record);

template <typename KeyType>
bool deleteRecord(Page<KeyType> *page, uint16_t slot_id);

template <typename KeyType>
void updatePageChecksum(Page<KeyType> *page);

