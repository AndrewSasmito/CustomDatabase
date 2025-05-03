#include "db/database.h"

// Create a table with the given name and columns
void Database::createTable(const std::string& name, const std::vector<std::string>& columns) {
    tables[name] = Table(columns);
}

/* Get a pointer to the table with the given name
*   Return a pointer to the table on success, nullptr otherwise
*/ 
Table* Database::getTable(const std::string& name) {
    if (tables.find(name) != tables.end()) {
        return &tables[name]; // Return ptr
    }
    return nullptr;
}
