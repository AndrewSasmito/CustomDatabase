#include "btree.h"
#include "fraction.h"
#include <cstring>

/*
 * BTree Constructor Implementation
 */
template <typename KeyType, typename ValueType>
BTree<KeyType, ValueType>::BTree(int maxKeys) 
    : maxKeysPerNode(maxKeys), 
      page_cache(&content_storage, 50),  // Cache up to 50 pages, can change latr 
      writer_queue(&content_storage, &page_cache, 2),  // 2 writer threads
      wal_manager("btree.wal", 8192), // 8KB WAL buffer
      current_transaction(0) {
    
    writer_queue.start();
    
    // Start first transaction
    current_transaction = wal_manager.beginTransaction();
    
    // Initially, the tree is empty, so we create a root node
    // and mark it as a leaf (all data starts at the leaf level in B+ Trees)
    uint16_t root_id = content_storage.storePage(createPage<KeyType>(true));
    root = page_cache.getPage(root_id);  // Get through cache
}

/*
 * BTree Destructor Implementation to stop writer q and flush pending writes
 */
template <typename KeyType, typename ValueType>
BTree<KeyType, ValueType>::~BTree() {
    if (current_transaction != 0) {
        wal_manager.commitTransaction(current_transaction);
    }
    
    writer_queue.stop();
    page_cache.flushAll();
    wal_manager.sync();
}

/*
 * Flush pending writes
 */
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::flush() {
    writer_queue.waitForEmpty();
    page_cache.flushAll();
}

/*
 * Transaction Management Methods
 */
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::beginTransaction() {
    if (current_transaction != 0) {
        wal_manager.commitTransaction(current_transaction);
    }
    current_transaction = wal_manager.beginTransaction();
}

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::commitTransaction() {
    if (current_transaction != 0) {
        wal_manager.commitTransaction(current_transaction);
        current_transaction = 0;
    }
}

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::abortTransaction() {
    if (current_transaction != 0) {
        wal_manager.abortTransaction(current_transaction);
        current_transaction = 0;
    }
}

/*
 * Placeholder method to insert key value pairs 
 */
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::insert(const KeyType& key, const ValueType& value) {
    // Ensure we have an active transaction
    if (current_transaction == 0) {
        current_transaction = wal_manager.beginTransaction();
    }
    
    // Serialize the value for WAL logging
    std::vector<uint8_t> serialized_value;
    const uint8_t* value_bytes = reinterpret_cast<const uint8_t*>(&value);
    serialized_value.assign(value_bytes, value_bytes + sizeof(ValueType));
    
    if (!root) { // If tree is empty, create a new root
        uint16_t root_id = content_storage.storePage(createPage<KeyType>(true));
        root = page_cache.getPage(root_id);
        
        // Log the insert operation
        wal_manager.logInsert(current_transaction, root_id, key, serialized_value);
    } else if (root->keys.size() == maxKeysPerNode) { // Check if the root is full
        Page<KeyType> new_root_page = createPage<KeyType>(false);
        new_root_page.children.push_back(root->header.page_id); // Page ID of the old root
        splitChild(std::make_shared<Page<KeyType>>(new_root_page), 0, root); // Split child bc of overflow
        
        // Store the new root using cache and writer queue
        page_cache.putPage(new_root_page.header.page_id, std::make_shared<Page<KeyType>>(new_root_page));
        writer_queue.enqueueWrite(new_root_page.header.page_id, std::make_shared<Page<KeyType>>(new_root_page));
        root = page_cache.getPage(new_root_page.header.page_id);
    }
    
    // Log the insert operation for all cases so that we can rollback if needed
    wal_manager.logInsert(current_transaction, root->header.page_id, key, serialized_value);
    
    // Now the root is guaranteed to not be empty
    insertNonFull(root, key, value); // Insert
   
}

// Find the node that has a key
template <typename KeyType, typename ValueType>
Page<KeyType> BTree<KeyType, ValueType>::findKey(std::shared_ptr<Page<KeyType>> node, const KeyType& key){
    size_t idx = 0; // Index to find the key
    while (idx < node->keys.size() && key > node->keys[idx]) {
        idx++;
    }
    if (node->is_leaf) { // If leaf node, just return
        if (idx < node->keys.size() && node->keys[idx] == key) {
            return *node;
        } else {
            // Key not found
            throw std::runtime_error("key not found");
        }
    } else { // If not leaf, need to find the child
        if (idx < node->keys.size() && node->keys[idx] == key) {
            idx++; // move to child that might have key
        }
        if (idx < node->children.size()) {
            // Load child page from cache
            auto child_page = page_cache.getPage(node->children[idx]);
            if (child_page) {
                return findKey(child_page, key);
            } else {
                throw std::runtime_error("child page not found");
            }
        } else {
            throw std::runtime_error("key not found");
        }
    }
}

