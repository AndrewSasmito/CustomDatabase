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
    root = new Page<KeyType>(createPage<KeyType>(true));
}

/*
 * Example Method to Insert Key-Value Pairs (Placeholder)
 */
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::insert(const KeyType& key, const ValueType& value) {
    if (!root) { // If tree is empty, create a new root
        root = new Page<KeyType>(createPage<KeyType>(true));
    } else if (root->keys.size() == maxKeysPerNode) { // Check if the root is full
        Page<KeyType>* newRoot = new Page<KeyType>(createPage<KeyType>(false));
        newRoot->children.push_back(0); // Page ID of the old root
        splitChild(newRoot, 0, root); // Split child bc of overflow
        root = newRoot; // Update the root to be the new node
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
            // For now, we'll just return the current node since we don't have page loading
            return *node;
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
        
        // Update content hash after modifying the page
        node->updateContentHash();
        
    } else {
        // Find child to descend into
        while (i >= 0 && key < node->keys[i])
            i--;
        i++;

        if (node->children[i] != 0 && node->children[i] < maxKeysPerNode) { // If the child is full, split it
            splitChild(node, i, root); // For now, just use root as placeholder
            if (key > node->keys[i]) i++; // Check which child to go to after split
        }

        insertNonFull(root, key, value); // Use recursion to insert in the child
    }
}

// Function to split a child node in case its full. helper for insert
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::splitChild(Page<KeyType>* parent, int index, Page<KeyType>* child) {
    int mid = maxKeysPerNode / 2; // Remember b+tree property

    Page<KeyType>* newChild = new Page<KeyType>(createPage<KeyType>(child->is_leaf));

    // Copy second half of keys/values to the new node
    newChild->keys.assign(child->keys.begin() + mid + 1, child->keys.end()); // Copy keys
    child->keys.resize(mid); // Keep the mid key in left for b+ tree

    if (child->is_leaf) { // If its a leaf, assign values
        size_t value_size = sizeof(ValueType);
        size_t start_offset = (mid + 1) * value_size;
        newChild->data.assign(child->data.begin() + start_offset, child->data.end());
        child->data.resize((mid + 1) * value_size);  // Keep the mid key in left for b+ tree
    } else { // If not leaf, copy children
        newChild->children.assign(child->children.begin() + mid + 1, child->children.end());
        child->children.resize(mid + 1);
    }

    // Insert new child into parent
    parent->children.insert(parent->children.begin() + index + 1, 1); // Insert new child page ID
    parent->keys.insert(parent->keys.begin() + index, child->keys[mid]); // Insert the mid key into parent
    
    child->updateContentHash();
    newChild->updateContentHash();
    parent->updateContentHash();
}

// Helper function to delete a key
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::deleteKey(const KeyType& key) {
    if (!root) return; // If tree is empty, nothing to delete
    deleteFromNode(root, key); // Delete fromthe node

    // If root is now empty and has a child, make child the new root
    if (!root->is_leaf && root->keys.empty()) {
        Page<KeyType>* oldRoot = root; // Store old root
        root = new Page<KeyType>(createPage<KeyType>(true)); // For now, create new empty root
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
            
            node->updateContentHash();
        } else {
            // Key not found
            return;
        }
    } else { // If not leaf, need to find the child to delete from
        if (idx < node->keys.size() && node->keys[idx] == key) {
            idx++; // move to child that might have key
        }

        // For now, just delete from root since we don't have proper child loading
        deleteFromNode(root, key); // Delete from child

        // Fix underflow (not enough keys in child)
        if (root->keys.size() < (maxKeysPerNode + 1) / 2) {
            // For now, just leave as is since we don't have proper sibling handling
        }
    }
}

// Borrow from left and right siblings

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::borrowFromLeft(Page<KeyType>* parent, int index) {
    Page<KeyType>* child = parent->children[index] == 0 ? root : root; // For now, use root
    Page<KeyType>* sibling = parent->children[index - 1] == 0 ? root : root; // For now, use root

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
        
        child->updateContentHash();
        sibling->updateContentHash();
        parent->updateContentHash();
    } else { // If not leaf, borrow the last key and child pointer
        child->keys.insert(child->keys.begin(), parent->keys[index - 1]);
        parent->keys[index - 1] = sibling->keys.back(); // Update the parent key
        sibling->keys.pop_back(); // Remove the last key from sibling

        child->children.insert(child->children.begin(), sibling->children.back());
        sibling->children.pop_back();
        
        child->updateContentHash();
        sibling->updateContentHash();
        parent->updateContentHash();
    }
}

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::borrowFromRight(Page<KeyType>* parent, int index) {
    Page<KeyType>* child = parent->children[index] == 0 ? root : root; // For now, use root
    Page<KeyType>* sibling = parent->children[index + 1] == 0 ? root : root; // For now, use root

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
        
        child->updateContentHash();
        sibling->updateContentHash();
        parent->updateContentHash();
    } else { // If not leaf, borrow the first key and child pointer
        child->keys.push_back(parent->keys[index]);
        parent->keys[index] = sibling->keys.front();
        sibling->keys.erase(sibling->keys.begin());

        child->children.push_back(sibling->children.front());
        sibling->children.erase(sibling->children.begin());
        
        child->updateContentHash();
        sibling->updateContentHash();
        parent->updateContentHash();
    }
}

// Merge two nodes
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::mergeNodes(Page<KeyType>* parent, int index) {
    Page<KeyType>* left = parent->children[index] == 0 ? root : root; // For now, use root
    Page<KeyType>* right = parent->children[index + 1] == 0 ? root : root; // For now, use root

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

    left->updateContentHash();
    parent->updateContentHash();
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

// Explicit template instantiations
template class BTree<int, std::string>;
template class BTree<std::string, std::string>;
template class BTree<int, int>;
