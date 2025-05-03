#pragma once
#include <vector>
#include <string>
#include <unordered_map>

class Table {
public: // Constructor, to initialize the table with column names
    Table() = default;
    Table(const std::vector<std::string>& columns);
    void insert(const std::vector<std::string>& values);
    std::string printAll(); // Sample use of print function

private: // Store column names and rows
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};