// Function that traverses tree and inserts into a node that isnt full. helper for insert
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::insertNonFull(std::shared_ptr<Page<KeyType>> node, const KeyType& key, const ValueType& value) {
    int i = node->keys.size() - 1; // Start from the last key

    if (node->is_leaf) { // If its a leaf node, insert the key and value
        // Create a copy of the node to modify
        Page<KeyType> modified_node = *node;
        
        // Insert in sorted order
        modified_node.keys.push_back(key);
        
        // Serialize the value to binary data
        std::vector<uint8_t> serialized_value;
        const uint8_t* value_bytes = reinterpret_cast<const uint8_t*>(&value);
        serialized_value.assign(value_bytes, value_bytes + sizeof(ValueType));
        
        // Insert the serialized value into the data vector
        modified_node.data.insert(modified_node.data.end(), serialized_value.begin(), serialized_value.end());
        
        // Sort both keys and data together
        for (int j = modified_node.keys.size() - 1; j > 0 && modified_node.keys[j] < modified_node.keys[j - 1]; --j) {
            std::swap(modified_node.keys[j], modified_node.keys[j - 1]); // Swap keys
            
            // Swap the corresponding data blocks
            size_t value_size = sizeof(ValueType);
            size_t offset1 = j * value_size;
            size_t offset2 = (j - 1) * value_size;
            
            for (size_t k = 0; k < value_size; ++k) {
                std::swap(modified_node.data[offset1 + k], modified_node.data[offset2 + k]);
            }
        }
        
        // Store the modified page using cache and writer queue
        page_cache.putPage(modified_node.header.page_id, std::make_shared<Page<KeyType>>(modified_node));
        writer_queue.enqueueWrite(modified_node.header.page_id, std::make_shared<Page<KeyType>>(modified_node));
        node = page_cache.getPage(modified_node.header.page_id);
        
    } else {
        // Find child to descend into
        while (i >= 0 && key < node->keys[i])
            i--;
        i++;

        // Load child page from cache
        auto child_page = page_cache.getPage(node->children[i]);
        if (!child_page) {
            throw std::runtime_error("child page not found");
        }
        
        if (child_page->keys.size() == maxKeysPerNode) { // If the child is full, split it
            splitChild(node, i, child_page);
            if (key > node->keys[i]) i++; // Check which child to go to after split
        }

        // Recursively insert into child
        insertNonFull(child_page, key, value);
    }
}

// Function to split a child node in case its full. helper for insert
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::splitChild(std::shared_ptr<Page<KeyType>> parent, int index, std::shared_ptr<Page<KeyType>> child) {
    int mid = maxKeysPerNode / 2; // Remember b+tree property

    // Create copies to modify
    Page<KeyType> modified_child = *child;
    Page<KeyType> new_child_page = createPage<KeyType>(child->is_leaf);

    // Copy second half of keys/values to the new node
    new_child_page.keys.assign(modified_child.keys.begin() + mid + 1, modified_child.keys.end()); // Copy keys
    modified_child.keys.resize(mid); // Keep the mid key in left for b+ tree

    if (modified_child.is_leaf) { // If its a leaf, assign values
        size_t value_size = sizeof(ValueType);
        size_t start_offset = (mid + 1) * value_size;
        new_child_page.data.assign(modified_child.data.begin() + start_offset, modified_child.data.end());
        modified_child.data.resize((mid + 1) * value_size);  // Keep the mid key in left for b+ tree
    } else { // If not leaf, copy children
        new_child_page.children.assign(modified_child.children.begin() + mid + 1, modified_child.children.end());
        modified_child.children.resize(mid + 1);
    }

    // Store both modified pages using cache and writer queue
    page_cache.putPage(modified_child.header.page_id, std::make_shared<Page<KeyType>>(modified_child));
    writer_queue.enqueueWrite(modified_child.header.page_id, std::make_shared<Page<KeyType>>(modified_child));
    
    uint16_t new_child_id = content_storage.storePage(new_child_page);  // New page needs ID first
    page_cache.putPage(new_child_id, std::make_shared<Page<KeyType>>(new_child_page));
    writer_queue.enqueueWrite(new_child_id, std::make_shared<Page<KeyType>>(new_child_page));
    
    // Update parent
    parent->children.insert(parent->children.begin() + index + 1, new_child_id); // Insert new child page ID
    parent->keys.insert(parent->keys.begin() + index, modified_child.keys[mid]); // Insert the mid key into parent
}

