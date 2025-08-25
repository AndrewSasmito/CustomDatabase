#include <iostream>
#include <string>
#include <vector>
#include "btree.h"
#include "page_manager.h"

int main() {
    std::cout << "=== Content-Addressable Storage Deep Dive ===" << std::endl;
    
    // Create two pages with identical content
    std::cout << "\n1. Creating two pages with identical content:" << std::endl;
    
    Page<int> page1 = createPage<int>(true);
    Page<int> page2 = createPage<int>(true);
    
    // Add same keys to both pages
    page1.keys = {1, 2, 3};
    page2.keys = {1, 2, 3};
    
    // Add same data to both pages
    std::string data1 = "apple";
    std::string data2 = "banana";
    std::string data3 = "cherry";
    
    // Serialize the data
    std::vector<uint8_t> serialized_data;
    for (char c : data1 + data2 + data3) {
        serialized_data.push_back(static_cast<uint8_t>(c));
    }
    
    page1.data = serialized_data;
    page2.data = serialized_data;
    
    // Update content hashes
    page1.updateContentHash();
    page2.updateContentHash();
    
    std::cout << "Page 1 content hash: " << page1.getContentHash() << std::endl;
    std::cout << "Page 2 content hash: " << page2.getContentHash() << std::endl;
    std::cout << "Pages have same content: " << (page1.hasSameContent(page2) ? "YES" : "NO") << std::endl;
    
    // Create a third page with different content
    std::cout << "\n2. Creating a third page with different content:" << std::endl;
    
    Page<int> page3 = createPage<int>(true);
    page3.keys = {1, 2, 4}; // Different key
    page3.data = serialized_data;
    page3.updateContentHash();
    
    std::cout << "Page 3 content hash: " << page3.getContentHash() << std::endl;
    std::cout << "Page 1 and Page 3 have same content: " << (page1.hasSameContent(page3) ? "YES" : "NO") << std::endl;
    
    // Demonstrate storage efficiency
    std::cout << "\n3. Storage Efficiency Benefits:" << std::endl;
    std::cout << "Traditional storage would store:" << std::endl;
    std::cout << "  - Page 1: " << page1.keys.size() << " keys + " << page1.data.size() << " bytes" << std::endl;
    std::cout << "  - Page 2: " << page2.keys.size() << " keys + " << page2.data.size() << " bytes" << std::endl;
    std::cout << "  - Page 3: " << page3.keys.size() << " keys + " << page3.data.size() << " bytes" << std::endl;
    std::cout << "  Total: " << (page1.keys.size() + page2.keys.size() + page3.keys.size()) << " keys + " 
              << (page1.data.size() + page2.data.size() + page3.data.size()) << " bytes" << std::endl;
    
    std::cout << "\nContent-addressable storage would store:" << std::endl;
    std::cout << "  - Unique content 1 (hash: " << page1.getContentHash() << "): " 
              << page1.keys.size() << " keys + " << page1.data.size() << " bytes" << std::endl;
    std::cout << "  - Unique content 2 (hash: " << page3.getContentHash() << "): " 
              << page3.keys.size() << " keys + " << page3.data.size() << " bytes" << std::endl;
    std::cout << "  Total: " << (page1.keys.size() + page3.keys.size()) << " keys + " 
              << (page1.data.size() + page3.data.size()) << " bytes" << std::endl;
    
    std::cout << "\nSavings: " << page2.keys.size() << " keys + " << page2.data.size() << " bytes eliminated!" << std::endl;
    
    return 0;
}
