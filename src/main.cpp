#include <iostream>
#include "db/database.h"

int main() { // Simple interface for now
    Database db; // Create db instance

    // Sample table layout
    db.createTable("users", {"id", "name"});
    Table* users = db.getTable("users");

    if (users) { // Insert some basic stuff
        users->insert({"1", "personA"});
        users->insert({"2", "personB"});
        users->printAll();
    } else {
        std::cerr << "Table not found\n"; // Error msg
    }

    return 0;
}
