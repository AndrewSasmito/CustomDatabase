#include "page_manager.h"
/*
*   Add a new record into the page
*   Store the binary representation of the record
*    
*   Return true if successful, false otherwise
*/
template <typename KeyType>
bool insertRecord(Page<KeyType> *page, const std::vector<uint8_t>& record) {
    uint16_t len = record.size(), slotSize = sizeof(SlotEntry);
    if (page->header.free_space_size < len + slotSize) return false;

    SlotEntry slot = {page->header.num_slots, page->header.free_space_offset, len, false};
    page->slot_directory.push_back(slot);
    
    page->header.num_slots += 1;
    page->header.free_space_size -= len + slotSize;
    page->header.free_space_offset += len;
    
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
template <typename KeyType>
bool deleteRecord(Page<KeyType> *page, uint16_t slot_id) {
    if (slot_id >= page->header.num_slots || slot_id < 0) return false;
    page->slot_directory[slot_id].is_deleted = true;

    return true;
}

/*
    Delete all records in the page that are marked for deletion
    Return true if succesfully deleted, false in out of bounds
*/
template <typename KeyType>
bool deleteRecord(Page<KeyType> *page) {
    std::vector<uint8_t> new_data;
    std::vector<SlotEntry> new_directory;
    for (SlotEntry slot : page->slot_directory){
        if (slot.is_deleted == true){
            --(page->header.num_slots);
            page->header.free_space_size += (sizeof(SlotEntry) + slot.length);
        }else{
            // Add kept data to data and new_directory
            int new_offset = new_data.size();
            for (int i = 0; i<slot.length; i++){
                try{
                    new_data.push_back(page->data[slot.offset + i]);
                }catch(std::exception& ex){
                    // Array out of bounds
                    return false;
                }
            }
            slot.offset = new_offset;

            new_directory.push_back(slot);
        }
    }

    page -> slot_directory = new_directory;
    page -> data = new_data;

    updatePageChecksum(page);
    return true;
}

/*
    When you add or delete a record, you have to update the checksum hash

*/
template <typename KeyType>
void updatePageChecksum(Page<KeyType> *page) {
    page->header.checksum = compute_sha256_page_management(page->data);
}

/*
 * BTreeNode Constructor Implementation
 */
template <typename KeyType>
Page<KeyType> createPage(bool is_leaf) {
    Page<KeyType> page;
    page.is_leaf = is_leaf;
    page.keys = std::vector<KeyType>();

    if (is_leaf) {
        // Leaf nodes store actual data in values
        // Children are not used, but we leave the vector empty
        page.data = std::vector<uint8_t>();
    } else {
        // Internal nodes store keys for indexing, and pointers to children
        // They dont store values
        page.children = std::vector<uint16_t>();
    }
    return page;
}