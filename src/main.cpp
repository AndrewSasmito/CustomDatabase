#include <iostream>
#include <string>
#include <sstream>
#include "btree.h"

int main() {
    std::cout << "=== B-Tree Database Test Interface ===" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  insert <key> <value>  - Insert a key-value pair" << std::endl;
    std::cout << "  delete <key>          - Delete a key" << std::endl;
    std::cout << "  search <key>          - Search for a key" << std::endl;
    std::cout << "  print                 - Print tree structure" << std::endl;
    std::cout << "  quit                  - Exit" << std::endl;
    std::cout << "=====================================" << std::endl;

    // Create a B-tree with max 3 keys per node for easy testing
    BTree<int, std::string> tree(3);
    
    std::string command;
    while (true) {
        std::cout << "\n> ";
        std::getline(std::cin, command);
        
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        
        if (cmd == "quit" || cmd == "exit") {
            std::cout << "Goodbye!" << std::endl;
            break;
        }
        else if (cmd == "insert") {
            int key;
            std::string value;
            if (iss >> key >> value) {
                try {
                    tree.insert(key, new std::string(value));
                    std::cout << "Inserted: " << key << " -> " << value << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error inserting: " << e.what() << std::endl;
                }
            } else {
                std::cout << "Usage: insert <key> <value>" << std::endl;
            }
        }
        else if (cmd == "delete") {
            int key;
            if (iss >> key) {
                try {
                    tree.deleteKey(key);
                    std::cout << "Deleted key: " << key << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error deleting: " << e.what() << std::endl;
                }
            } else {
                std::cout << "Usage: delete <key>" << std::endl;
            }
        }
        else if (cmd == "search") {
            int key;
            if (iss >> key) {
                std::string* result = tree.search(key);
                if (result) {
                    std::cout << "Found key: " << key << " -> " << *result << std::endl;
                } else {
                    std::cout << "Key not found: " << key << std::endl;
                }
            } else {
                std::cout << "Usage: search <key>" << std::endl;
            }
        }
        else if (cmd == "print") {
            std::cout << "Tree structure (simplified):" << std::endl;
            std::cout << "B-tree with max " << 3 << " keys per node" << std::endl;
        }
        else if (cmd.empty()) {
            continue;
        }
        else {
            std::cout << "Unknown command: " << cmd << std::endl;
            std::cout << "Available commands: insert, delete, search, print, quit" << std::endl;
        }
    }
    
    return 0;
}
