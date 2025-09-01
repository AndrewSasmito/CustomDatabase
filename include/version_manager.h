#pragma once
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <memory>
#include "page_manager.h"

// Transaction timestamp for MVCC
using TransactionId = uint64_t;
using Timestamp = std::chrono::steady_clock::time_point;

template<typename KeyType>
struct VersionedRecord {
    KeyType key;
    std::vector<uint8_t> data;
    TransactionId created_by;
    TransactionId deleted_by;  // 0 if not deleted
    Timestamp created_at;
    Timestamp deleted_at;
    bool is_deleted;
    
    VersionedRecord(const KeyType& k, const std::vector<uint8_t>& d, TransactionId txn_id)
        : key(k), data(d), created_by(txn_id), deleted_by(0), 
          created_at(std::chrono::steady_clock::now()), is_deleted(false) {}
};

template<typename KeyType>
struct Transaction {
    TransactionId id;
    Timestamp start_time;
    Timestamp commit_time;
    bool is_committed;
    bool is_aborted;
    std::vector<KeyType> read_set;
    std::vector<KeyType> write_set;
    
    Transaction(TransactionId txn_id) 
        : id(txn_id), start_time(std::chrono::steady_clock::now()),
          is_committed(false), is_aborted(false) {}
};

template<typename KeyType>
class VersionManager {
private:
    // Version storage: key -> list of versions (newest first)
    std::unordered_map<KeyType, std::vector<std::shared_ptr<VersionedRecord<KeyType>>>> versions;
    
    // Active transactions
    std::unordered_map<TransactionId, std::shared_ptr<Transaction<KeyType>>> active_transactions;
    std::unordered_map<TransactionId, std::shared_ptr<Transaction<KeyType>>> committed_transactions;
    
    // Transaction ID generation
    std::atomic<TransactionId> next_transaction_id;
    
    // Version cleanup tracking
    std::atomic<size_t> total_versions;
    std::atomic<size_t> cleaned_versions;
    Timestamp last_cleanup;
    
    // Synchronization
    mutable std::mutex versions_mutex;
    mutable std::mutex transactions_mutex;
    
    // Configuration
    std::chrono::hours version_retention_period;
    size_t max_versions_per_key;
    
    // Helper methods
    bool isVisible(const std::shared_ptr<VersionedRecord<KeyType>>& version, TransactionId reader_txn) const;
    std::shared_ptr<VersionedRecord<KeyType>> findVisibleVersion(const KeyType& key, TransactionId reader_txn) const;
    
public:
    VersionManager(std::chrono::hours retention = std::chrono::hours(24), size_t max_versions = 100);
    ~VersionManager();
    
    // Transaction management
    TransactionId beginTransaction();
    bool commitTransaction(TransactionId txn_id);
    bool abortTransaction(TransactionId txn_id);
    bool isTransactionActive(TransactionId txn_id) const;
    
    // MVCC operations
    bool insert(TransactionId txn_id, const KeyType& key, const std::vector<uint8_t>& data);
    bool update(TransactionId txn_id, const KeyType& key, const std::vector<uint8_t>& new_data);
    bool remove(TransactionId txn_id, const KeyType& key);
    std::shared_ptr<VersionedRecord<KeyType>> read(TransactionId txn_id, const KeyType& key);
    
    // Version cleanup
    size_t cleanupOldVersions();
    size_t cleanupAbortedTransactions();
    bool canCleanupVersion(const std::shared_ptr<VersionedRecord<KeyType>>& version) const;
    
    // Statistics and monitoring
    struct VersionStats {
        size_t total_versions;
        size_t active_transactions;
        size_t committed_transactions;
        size_t versions_per_key_avg;
        size_t cleaned_versions;
        double cleanup_efficiency;
        std::chrono::steady_clock::time_point last_cleanup_time;
    };
    
    VersionStats getStats() const;
    void printStats() const;
    
    // Configuration
    void setRetentionPeriod(std::chrono::hours period);
    void setMaxVersionsPerKey(size_t max_versions);
};
