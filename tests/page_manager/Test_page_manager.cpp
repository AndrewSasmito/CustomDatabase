#include "page_manager.h"

#include <assert.h>
#include <iostream>

int main(){
    PageHeader page_header = {1, 0, 0, sizeof(SlotEntry) + 2, "", 0};

    Page page = {page_header, std::vector<SlotEntry> {}, std::vector<uint8_t> {}};

    std::vector<uint8_t> record;
    record.push_back(24);   
    record.push_back(48);   
    std::cout << "Attempting to insert a record\n";
    assert(insertRecord<int>(&page, record));

    std::string pageCurrentHash = page.header.checksum;

    std::cout << "Marking the slot to be deleted\n";
    assert(markDeleteRecord(&page, page.slot_directory[0].id));

    std::cout << "Deleting all dirty records\n";
    std::cout << page.header.free_space_size << '\n';
    assert(deleteRecord(&page));

    std::cout << "Checking that the hash changed\n";
    std::cout << "Hashes: " << page.header.checksum << ' ' << pageCurrentHash << '\n';
    assert(page.header.checksum != pageCurrentHash);

    // It is full
    std::cout << "Attempting to insert a record into a full page\n";
    record.push_back(64);
    std::cout << "Statistics: " << page.header.free_space_size << ' ' << record.size() + sizeof(SlotEntry) << '\n';
    assert(!(insertRecord(&page, record)));

    return 0;
}