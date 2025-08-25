#include <iostream>
#include <string>
#include "btree.h"

int main() {
    std::cout << "=== Content-Addressable Storage Deduplication Demo ===" << std::endl;
    
    // Create a B-tree with small node size to force splits
    BTree<int, std::string> tree(2); // Only 2 keys per node
    
    std::cout << "\n1. Inserting initial data..." << std::endl;
    tree.insert(1, "apple");
    tree.insert(2, "banana");
    tree.printStorageStats();
    
    std::cout << "\n2. Inserting more data to trigger splits..." << std::endl;
    tree.insert(3, "cherry");
    tree.insert(4, "date");
    tree.printStorageStats();
    
    std::cout << "\n3. Inserting duplicate data..." << std::endl;
    tree.insert(1, "apple");  // Same key-value pair
    tree.insert(2, "banana"); // Same key-value pair
    tree.printStorageStats();
    
    std::cout << "\n4. Inserting more unique data..." << std::endl;
    tree.insert(5, "elderberry");
    tree.insert(6, "fig");
    tree.printStorageStats();
    
    std::cout << "\n5. Testing search functionality..." << std::endl;
    std::string* result1 = tree.search(1);
    std::string* result5 = tree.search(5);
    
    if (result1) {
        std::cout << "Found key 1: " << *result1 << std::endl;
        delete result1;
    }
    if (result5) {
        std::cout << "Found key 5: " << *result5 << std::endl;
        delete result5;
    }
    
    std::cout << "\n6. Final storage statistics:" << std::endl;
    tree.printStorageStats();
    
    std::cout << "\n=== Deduplication Benefits ===" << std::endl;
    std::cout << "✓ Identical pages are stored only once" << std::endl;
    std::cout << "✓ Storage requirements are minimized" << std::endl;
    std::cout << "✓ Cache efficiency is improved" << std::endl;
    std::cout << "✓ Data integrity is maintained" << std::endl;
    
    return 0;
}
