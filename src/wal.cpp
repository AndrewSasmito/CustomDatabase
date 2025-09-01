#include "wal.h"
#include <iostream>
#include <iomanip>
#include <cstring>

template<typename KeyType>
WALManager<KeyType>::WALManager(const std::string& wal_path, size_t buffer_limit)
    : wal_file_path(wal_path), buffer_size_limit(buffer_limit),
      next_lsn(1), next_transaction_id(1), last_checkpoint_lsn(0) {
    
    wal_file.open(wal_file_path, std::ios::binary | std::ios::app);
    if (!wal_file.is_open()) {
        throw std::runtime_error("Failed to open WAL file: " + wal_file_path);
    }
    
    write_buffer.reserve(buffer_size_limit);
    
    std::cout << "WAL: Initialized with file " << wal_file_path << std::endl;
}

template<typename KeyType>
WALManager<KeyType>::~WALManager() {
    if (wal_file.is_open()) {
        flushBuffer();
        wal_file.close();
    }
}

template<typename KeyType>
uint32_t WALManager<KeyType>::calculateChecksum(const void* data, size_t size) {
    uint32_t checksum = 0;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        checksum = (checksum << 1) ^ bytes[i];
    }
    return checksum;
}

template<typename KeyType>
void WALManager<KeyType>::flushBuffer() {
    if (!write_buffer.empty()) {
        wal_file.write(reinterpret_cast<const char*>(write_buffer.data()), write_buffer.size());
        wal_file.flush();
        write_buffer.clear();
        std::cout << "WAL: Flushed buffer to disk" << std::endl;
    }
}

template<typename KeyType>
uint64_t WALManager<KeyType>::beginTransaction() {
    uint64_t txn_id = next_transaction_id.fetch_add(1);
    std::cout << "WAL: Started transaction " << txn_id << std::endl;
    return txn_id;
}

template<typename KeyType>
void WALManager<KeyType>::commitTransaction(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    uint64_t lsn = next_lsn.fetch_add(1);
    WALRecordHeader commit_record(WALRecordType::COMMIT, sizeof(WALRecordHeader), txn_id, lsn);
    commit_record.checksum = calculateChecksum(&commit_record, sizeof(commit_record) - sizeof(commit_record.checksum));
    
    const uint8_t* record_bytes = reinterpret_cast<const uint8_t*>(&commit_record);
    write_buffer.insert(write_buffer.end(), record_bytes, record_bytes + sizeof(commit_record));
    
    if (write_buffer.size() >= buffer_size_limit) {
        flushBuffer();
    }
    
    flushBuffer();
    
    std::cout << "WAL: Committed transaction " << txn_id << " (LSN: " << lsn << ")" << std::endl;
}

template<typename KeyType>
void WALManager<KeyType>::abortTransaction(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    uint64_t lsn = next_lsn.fetch_add(1);
    WALRecordHeader abort_record(WALRecordType::ABORT, sizeof(WALRecordHeader), txn_id, lsn);
    abort_record.checksum = calculateChecksum(&abort_record, sizeof(abort_record) - sizeof(abort_record.checksum));
    
    const uint8_t* record_bytes = reinterpret_cast<const uint8_t*>(&abort_record);
    write_buffer.insert(write_buffer.end(), record_bytes, record_bytes + sizeof(abort_record));
    
    std::cout << "WAL: Aborted transaction " << txn_id << " (LSN: " << lsn << ")" << std::endl;
}

template<typename KeyType>
uint64_t WALManager<KeyType>::logInsert(uint64_t txn_id, uint16_t page_id, const KeyType& key, 
                                        const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    uint64_t lsn = next_lsn.fetch_add(1);
    WALDataRecord<KeyType> record(WALRecordType::INSERT, txn_id, lsn, page_id, key);
    record.new_data = data;
    
    // Calculate actual record size including variable data
    record.header.record_size = sizeof(WALDataRecord<KeyType>) + data.size();
    record.header.checksum = calculateChecksum(&record, sizeof(record) - sizeof(record.header.checksum));
    
    // Serialize to buffer
    const uint8_t* record_bytes = reinterpret_cast<const uint8_t*>(&record);
    write_buffer.insert(write_buffer.end(), record_bytes, record_bytes + sizeof(record));
    
    write_buffer.insert(write_buffer.end(), data.begin(), data.end());
    
    if (write_buffer.size() >= buffer_size_limit) {
        flushBuffer();
    }
    
    std::cout << "WAL: Logged INSERT for key " << key << " (LSN: " << lsn << ")" << std::endl;
    return lsn;
}

