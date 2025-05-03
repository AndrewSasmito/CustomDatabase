#pragma once
#include <string>
#include <unordered_map>
#include "table.h"

class Database {
public: // Create a table with the given name and columns
    void createTable(const std::string& name, const std::vector<std::string>& columns);
    Table* getTable(const std::string& name);


private: // Store tables in a map with the table name as the key
    std::unordered_map<std::string, Table> tables;
};


using VariantType = std::variant<int, float, std::string, fraction>;

// extern types
extern std::unordered_map<std::string, BTree<int, int>> hashTreeStorage;
extern std::unordered_map<std::string, BTree<int, int>> intTreeStorage;
extern std::unordered_map<std::string, BTree<std::string, int>> stringTreeStorage;
extern std::unordered_map<std::string, BTree<fraction, int>> decTreeStorage;
extern std::vector<VariantType> memory;
