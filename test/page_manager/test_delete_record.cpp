#include <page_manager.h>

int main(){
    PageHeader page_header;
    SlotEntry slot;
    Page *page;
    std::vector<uint8_t> record;
    std::vector<SlotEntry> slotdir;
    
    record.push_back(10);
    record.push_back(20);
    
    assert(insertRecord(&page, record))

    slot.offset = 0;
    slot.length = sizeof(int);
    slot.slot_id = 0;
    slot.is_deleted = -1;

    page_header.num_slots = 2;
    page_header.checksum = "a";

    page->header = page_header;
    slotdir.push_back(slot);
    page->slot_directory = slotdir;


    assert(deleteRecord(page, slot.slot_id));
    assert(page->header.checksum != "a");

    insertRecord(&page, record);

    return 0;
}