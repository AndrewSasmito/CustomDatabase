#pragma once

#include<unordered_map>
#include<vector>
#include<variant>
#include<string>
#include "fraction.h"
#include "page_manager.h"

/*
*   BTree that stores the BTreeNodes, ensures it is balanced
*
*/
template <typename KeyType, typename ValueType>
class BTree {
    private:
        Page<KeyType>* root;
        int maxKeysPerNode;  // Maximum keys in each node
        void insertNonFull(Page<KeyType>* root, const KeyType& key, ValueType* value);
        void splitChild(Page<KeyType>* parent, int index, Page<KeyType>* child);

        void deleteFromNode(Page<KeyType>* node, const KeyType& key);
        void borrowFromLeft(Page<KeyType>* parent, int index);
        void borrowFromRight(Page<KeyType>* parent, int index);
        void mergeNodes(Page<KeyType>* parent, int index);

    public:
        BTree(int maxKeys);  // Constructor declaration
        void insert(const KeyType& key, ValueType* value);
        void deleteKey(const KeyType& key);

};
