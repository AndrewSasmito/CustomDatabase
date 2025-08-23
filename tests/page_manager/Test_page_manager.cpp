#include "page_manager.h"

#include <assert.h>
#include <iostream>

int main(){
    PageHeader page_header = {1, 0, 0, sizeof(SlotEntry) + 3, "", 0};

    std::cout << "Brainiac\n";
    Page page = {page_header, std::vector<SlotEntry> {}, std::vector<uint8_t> {}};

    std::vector<uint8_t> record;
    record.push_back(24);   
    record.push_back(48);   
    std::cout << "Attempting to insert a record\n";
    assert(insertRecord(&page, record));

    std::string pageCurrentHash = page.header.checksum;

    std::cout << "Attempting to delete a record\n";
    assert(deleteRecord(&page, page.slot_directory[0].id));

    std::cout << "Checking that the hash changed\n";
    std::cout << "Hashes: " << page.header.checksum << ' ' << pageCurrentHash << '\n';
    assert(page.header.checksum != pageCurrentHash);

    // It is full
    std::cout << "Attempting to insert a record into a full page\n";
    assert(!(insertRecord(&page, record)));

    return 0;
}