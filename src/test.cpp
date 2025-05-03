#include <iostream>
#include <cassert>
#include "db/database.h"


// If you return anything but 0, the github workflow will return an error!
int main() { // Simple interface for now
    Database db; // Create db instance

    // Sample table layout
    db.createTable("users", {"id", "name"});
    Table* users = db.getTable("users");

    assert(users); //Check that the users are saved
    assert(users->insert({"1", "personA"}) == 0);
    assert(users->insert({"2", "personB"}) == 0);

    assert(users->printAll().compare("1 personA \n2 personB \n") == 0); //Check that the users are in the database

    std::cout << "All tests have successfully passed!\n";
    return 0;
}
