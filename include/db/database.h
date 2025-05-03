#pragma once
#include <string>
#include <unordered_map>
#include "table.h"
#include "btree.h"

class Database {
    public: // Create a table with the given name and columns
        void createTable(const std::string& name, const std::vector<std::string>& columns);
        Table* getTable(const std::string& name);


    private: // Store tables in a map with the table name as the key
        std::unordered_map<std::string, Table> tables;
};