#include <iostream>
#include <string>
#include "btree.h"
#include "content_hash.h"

int main() {
    std::cout << "=== Content-Addressable Storage Demo ===" << std::endl;
    
    // Create a B-tree
    BTree<int, std::string> tree(3);
    
    // Insert some data
    std::cout << "Inserting data..." << std::endl;
    tree.insert(1, "apple");
    tree.insert(2, "banana");
    tree.insert(3, "cherry");
    
    // Demonstrate content hashing
    std::cout << "\nContent Hash Demo:" << std::endl;
    std::cout << "1. Same content should have same hash" << std::endl;
    std::cout << "2. Different content should have different hashes" << std::endl;
    
    // Create some test data
    std::vector<uint8_t> data1 = {1, 2, 3, 4, 5};
    std::vector<uint8_t> data2 = {1, 2, 3, 4, 5}; // Same as data1
    std::vector<uint8_t> data3 = {1, 2, 3, 4, 6}; // Different from data1
    
    std::string hash1 = ContentHash::computeHash(data1);
    std::string hash2 = ContentHash::computeHash(data2);
    std::string hash3 = ContentHash::computeHash(data3);
    
    std::cout << "\nHash of data1: " << hash1 << std::endl;
    std::cout << "Hash of data2: " << hash2 << std::endl;
    std::cout << "Hash of data3: " << hash3 << std::endl;
    
    std::cout << "\nData1 == Data2: " << (hash1 == hash2 ? "Yes" : "No") << std::endl;
    std::cout << "Data1 == Data3: " << (hash1 == hash3 ? "Yes" : "No") << std::endl;
    
    std::cout << "\nContent-addressable storage benefits:" << std::endl;
    std::cout << "✓ Eliminates duplicate pages" << std::endl;
    std::cout << "✓ Enables data deduplication" << std::endl;
    std::cout << "✓ Reduces storage requirements" << std::endl;
    std::cout << "✓ Improves cache efficiency" << std::endl;
    
    return 0;
}
