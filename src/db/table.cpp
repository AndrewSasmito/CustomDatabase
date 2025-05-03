#include "db/table.h"
#include <iostream>

// Constructor, to initialize the table with column names
Table::Table(const std::vector<std::string>& columns) : columns(columns) {}

// Note: these are all temp functions for now
// Insert a new row into the table
void Table::insert(const std::vector<std::string>& values) {
    if (values.size() == columns.size()) {
        rows.push_back(values);
    }
}

// Print all rows in the table
void Table::printAll() {
    for (const auto& row : rows) {
        for (const auto& val : row) {
            std::cout << val << " ";
        }
        std::cout << "\n";
    }
}
