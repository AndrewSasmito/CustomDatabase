#include "btree.h"

/*
 * BTreeNode Constructor Implementation
 */
template <typename KeyType, typename ValueType>
BTreeNode<KeyType, ValueType>::BTreeNode(bool leaf) : is_leaf(leaf) {
    // Constructor implementation
}

/*
 * BTree Constructor Implementation
 */
template <typename KeyType, typename ValueType>
BTree<KeyType, ValueType>::BTree(int maxKeys) : maxKeysPerNode(maxKeys) {
    root = new BTreeNode<KeyType, ValueType>(true);  // Initially, root is a leaf
}

/*
 * Example Method to Insert Key-Value Pairs (Placeholder)
 */
template <typename KeyType, typename ValueType>
void BTree<KeyType, ValueType>::insert(const KeyType& key, ValueType* value) {
    // Method implementation for insert
    // Insert logic for the BTree would go here
}

// Explicit template instantiation (to generate the specific versions of the template classes)
template class BTreeNode<int, int>;
template class BTree<int, int>;
template class BTreeNode<std::string, int>;
template class BTree<std::string, int>;
template class BTreeNode<fraction, int>;
template class BTree<fraction, int>;
