#include "btree.h"
#include "fraction.h"
#include <cstring>

/*
 * BTree Constructor Implementation
 */
template <typename KeyType, typename ValueType>
BTree<KeyType, ValueType>::BTree(int maxKeys) : maxKeysPerNode(maxKeys) {
    // Initially, the tree is empty, so we create a root node
    // and mark it as a leaf (all data starts at the leaf level in B+ Trees)
    Page<KeyType> root_page = createPage<KeyType>(true);
    uint16_t root_id = content_storage.storePage(root_page);
    root = new Page<KeyType>(*content_storage.getPage(root_id));
}

/*
 * Example Method to Insert Key-Value Pairs (Placeholder)
 */
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::insert(const KeyType& key, const ValueType& value) {
    if (!root) { // If tree is empty, create a new root
        Page<KeyType> root_page = createPage<KeyType>(true);
        uint16_t root_id = content_storage.storePage(root_page);
        root = new Page<KeyType>(*content_storage.getPage(root_id));
    } else if (root->keys.size() == maxKeysPerNode) { // Check if the root is full
        Page<KeyType> new_root_page = createPage<KeyType>(false);
        new_root_page.children.push_back(root->header.page_id); // Page ID of the old root
        splitChild(&new_root_page, 0, root); // Split child bc of overflow
        
        // Store the new root in content storage
        uint16_t new_root_id = content_storage.storePage(new_root_page);
        root = new Page<KeyType>(*content_storage.getPage(new_root_id));
    }
    // Now the root is guaranteed to not be empty
    insertNonFull(root, key, value); // Insert
   
}

