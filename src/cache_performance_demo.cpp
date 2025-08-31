#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include "btree.h"

int main() {
    std::cout << "=== Cache & Writer Queue Performance Demo ===" << std::endl;
    
    // Create B-tree with small node size to force more operations
    BTree<int, std::string> tree(3); // 3 keys per node
    
    // Random test data cuz I was craving fruits
    std::vector<std::pair<int, std::string>> test_data;
    std::vector<std::string> fruits = {"apple", "banana", "cherry", "date", "elderberry", 
                                       "fig", "grape", "honeydew", "kiwi", "lemon"};
    
    for (int i = 1; i <= 50; ++i) {
        test_data.emplace_back(i, fruits[i % fruits.size()] + "_" + std::to_string(i));
    }
    
    std::cout << "\n1. Inserting " << test_data.size() << " key-value pairs" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& pair : test_data) {
        tree.insert(pair.first, pair.second);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Insert completed in: " << insert_duration.count() << " microseconds" << std::endl;
    
    // Test search performance (should benefit from cache)
    std::cout << "\n2. Testing search performance..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    
    int successful_searches = 0;
    for (int i = 1; i <= 50; ++i) {
        auto result = tree.search(i);
        if (result) {
            successful_searches++;
            delete result;
        }
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto search_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Found " << successful_searches << " keys" << std::endl;
    std::cout << "Search completed in: " << search_duration.count() << " microseconds" << std::endl;
    
    // Test repeated searches (should show cache benefits)
    std::cout << "\n3. Testing repeated searches (cache should help)..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    
    successful_searches = 0;
    for (int round = 0; round < 3; ++round) {
        for (int i = 1; i <= 20; ++i) {
            auto result = tree.search(i);
            if (result) {
                successful_searches++;
                delete result;
            }
        }
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto repeated_search_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Found " << successful_searches << " keys across 3 rounds" << std::endl;
    std::cout << "Repeated searches completed in: " << repeated_search_duration.count() << " microseconds" << std::endl;
    
    // Insert more data to trigger more cache and writer queue activity
    std::cout << "\n4. Inserting additional data to stress test cache and writer queue..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 51; i <= 100; ++i) {
        tree.insert(i, "stress_test_" + std::to_string(i));
    }
    
    end = std::chrono::high_resolution_clock::now();
    auto stress_insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Stress test insert completed in: " << stress_insert_duration.count() << " microseconds" << std::endl;
    
    // Flush all pending writes
    std::cout << "\n5. Flushing all pending writes..." << std::endl;
    start = std::chrono::high_resolution_clock::now();
    
    tree.flush();
    
    end = std::chrono::high_resolution_clock::now();
    auto flush_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Flush completed in: " << flush_duration.count() << " microseconds" << std::endl;
    
    // Final verification
    std::cout << "\n6. Final verification - searching for some keys..." << std::endl;
    std::vector<int> test_keys = {1, 25, 50, 75, 100};
    
    for (int key : test_keys) {
        auto result = tree.search(key);
        if (result) {
            std::cout << "Key " << key << ": " << *result << std::endl;
            delete result;
        } else {
            std::cout << "Key " << key << ": NOT FOUND" << std::endl;
        }
    }
    
    // Show storage stats
    std::cout << "\n7. Storage statistics:" << std::endl;
    tree.printStorageStats();
    
    std::cout << "\n=== Performance Summary ===" << std::endl;
    std::cout << "Initial insert (50 items): " << insert_duration.count() << " μs" << std::endl;
    std::cout << "Search (50 items): " << search_duration.count() << " μs" << std::endl;
    std::cout << "Repeated searches (60 items): " << repeated_search_duration.count() << " μs" << std::endl;
    std::cout << "Stress insert (50 items): " << stress_insert_duration.count() << " μs" << std::endl;
    std::cout << "Flush time: " << flush_duration.count() << " μs" << std::endl;
    
    double avg_insert_time = (insert_duration.count() + stress_insert_duration.count()) / 100.0;
    double avg_search_time = search_duration.count() / 50.0;
    
    std::cout << "Average insert time: " << avg_insert_time << " μs per item" << std::endl;
    std::cout << "Average search time: " << avg_search_time << " μs per item" << std::endl;
    
    std::cout << "\n=== Cache & Writer Queue Benefits ===" << std::endl;
    std::cout << "✓ Pages are cached for faster repeated access" << std::endl;
    std::cout << "✓ Writes are batched and processed in background" << std::endl;
    std::cout << "✓ Content-addressable storage still provides deduplication" << std::endl;
    std::cout << "✓ Multi-threaded write processing improves throughput" << std::endl;
    
    return 0;
}
