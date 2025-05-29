#include "page_manager.h"

/*
*   Add a new record into the page
*   Store the binary representation of the record
*    
*   Return true if successful, false otherwise
*/
bool insertRecord(Page *page, const std::vector<uint8_t>& record) {
    uint16_t len = record.size(), slotSize = sizeof(SlotEntry);
    if (page->header.free_space_size < len + slotSize) return false;

    SlotEntry slot = {page->header.free_space_offset, len, false};
    page->slot_directory.push_back(slot);
    
    page->header.num_slots += 1;
    page->header.free_space_size -= (len + slotSize);
    page->header.free_space_offset += len + slotSize;
    
    for (auto info : record){
        page->data.push_back(info);
    }
    
    updatePageChecksum(page); // Need to update
    return true;
}

/*
    Delete the record with index slot_id from the page

    Return true if successful, false otherwise
*/
bool deleteRecord(Page *page, uint16_t slot_id) {
    if (slot_id >= page->header.num_slots || slot_id < 0) return false;
    page->slot_directory[slot_id].is_deleted = true;

    updatePageChecksum(page);
    return true;
}

/*
    When you add or delete a record, you have to update the checksum hash

*/
void updatePageChecksum(Page *page) {
    page->header.checksum = compute_sha256_page_management(page->data);
}