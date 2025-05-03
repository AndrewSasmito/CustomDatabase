#include <iostream>
#include <cassert>
#include "db/database.h"

int main() { // Simple interface for now
    Database db; // Create db instance

    // Sample table layout
    db.createTable("users", {"id", "name"});
    Table* users = db.getTable("users");

    assert(users);
    users->insert({"1", "personA"});
    users->insert({"2", "personB"});

    assert(users->printAll().compare("1 personA \n2 personB \n") == 0);

    std::cout << "All tests have successfully passed!\n";
    return 0;
}
