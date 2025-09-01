#pragma once

#include<unordered_map>
#include<vector>
#include<variant>
#include<string>
#include<memory>
#include "fraction.h"
#include "page_manager.h"
#include "content_storage.h"
#include "page_cache.h"
#include "writer_queue.h"
#include "wal.h"

/*
*   BTree that stores the BTreeNodes, ensures it is balanced
*
*/
template <typename KeyType, typename ValueType>
class BTree {
    private:
        std::shared_ptr<Page<KeyType>> root;
        int maxKeysPerNode;  // Maximum keys in each node
        ContentStorage<KeyType> content_storage;
        PageCache<KeyType> page_cache;
        WriterQueue<KeyType> writer_queue;
        WALManager<KeyType> wal_manager;
        uint64_t current_transaction;
        
        void insertNonFull(std::shared_ptr<Page<KeyType>> root, const KeyType& key, const ValueType& value);
        void splitChild(std::shared_ptr<Page<KeyType>> parent, int index, std::shared_ptr<Page<KeyType>> child);

        void deleteFromNode(std::shared_ptr<Page<KeyType>> node, const KeyType& key);
        void borrowFromLeft(std::shared_ptr<Page<KeyType>> parent, int index);
        void borrowFromRight(std::shared_ptr<Page<KeyType>> parent, int index);
        void mergeNodes(std::shared_ptr<Page<KeyType>> parent, int index);

    public:
        BTree(int maxKeys);
        ~BTree();
        void insert(const KeyType& key, const ValueType& value);
        void deleteKey(const KeyType& key);
        ValueType* search(const KeyType& key); // Public search method
        void printStorageStats() const;
        void flush(); // To flush all pending writes
        
        void beginTransaction();
        void commitTransaction();
        void abortTransaction();

        Page<KeyType> findKey(std::shared_ptr<Page<KeyType>> node, const KeyType& key);

};
