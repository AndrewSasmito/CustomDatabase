#include "db/table.h"

// Constructor, to initialize the table with column names
Table::Table(const std::vector<std::string>& columns) : columns(columns) {}

// Note: these are all temp functions for now
// Insert a new row into the table
// Return 0 on success, anything else on error
bool Table::insert(const std::vector<std::string>& values) {
    if (values.size() == columns.size()) {
        rows.push_back(values);
        return 0;
    } else {
        return 1;
    }
}

// Print all rows in the table
std::string Table::printAll() {
    std::string resultString = "";
    for (const auto& row : rows) {
        for (const auto& val : row) {
            resultString += val + " ";
        }
        resultString += "\n";
    }
    return resultString;
}