// Helper function to delete a key
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::deleteKey(const KeyType& key) {
    if (!root) return; // If tree is empty, nothing to delete
    deleteFromNode(root, key); // Delete fromthe node

    // If root is now empty and has a child, make child the new root
    if (!root->is_leaf && root->keys.empty()) {
        auto child_page = page_cache.getPage(root->children[0]);
        if (child_page) {
            root = child_page;
        } else {
            uint16_t root_id = content_storage.storePage(createPage<KeyType>(true));
            root = page_cache.getPage(root_id);
        }
    }
}

// Helper function to delete key from node
// Pretty complex, but it handles the case of underflow and merging nodes. Search, delete
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::deleteFromNode(std::shared_ptr<Page<KeyType>> node, const KeyType& key) {
    size_t idx = 0; // Index to find the key
    while (idx < node->keys.size() && key > node->keys[idx]) { // Traverse to find key
        idx++;
    }

    if (node->is_leaf) { // If leaf node, just delete the key
        if (idx < node->keys.size() && node->keys[idx] == key) {
            // Create a copy to modify
            Page<KeyType> modified_node = *node;
            
            modified_node.keys.erase(modified_node.keys.begin() + idx); // Remove the key
            
            // Remove the corresponding data block
            size_t value_size = sizeof(ValueType);
            size_t start_offset = idx * value_size;
            modified_node.data.erase(modified_node.data.begin() + start_offset, modified_node.data.begin() + start_offset + value_size);
            
            // Store the modified page using cache and writer queue
            page_cache.putPage(modified_node.header.page_id, std::make_shared<Page<KeyType>>(modified_node));
            writer_queue.enqueueWrite(modified_node.header.page_id, std::make_shared<Page<KeyType>>(modified_node));
            node = page_cache.getPage(modified_node.header.page_id);
        } else {
            // Key not found
            return;
        }
    } else { // If not leaf, need to find the child to delete from
        if (idx < node->keys.size() && node->keys[idx] == key) {
            idx++; // move to child that might have key
        }

        // Load child page from cache
        auto child_page = page_cache.getPage(node->children[idx]);
        if (!child_page) {
            throw std::runtime_error("child page not found");
        }
        
        deleteFromNode(child_page, key); // Delete from child

        // Fix underflow (not enough keys in child)
        if (child_page->keys.size() < (maxKeysPerNode + 1) / 2) {
            // For now, just leave as is since we don't have proper sibling handling
        }
    }
}