template<typename KeyType>
uint64_t WALManager<KeyType>::logDelete(uint64_t txn_id, uint16_t page_id, const KeyType& key, 
                                        const std::vector<uint8_t>& old_data) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    uint64_t lsn = next_lsn.fetch_add(1);
    WALDataRecord<KeyType> record(WALRecordType::DELETE, txn_id, lsn, page_id, key);
    record.old_data = old_data;
    
    record.header.record_size = sizeof(WALDataRecord<KeyType>) + old_data.size();
    record.header.checksum = calculateChecksum(&record, sizeof(record) - sizeof(record.header.checksum));
    
    const uint8_t* record_bytes = reinterpret_cast<const uint8_t*>(&record);
    write_buffer.insert(write_buffer.end(), record_bytes, record_bytes + sizeof(record));
    write_buffer.insert(write_buffer.end(), old_data.begin(), old_data.end());
    
    if (write_buffer.size() >= buffer_size_limit) {
        flushBuffer();
    }
    
    std::cout << "WAL: Logged DELETE for key " << key << " (LSN: " << lsn << ")" << std::endl;
    return lsn;
}

template<typename KeyType>
uint64_t WALManager<KeyType>::logUpdate(uint64_t txn_id, uint16_t page_id, const KeyType& key,
                                        const std::vector<uint8_t>& old_data, 
                                        const std::vector<uint8_t>& new_data) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    uint64_t lsn = next_lsn.fetch_add(1);
    WALDataRecord<KeyType> record(WALRecordType::UPDATE, txn_id, lsn, page_id, key);
    record.old_data = old_data;
    record.new_data = new_data;
    
    record.header.record_size = sizeof(WALDataRecord<KeyType>) + old_data.size() + new_data.size();
    record.header.checksum = calculateChecksum(&record, sizeof(record) - sizeof(record.header.checksum));
    
    const uint8_t* record_bytes = reinterpret_cast<const uint8_t*>(&record);
    write_buffer.insert(write_buffer.end(), record_bytes, record_bytes + sizeof(record));
    write_buffer.insert(write_buffer.end(), old_data.begin(), old_data.end());
    write_buffer.insert(write_buffer.end(), new_data.begin(), new_data.end());
    
    if (write_buffer.size() >= buffer_size_limit) {
        flushBuffer();
    }
    
    std::cout << "WAL: Logged UPDATE for key " << key << " (LSN: " << lsn << ")" << std::endl;
    return lsn;
}

template<typename KeyType>
uint64_t WALManager<KeyType>::writeCheckpoint() {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    // Make sure to flush any pending writes first
    flushBuffer();
    
    uint64_t lsn = next_lsn.fetch_add(1);
    uint64_t checkpoint_txn = next_transaction_id.fetch_add(1);
    
    WALRecordHeader checkpoint_record(WALRecordType::CHECKPOINT, sizeof(WALRecordHeader), checkpoint_txn, lsn);
    checkpoint_record.checksum = calculateChecksum(&checkpoint_record, sizeof(checkpoint_record) - sizeof(checkpoint_record.checksum));
    
    // Write checkpoint record directly to file
    wal_file.write(reinterpret_cast<const char*>(&checkpoint_record), sizeof(checkpoint_record));
    wal_file.flush();
    
    last_checkpoint_lsn.store(lsn);
    
    std::cout << "WAL: Wrote checkpoint at LSN " << lsn << std::endl;
    return lsn;
}

template<typename KeyType>
void WALManager<KeyType>::sync() {
    std::lock_guard<std::mutex> lock(wal_mutex);
    flushBuffer();
}

template<typename KeyType>
void WALManager<KeyType>::truncate(uint64_t up_to_lsn) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    std::cout << "WAL: Truncated up to LSN " << up_to_lsn << std::endl;
}

template<typename KeyType>
size_t WALManager<KeyType>::getWALSize() const {
    std::ifstream file(wal_file_path, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        return file.tellg();
    }
    return 0;
}

template<typename KeyType>
void WALManager<KeyType>::replay(uint64_t from_lsn) {
    std::cout << "WAL: Replaying from LSN " << from_lsn << std::endl;
    // Recovery implementation would go here
}

template class WALManager<int>;
template class WALManager<std::string>;
