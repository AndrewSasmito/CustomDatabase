#include "btree.h"
#include "fraction.h"

/*
 * BTree Constructor Implementation
 */
template <typename KeyType, typename ValueType>
BTree<KeyType, ValueType>::BTree(int maxKeys) : maxKeysPerNode(maxKeys) {
    // Initially, the tree is empty, so we create a root node
    // and mark it as a leaf (all data starts at the leaf level in B+ Trees)
    root = createPage<KeyType>(true);
}

/*
 * Example Method to Insert Key-Value Pairs (Placeholder)
 */
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::insert(const KeyType& key, ValueType* value) {
    if (!root) { // If tree is empty, create a new root
        root = createPage<KeyType>(true);
    } else if (root->keys.size() == maxKeysPerNode) { // Check if the root is full
        Page<KeyType>* newRoot = createPage<KeyType>(true);
        newRoot->children.push_back(root); // Make the old root a child of the new root
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
            return node;
        } else {
            // Key not found
            throw std::runtime_error("key not found");
        }
    } else { // If not leaf, need to find:w
    //  the child
        if (idx < node->keys.size() && node->keys[idx] == key) {
            idx++; // move to child that might have key
        }
        return node;     
    }
}
// Function that traverses tree and inserts into a node that isnt full. helper for insert
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::insertNonFull(Page<KeyType>* node, const KeyType& key, ValueType* value) {
    int i = node->keys.size() - 1; // Start from the last key

    if (node->is_leaf) { // If its a leaf node, insert the key and value
        // Insert in sorted order
        node->keys.push_back(key);
        node->data.push_back(value);
        // Sort both keys and data together
        for (int j = node->keys.size() - 1; j > 0 && node->keys[j] < node->keys[j - 1]; --j) {
            std::swap(node->keys[j], node->keys[j - 1]); // Swap keys
            std::swap(node->data[j], node->data[j - 1]); // Swap values
        }
    } else {
        // Find child to descend into
        while (i >= 0 && key < node->keys[i])
            i--;
        i++;

        if (node->children[i]->keys.size() == maxKeysPerNode) { // If the child is full, split it
            splitChild(node, i, node->children[i]);
            if (key > node->keys[i]) i++; // Check which child to go to after split
        }

        insertNonFull(node->children[i], key, value); // Use recursion to insert in the child
    }
}

// Function to split a child node in case its full. helper for insert
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::splitChild(Page<KeyType>* parent, int index, Page<KeyType>* child) {
    int mid = maxKeysPerNode / 2; // Remember b+tree property

    Page<KeyType>* newChild = createPage<KeyType>(child->is_leaf);

    // Copy second half of keys/values to the new node
    newChild->keys.assign(child->keys.begin() + mid + 1, child->keys.end()); // Copy keys
    child->keys.resize(mid); // Keep the mid key in left for b+ tree

    if (child->is_leaf) { // If its a lef, assign values
        newChild->data.assign(child->data.begin() + mid + 1, child->data.end());
        child->data.resize(mid + 1);  // Keep the mid key in left for b+ tree
    } else { // If not leaf, copy children
        newChild->children.assign(child->children.begin() + mid + 1, child->children.end());
        child->children.resize(mid + 1);
    }

    // Insert new child into parent
    parent->children.insert(parent->children.begin() + index + 1, newChild); // Insert new child
    parent->keys.insert(parent->keys.begin() + index, child->keys[mid]); // Insert the mid key into parent
}

// Helper function to delete a key
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::deleteKey(const KeyType& key) {
    if (!root) return; // If tree is empty, nothing to delete
    deleteFromNode(root, key); // Delete fromthe node

    // If root is now empty and has a child, make child the new root
    if (!root->is_leaf && root->keys.empty()) {
        Page<KeyType>* oldRoot = root; // Store old root
        root = root->children[0]; // Update root to be the first child
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
            node->data.erase(node->data.begin() + idx); // Remove the value
        } else {
            // Key not found
            return;
        }
    } else { // If not leaf, need to find the child to delete from
        if (idx < node->keys.size() && node->keys[idx] == key) {
            idx++; // move to child that might have key
        }

        Page<KeyType>* child = node->children[idx]; // Get child
        deleteFromNode(child, key); // Delete from child

        // Fix underflow (not enough keys in child)
        if (child->keys.size() < (maxKeysPerNode + 1) / 2) {
            if (idx > 0 && node->children[idx - 1]->keys.size() > (maxKeysPerNode + 1) / 2) {
                borrowFromLeft(node, idx); // In this implementation we have to borrow the nodes
            } else if (idx < node->children.size() - 1 &&
                       node->children[idx + 1]->keys.size() > (maxKeysPerNode + 1) / 2) {
                borrowFromRight(node, idx); // Borrow from right sibling
            } else { // Merge with sibling
                if (idx > 0) {
                    mergeNodes(node, idx - 1); // Merge with left sibling
                } else {
                    mergeNodes(node, idx); // Merge with right sibling
                }
            }
        }
    }
}

// Borrow from left and right siblings

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::borrowFromLeft(Page<KeyType>* parent, int index) {
    Page<KeyType>* child = parent->children[index]; // Child that needs to borrow
    Page<KeyType>* sibling = parent->children[index - 1]; // Sibling to borrow from

    if (child->is_leaf) { // If leaf, just borrow the last key from sibling
        child->keys.insert(child->keys.begin(), sibling->keys.back()); // Insert at the beginning
        child->data.insert(child->data.begin(), sibling->data.back());
        sibling->keys.pop_back(); // Remove the last key from sibling
        sibling->data.pop_back(); // Remove the last value from sibling
        parent->keys[index - 1] = child->keys[0]; // Update the parent key
    } else { // If not leaf, borrow the last key and child pointer
        child->keys.insert(child->keys.begin(), parent->keys[index - 1]);
        parent->keys[index - 1] = sibling->keys.back(); // Update the parent key
        sibling->keys.pop_back(); // Remove the last key from sibling

        child->children.insert(child->children.begin(), sibling->children.back());
        sibling->children.pop_back();
    }
}

template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::borrowFromRight(Page<KeyType>* parent, int index) {
    Page<KeyType>* child = parent->children[index]; // Child that needs to borrow
    Page<KeyType>* sibling = parent->children[index + 1]; // Sibling to borrow from

    if (child->is_leaf) { // If leaf, just borrow the first key from sibling
        child->keys.push_back(sibling->keys.front());
        child->data.push_back(sibling->data.front());
        sibling->keys.erase(sibling->keys.begin());
        sibling->data.erase(sibling->data.begin());
        parent->keys[index] = sibling->keys.front();
    } else { // If not leaf, borrow the first key and child pointer
        child->keys.push_back(parent->keys[index]);
        parent->keys[index] = sibling->keys.front();
        sibling->keys.erase(sibling->keys.begin());

        child->children.push_back(sibling->children.front());
        sibling->children.erase(sibling->children.begin());
    }
}

// Merge two nodes
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::mergeNodes(Page<KeyType>* parent, int index) {
    Page<KeyType>* left = parent->children[index]; // Left child
    Page<KeyType>* right = parent->children[index + 1]; // Right child

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
}
