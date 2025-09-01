#include "version_manager.h"
#include <iostream>
#include <algorithm>

template<typename KeyType>
VersionManager<KeyType>::VersionManager(std::chrono::hours retention, size_t max_versions)
    : next_transaction_id(1), total_versions(0), cleaned_versions(0),
      last_cleanup(std::chrono::steady_clock::now()),
      version_retention_period(retention), max_versions_per_key(max_versions) {
    
    std::cout << "VersionManager: Initialized with " << retention.count() 
              << "h retention, max " << max_versions << " versions per key" << std::endl;
}

template<typename KeyType>
VersionManager<KeyType>::~VersionManager() {
    // Clean up remaining transactions
    std::lock_guard<std::mutex> lock(transactions_mutex);
    for (auto& [txn_id, txn] : active_transactions) {
        if (!txn->is_committed && !txn->is_aborted) {
            txn->is_aborted = true;
        }
    }
}

template<typename KeyType>
TransactionId VersionManager<KeyType>::beginTransaction() {
    TransactionId txn_id = next_transaction_id.fetch_add(1);
    
    auto transaction = std::make_shared<Transaction<KeyType>>(txn_id);
    
    {
        std::lock_guard<std::mutex> lock(transactions_mutex);
        active_transactions[txn_id] = transaction;
    }
    
    std::cout << "VersionManager: Started transaction " << txn_id << std::endl;
    return txn_id;
}

template<typename KeyType>
bool VersionManager<KeyType>::commitTransaction(TransactionId txn_id) {
    std::lock_guard<std::mutex> lock(transactions_mutex);
    
    auto it = active_transactions.find(txn_id);
    if (it == active_transactions.end()) {
        std::cerr << "VersionManager: Transaction " << txn_id << " not found" << std::endl;
        return false;
    }
    
    auto& txn = it->second;
    txn->is_committed = true;
    txn->commit_time = std::chrono::steady_clock::now();
    
    // Move to committed transactions
    committed_transactions[txn_id] = txn;
    active_transactions.erase(it);
    
    std::cout << "VersionManager: Committed transaction " << txn_id << std::endl;
    return true;
}

template<typename KeyType>
bool VersionManager<KeyType>::abortTransaction(TransactionId txn_id) {
    std::lock_guard<std::mutex> lock(transactions_mutex);
    
    auto it = active_transactions.find(txn_id);
    if (it == active_transactions.end()) {
        return false;
    }
    
    auto& txn = it->second;
    txn->is_aborted = true;
    
    active_transactions.erase(it);
    
    std::cout << "VersionManager: Aborted transaction " << txn_id << std::endl;
    return true;
}

template<typename KeyType>
bool VersionManager<KeyType>::isTransactionActive(TransactionId txn_id) const {
    std::lock_guard<std::mutex> lock(transactions_mutex);
    return active_transactions.find(txn_id) != active_transactions.end();
}

template<typename KeyType>
bool VersionManager<KeyType>::insert(TransactionId txn_id, const KeyType& key, const std::vector<uint8_t>& data) {
    if (!isTransactionActive(txn_id)) {
        std::cerr << "VersionManager: Transaction " << txn_id << " not active" << std::endl;
        return false;
    }
    
    auto version = std::make_shared<VersionedRecord<KeyType>>(key, data, txn_id);
    
    {
        std::lock_guard<std::mutex> lock(versions_mutex);
        versions[key].insert(versions[key].begin(), version);
        total_versions.fetch_add(1);
    }
    
    // Add to transaction's write set
    {
        std::lock_guard<std::mutex> lock(transactions_mutex);
        auto it = active_transactions.find(txn_id);
        if (it != active_transactions.end()) {
            it->second->write_set.push_back(key);
        }
    }
    
    return true;
}