// Find the node that has a key
template <typename KeyType, typename ValueType>
Page<KeyType> BTree<KeyType, ValueType>::findKey(Page<KeyType>* node, const KeyType& key){
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
            // Load child page from content storage
            auto child_page = content_storage.getPage(node->children[idx]);
            if (child_page) {
                return findKey(child_page.get(), key);
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
void BTree<KeyType, ValueType>::insertNonFull(Page<KeyType>* node, const KeyType& key, const ValueType& value) {
    int i = node->keys.size() - 1; // Start from the last key

    if (node->is_leaf) { // If its a leaf node, insert the key and value
        // Insert in sorted order
        node->keys.push_back(key);
        
        // Serialize the value to binary data
        std::vector<uint8_t> serialized_value;
        const uint8_t* value_bytes = reinterpret_cast<const uint8_t*>(&value);
        serialized_value.assign(value_bytes, value_bytes + sizeof(ValueType));
        
        // Insert the serialized value into the data vector
        node->data.insert(node->data.end(), serialized_value.begin(), serialized_value.end());
        
        // Sort both keys and data together
        for (int j = node->keys.size() - 1; j > 0 && node->keys[j] < node->keys[j - 1]; --j) {
            std::swap(node->keys[j], node->keys[j - 1]); // Swap keys
            
            // Swap the corresponding data blocks
            size_t value_size = sizeof(ValueType);
            size_t offset1 = j * value_size;
            size_t offset2 = (j - 1) * value_size;
            
            for (size_t k = 0; k < value_size; ++k) {
                std::swap(node->data[offset1 + k], node->data[offset2 + k]);
            }
        }
        
        // Store the modified page in content storage
        uint16_t new_page_id = content_storage.storePage(*node);
        node->header.page_id = new_page_id;
        
    } else {
        // Find child to descend into
        while (i >= 0 && key < node->keys[i])
            i--;
        i++;

        // Load child page from content storage
        auto child_page = content_storage.getPage(node->children[i]);
        if (!child_page) {
            throw std::runtime_error("child page not found");
        }
        
        if (child_page->keys.size() == maxKeysPerNode) { // If the child is full, split it
            splitChild(node, i, child_page.get());
            if (key > node->keys[i]) i++; // Check which child to go to after split
        }

        // Recursively insert into child
        insertNonFull(child_page.get(), key, value);
    }
}

// Function to split a child node in case its full. helper for insert
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::splitChild(Page<KeyType>* parent, int index, Page<KeyType>* child) {
    int mid = maxKeysPerNode / 2; // Remember b+tree property

    Page<KeyType> new_child_page = createPage<KeyType>(child->is_leaf);

    // Copy second half of keys/values to the new node
    new_child_page.keys.assign(child->keys.begin() + mid + 1, child->keys.end()); // Copy keys
    child->keys.resize(mid); // Keep the mid key in left for b+ tree

    if (child->is_leaf) { // If its a leaf, assign values
        size_t value_size = sizeof(ValueType);
        size_t start_offset = (mid + 1) * value_size;
        new_child_page.data.assign(child->data.begin() + start_offset, child->data.end());
        child->data.resize((mid + 1) * value_size);  // Keep the mid key in left for b+ tree
    } else { // If not leaf, copy children
        new_child_page.children.assign(child->children.begin() + mid + 1, child->children.end());
        child->children.resize(mid + 1);
    }

    // Store both modified pages in content storage
    uint16_t child_id = content_storage.storePage(*child);
    uint16_t new_child_id = content_storage.storePage(new_child_page);
    
    // Update parent
    parent->children.insert(parent->children.begin() + index + 1, new_child_id); // Insert new child page ID
    parent->keys.insert(parent->keys.begin() + index, child->keys[mid]); // Insert the mid key into parent
    
    child->header.page_id = child_id;
}

// Helper function to delete a key
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::deleteKey(const KeyType& key) {
    if (!root) return; // If tree is empty, nothing to delete
    deleteFromNode(root, key); // Delete fromthe node

    // If root is now empty and has a child, make child the new root
    if (!root->is_leaf && root->keys.empty()) {
        Page<KeyType>* oldRoot = root; // Store old root
        auto child_page = content_storage.getPage(root->children[0]);
        if (child_page) {
            root = new Page<KeyType>(*child_page);
        } else {
            root = new Page<KeyType>(createPage<KeyType>(true)); // For now, create new empty root
        }
        delete oldRoot; // Free the old root
    }
}

// Helper function to delete key from node
// Pretty complex, but it handles the case of underflow and merging nodes. Search, delete
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::deleteFromNode(Page<KeyType>* node, const KeyType& key) {
    size_t idx = 0; // Index to find the key
    while (idx < node->keys.size() && key > node->keys[idx]) { // Traverse to find key
        idx++;
    }

    if (node->is_leaf) { // If leaf node, just delete the key
        if (idx < node->keys.size() && node->keys[idx] == key) {
            node->keys.erase(node->keys.begin() + idx); // Remove the key
            
            // Remove the corresponding data block
            size_t value_size = sizeof(ValueType);
            size_t start_offset = idx * value_size;
            node->data.erase(node->data.begin() + start_offset, node->data.begin() + start_offset + value_size);
            
            // Store the modified page in content storage
            uint16_t new_page_id = content_storage.storePage(*node);
            node->header.page_id = new_page_id;
        } else {
            // Key not found
            return;
        }
    } else { // If not leaf, need to find the child to delete from
        if (idx < node->keys.size() && node->keys[idx] == key) {
            idx++; // move to child that might have key
        }

        // Load child page from content storage
        auto child_page = content_storage.getPage(node->children[idx]);
        if (!child_page) {
            throw std::runtime_error("child page not found");
        }
        
        deleteFromNode(child_page.get(), key); // Delete from child

        // Fix underflow (not enough keys in child)
        if (child_page->keys.size() < (maxKeysPerNode + 1) / 2) {
            // For now, just leave as is since we don't have proper sibling handling
        }
    }
}

// Borrow from left and right siblings

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::borrowFromLeft(Page<KeyType>* parent, int index) {
    auto child_page = content_storage.getPage(parent->children[index]);
    auto sibling_page = content_storage.getPage(parent->children[index - 1]);
    
    if (!child_page || !sibling_page) {
        throw std::runtime_error("child or sibling page not found");
    }
    
    Page<KeyType>* child = child_page.get();
    Page<KeyType>* sibling = sibling_page.get();

    if (child->is_leaf) { // If leaf, just borrow the last key from sibling
        child->keys.insert(child->keys.begin(), sibling->keys.back()); // Insert at the beginning
        
        // Borrow the corresponding data
        size_t value_size = sizeof(ValueType);
        size_t sibling_offset = (sibling->keys.size() - 1) * value_size;
        child->data.insert(child->data.begin(), 
                          sibling->data.begin() + sibling_offset,
                          sibling->data.begin() + sibling_offset + value_size);
        
        sibling->keys.pop_back(); // Remove the last key from sibling
        sibling->data.resize(sibling->data.size() - value_size); // Remove the last value from sibling
        parent->keys[index - 1] = child->keys[0]; // Update the parent key
        
        // Store modified pages in content storage
        uint16_t child_id = content_storage.storePage(*child);
        uint16_t sibling_id = content_storage.storePage(*sibling);
        child->header.page_id = child_id;
        sibling->header.page_id = sibling_id;
    } else { // If not leaf, borrow the last key and child pointer
        child->keys.insert(child->keys.begin(), parent->keys[index - 1]);
        parent->keys[index - 1] = sibling->keys.back(); // Update the parent key
        sibling->keys.pop_back(); // Remove the last key from sibling

        child->children.insert(child->children.begin(), sibling->children.back());
        sibling->children.pop_back();
        
        // Store modified pages in content storage
        uint16_t child_id = content_storage.storePage(*child);
        uint16_t sibling_id = content_storage.storePage(*sibling);
        child->header.page_id = child_id;
        sibling->header.page_id = sibling_id;
    }
}

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::borrowFromRight(Page<KeyType>* parent, int index) {
    auto child_page = content_storage.getPage(parent->children[index]);
    auto sibling_page = content_storage.getPage(parent->children[index + 1]);
    
    if (!child_page || !sibling_page) {
        throw std::runtime_error("child or sibling page not found");
    }
    
    Page<KeyType>* child = child_page.get();
    Page<KeyType>* sibling = sibling_page.get();

    if (child->is_leaf) { // If leaf, just borrow the first key from sibling
        child->keys.push_back(sibling->keys.front());
        
        // Borrow the corresponding data
        size_t value_size = sizeof(ValueType);
        child->data.insert(child->data.end(),
                          sibling->data.begin(),
                          sibling->data.begin() + value_size);
        
        sibling->keys.erase(sibling->keys.begin());
        sibling->data.erase(sibling->data.begin(), sibling->data.begin() + value_size);
        parent->keys[index] = sibling->keys.front();
        
        // Store modified pages in content storage
        uint16_t child_id = content_storage.storePage(*child);
        uint16_t sibling_id = content_storage.storePage(*sibling);
        child->header.page_id = child_id;
        sibling->header.page_id = sibling_id;
    } else { // If not leaf, borrow the first key and child pointer
        child->keys.push_back(parent->keys[index]);
        parent->keys[index] = sibling->keys.front();
        sibling->keys.erase(sibling->keys.begin());

        child->children.push_back(sibling->children.front());
        sibling->children.erase(sibling->children.begin());
        
        // Store modified pages in content storage
        uint16_t child_id = content_storage.storePage(*child);
        uint16_t sibling_id = content_storage.storePage(*sibling);
        child->header.page_id = child_id;
        sibling->header.page_id = sibling_id;
    }
}

// Merge two nodes
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::mergeNodes(Page<KeyType>* parent, int index) {
    auto left_page = content_storage.getPage(parent->children[index]);
    auto right_page = content_storage.getPage(parent->children[index + 1]);
    
    if (!left_page || !right_page) {
        throw std::runtime_error("left or right page not found");
    }
    
    Page<KeyType>* left = left_page.get();
    Page<KeyType>* right = right_page.get();

    if (!left->is_leaf) { // If not leaf, merge keys and children
        left->keys.push_back(parent->keys[index]); // Move the parent key down
        left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end()); // Merge keys
        left->children.insert(left->children.end(), right->children.begin(), right->children.end());
    } else { // If leaf, merge keys and values
        left->keys.insert(left->keys.end(), right->keys.begin(), right->keys.end()); // Merge keys
        left->data.insert(left->data.end(), right->data.begin(), right->data.end());
    }

    parent->keys.erase(parent->keys.begin() + index); // Remove the parent key
    parent->children.erase(parent->children.begin() + index + 1); // Remove the right child
    
    // Store modified pages in content storage
    uint16_t left_id = content_storage.storePage(*left);
    left->header.page_id = left_id;
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