// Borrow from left and right siblings

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::borrowFromLeft(std::shared_ptr<Page<KeyType>> parent, int index) {
    auto child_page = page_cache.getPage(parent->children[index]);
    auto sibling_page = page_cache.getPage(parent->children[index - 1]);
    
    if (!child_page || !sibling_page) {
        throw std::runtime_error("child or sibling page not found");
    }
    
    // Create copies to modify
    Page<KeyType> modified_child = *child_page;
    Page<KeyType> modified_sibling = *sibling_page;

    if (modified_child.is_leaf) { // If leaf, just borrow the last key from sibling
        modified_child.keys.insert(modified_child.keys.begin(), modified_sibling.keys.back()); // Insert at the beginning
        
        // Borrow the corresponding data
        size_t value_size = sizeof(ValueType);
        size_t sibling_offset = (modified_sibling.keys.size() - 1) * value_size;
        modified_child.data.insert(modified_child.data.begin(), 
                          modified_sibling.data.begin() + sibling_offset,
                          modified_sibling.data.begin() + sibling_offset + value_size);
        
        modified_sibling.keys.pop_back(); // Remove the last key from sibling
        modified_sibling.data.resize(modified_sibling.data.size() - value_size); // Remove the last value from sibling
        parent->keys[index - 1] = modified_child.keys[0]; // Update the parent key
        
        // Store modified pages using cache and writer queue
        page_cache.putPage(modified_child.header.page_id, std::make_shared<Page<KeyType>>(modified_child));
        writer_queue.enqueueWrite(modified_child.header.page_id, std::make_shared<Page<KeyType>>(modified_child));
        page_cache.putPage(modified_sibling.header.page_id, std::make_shared<Page<KeyType>>(modified_sibling));
        writer_queue.enqueueWrite(modified_sibling.header.page_id, std::make_shared<Page<KeyType>>(modified_sibling));
    } else { // If not leaf, borrow the last key and child pointer
        modified_child.keys.insert(modified_child.keys.begin(), parent->keys[index - 1]);
        parent->keys[index - 1] = modified_sibling.keys.back(); // Update the parent key
        modified_sibling.keys.pop_back(); // Remove the last key from sibling

        modified_child.children.insert(modified_child.children.begin(), modified_sibling.children.back());
        modified_sibling.children.pop_back();
        
        // Store modified pages in content storage
        content_storage.storePage(modified_child);
        content_storage.storePage(modified_sibling);
    }
}

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::borrowFromRight(std::shared_ptr<Page<KeyType>> parent, int index) {
    auto child_page = content_storage.getPage(parent->children[index]);
    auto sibling_page = content_storage.getPage(parent->children[index + 1]);
    
    if (!child_page || !sibling_page) {
        throw std::runtime_error("child or sibling page not found");
    }
    
    // Create copies to modify
    Page<KeyType> modified_child = *child_page;
    Page<KeyType> modified_sibling = *sibling_page;

    if (modified_child.is_leaf) { // If leaf, just borrow the first key from sibling
        modified_child.keys.push_back(modified_sibling.keys.front());
        
        // Borrow the corresponding data
        size_t value_size = sizeof(ValueType);
        modified_child.data.insert(modified_child.data.end(),
                          modified_sibling.data.begin(),
                          modified_sibling.data.begin() + value_size);
        
        modified_sibling.keys.erase(modified_sibling.keys.begin());
        modified_sibling.data.erase(modified_sibling.data.begin(), modified_sibling.data.begin() + value_size);
        parent->keys[index] = modified_sibling.keys.front();
        
        // Store modified pages in content storage
        content_storage.storePage(modified_child);
        content_storage.storePage(modified_sibling);
    } else { // If not leaf, borrow the first key and child pointer
        modified_child.keys.push_back(parent->keys[index]);
        parent->keys[index] = modified_sibling.keys.front();
        modified_sibling.keys.erase(modified_sibling.keys.begin());

        modified_child.children.push_back(modified_sibling.children.front());
        modified_sibling.children.erase(modified_sibling.children.begin());
        
        // Store modified pages in content storage
        content_storage.storePage(modified_child);
        content_storage.storePage(modified_sibling);
    }
}

// Merge two nodes
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::mergeNodes(std::shared_ptr<Page<KeyType>> parent, int index) {
    auto left_page = content_storage.getPage(parent->children[index]);
    auto right_page = content_storage.getPage(parent->children[index + 1]);
    
    if (!left_page || !right_page) {
        throw std::runtime_error("left or right page not found");
    }
    
    // Create copies to modify
    Page<KeyType> modified_left = *left_page;
    Page<KeyType> modified_right = *right_page;

    if (!modified_left.is_leaf) { // If not leaf, merge keys and children
        modified_left.keys.push_back(parent->keys[index]); // Move the parent key down
        modified_left.keys.insert(modified_left.keys.end(), modified_right.keys.begin(), modified_right.keys.end()); // Merge keys
        modified_left.children.insert(modified_left.children.end(), modified_right.children.begin(), modified_right.children.end());
    } else { // If leaf, merge keys and values
        modified_left.keys.insert(modified_left.keys.end(), modified_right.keys.begin(), modified_right.keys.end()); // Merge keys
        modified_left.data.insert(modified_left.data.end(), modified_right.data.begin(), modified_right.data.end());
    }

    parent->keys.erase(parent->keys.begin() + index); // Remove the parent key
    parent->children.erase(parent->children.begin() + index + 1); // Remove the right child
    
    // Store modified pages in content storage
    content_storage.storePage(modified_left);
}

// Public search method
template <typename KeyType, typename ValueType>
ValueType* BTree<KeyType, ValueType>::search(const KeyType& key) {
    if (!root) return nullptr;
    
    try {
        Page<KeyType> node = findKey(root, key);
        // Find the key in the node and return its value
        for (size_t i = 0; i < node.keys.size(); i++) {
            if (node.keys[i] == key) {
                // Deserialize the value from binary data
                size_t value_size = sizeof(ValueType);
                size_t offset = i * value_size;
                if (offset + value_size <= node.data.size()) {
                    ValueType* result = new ValueType();
                    std::memcpy(result, &node.data[offset], value_size);
                    return result;
                }
            }
        }
        return nullptr;
    } catch (const std::runtime_error& e) {
        return nullptr;
    }
}

// Print storage statistics
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::printStorageStats() const {
    content_storage.printStats();
}

// Explicit template instantiations
template class BTree<int, std::string>;
template class BTree<std::string, std::string>;
template class BTree<int, int>;