template<typename KeyType>
bool VersionManager<KeyType>::update(TransactionId txn_id, const KeyType& key, const std::vector<uint8_t>& new_data) {
    if (!isTransactionActive(txn_id)) {
        return false;
    }
    
    // Create new version
    auto new_version = std::make_shared<VersionedRecord<KeyType>>(key, new_data, txn_id);
    
    {
        std::lock_guard<std::mutex> lock(versions_mutex);
        versions[key].insert(versions[key].begin(), new_version);
        total_versions.fetch_add(1);
    }
    
    // Add to transaction's write set
    {
        std::lock_guard<std::mutex> lock(transactions_mutex);
        auto it = active_transactions.find(txn_id);
        if (it != active_transactions.end()) {
            it->second->write_set.push_back(key);
        }
    }
    
    return true;
}

template<typename KeyType>
bool VersionManager<KeyType>::remove(TransactionId txn_id, const KeyType& key) {
    if (!isTransactionActive(txn_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(versions_mutex);
    
    auto it = versions.find(key);
    if (it == versions.end() || it->second.empty()) {
        return false;
    }
    
    // Find the visible version and mark it as deleted
    for (auto& version : it->second) {
        if (isVisible(version, txn_id) && !version->is_deleted) {
            version->is_deleted = true;
            version->deleted_by = txn_id;
            version->deleted_at = std::chrono::steady_clock::now();
            return true;
        }
    }
    
    return false;
}

template<typename KeyType>
std::shared_ptr<VersionedRecord<KeyType>> VersionManager<KeyType>::read(TransactionId txn_id, const KeyType& key) {
    // Add to transaction's read set
    {
        std::lock_guard<std::mutex> lock(transactions_mutex);
        auto it = active_transactions.find(txn_id);
        if (it != active_transactions.end()) {
            it->second->read_set.push_back(key);
        }
    }
    
    return findVisibleVersion(key, txn_id);
}

template<typename KeyType>
bool VersionManager<KeyType>::isVisible(const std::shared_ptr<VersionedRecord<KeyType>>& version, TransactionId reader_txn) const {
    // Version is visible if:
    // 1. It was created by a committed transaction that committed before reader started
    // 2. OR it was created by the reader transaction itself
    // 3. AND it's not deleted by a committed transaction that committed before reader started
    
    if (version->created_by == reader_txn) {
        return !version->is_deleted || version->deleted_by == reader_txn;
    }
    
    // Check if creating transaction is committed
    auto creator_it = committed_transactions.find(version->created_by);
    if (creator_it == committed_transactions.end()) {
        return false; // Creator not committed
    }
    
    // Check if deleted by a committed transaction
    if (version->is_deleted && version->deleted_by != reader_txn) {
        auto deleter_it = committed_transactions.find(version->deleted_by);
        if (deleter_it != committed_transactions.end()) {
            return false; // Deleted by committed transaction
        }
    }
    
    return true;
}

template<typename KeyType>
std::shared_ptr<VersionedRecord<KeyType>> VersionManager<KeyType>::findVisibleVersion(const KeyType& key, TransactionId reader_txn) const {
    std::lock_guard<std::mutex> lock(versions_mutex);
    
    auto it = versions.find(key);
    if (it == versions.end()) {
        return nullptr;
    }
    
    // Find the newest visible version
    for (const auto& version : it->second) {
        if (isVisible(version, reader_txn)) {
            return version;
        }
    }
    
    return nullptr;
}

template<typename KeyType>
size_t VersionManager<KeyType>::cleanupOldVersions() {
    std::lock_guard<std::mutex> versions_lock(versions_mutex);
    std::lock_guard<std::mutex> txn_lock(transactions_mutex);
    
    size_t cleaned = 0;
    auto cutoff_time = std::chrono::steady_clock::now() - version_retention_period;
    
    for (auto& [key, version_list] : versions) {
        auto it = version_list.begin();
        size_t kept = 0;
        
        while (it != version_list.end()) {
            // Always keep at least one version
            if (kept == 0) {
                ++it;
                ++kept;
                continue;
            }
            
            // Remove if too old and can be safely cleaned
            if ((*it)->created_at < cutoff_time && canCleanupVersion(*it)) {
                it = version_list.erase(it);
                cleaned++;
            } else if (kept >= max_versions_per_key && canCleanupVersion(*it)) {
                // Remove excess versions
                it = version_list.erase(it);
                cleaned++;
            } else {
                ++it;
                ++kept;
            }
        }
    }
    
    cleaned_versions.fetch_add(cleaned);
    last_cleanup = std::chrono::steady_clock::now();
    
    if (cleaned > 0) {
        std::cout << "VersionManager: Cleaned up " << cleaned << " old versions" << std::endl;
    }
    
    return cleaned;
}

template<typename KeyType>
size_t VersionManager<KeyType>::cleanupAbortedTransactions() {
    std::lock_guard<std::mutex> versions_lock(versions_mutex);
    std::lock_guard<std::mutex> txn_lock(transactions_mutex);
    
    size_t cleaned = 0;
    
    // Find aborted transaction IDs
    std::vector<TransactionId> aborted_txns;
    for (const auto& [txn_id, txn] : active_transactions) {
        if (txn->is_aborted) {
            aborted_txns.push_back(txn_id);
        }
    }
    
    // Remove versions created by aborted transactions
    for (auto& [key, version_list] : versions) {
        auto it = version_list.begin();
        while (it != version_list.end()) {
            if (std::find(aborted_txns.begin(), aborted_txns.end(), (*it)->created_by) != aborted_txns.end()) {
                it = version_list.erase(it);
                cleaned++;
            } else {
                ++it;
            }
        }
    }
    
    // Remove aborted transactions
    for (TransactionId txn_id : aborted_txns) {
        active_transactions.erase(txn_id);
    }
    
    if (cleaned > 0) {
        std::cout << "VersionManager: Cleaned up " << cleaned << " versions from aborted transactions" << std::endl;
    }
    
    return cleaned;
}

template<typename KeyType>
bool VersionManager<KeyType>::canCleanupVersion(const std::shared_ptr<VersionedRecord<KeyType>>& version) const {
    // Can cleanup if no active transaction could potentially read this version
    // For simplicity just check if the creating transaction is committed
    return committed_transactions.find(version->created_by) != committed_transactions.end();
}

template<typename KeyType>
typename VersionManager<KeyType>::VersionStats VersionManager<KeyType>::getStats() const {
    std::lock_guard<std::mutex> versions_lock(versions_mutex);
    std::lock_guard<std::mutex> txn_lock(transactions_mutex);
    
    size_t total_keys = versions.size();
    size_t avg_versions = total_keys > 0 ? total_versions.load() / total_keys : 0;
    double cleanup_efficiency = total_versions.load() > 0 ? 
        (double)cleaned_versions.load() / total_versions.load() * 100.0 : 0.0;
    
    return {
        total_versions.load(),
        active_transactions.size(),
        committed_transactions.size(),
        avg_versions,
        cleaned_versions.load(),
        cleanup_efficiency,
        last_cleanup
    };
}

template<typename KeyType>
void VersionManager<KeyType>::printStats() const {
    auto stats = getStats();
    
    std::cout << "\n=== Version Manager Statistics ===" << std::endl;
    std::cout << "Total versions: " << stats.total_versions << std::endl;
    std::cout << "Active transactions: " << stats.active_transactions << std::endl;
    std::cout << "Committed transactions: " << stats.committed_transactions << std::endl;
    std::cout << "Avg versions per key: " << stats.versions_per_key_avg << std::endl;
    std::cout << "Cleaned versions: " << stats.cleaned_versions << std::endl;
    std::cout << "Cleanup efficiency: " << stats.cleanup_efficiency << "%" << std::endl;
    std::cout << "==================================" << std::endl;
}

template<typename KeyType>
void VersionManager<KeyType>::setRetentionPeriod(std::chrono::hours period) {
    version_retention_period = period;
    std::cout << "VersionManager: Updated retention period to " << period.count() << " hours" << std::endl;
}

template<typename KeyType>
void VersionManager<KeyType>::setMaxVersionsPerKey(size_t max_versions) {
    max_versions_per_key = max_versions;
    std::cout << "VersionManager: Updated max versions per key to " << max_versions << std::endl;
}

// Explicit template instantiations
template class VersionManager<int>;
template class VersionManager<std::string>;
