#pragma once
#include <fstream>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <string>
#include <cstdint>

enum class WALRecordType : uint8_t {
    INSERT = 1,
    DELETE = 2,
    UPDATE = 3,
    CHECKPOINT = 4,
    COMMIT = 5,
    ABORT = 6
};

struct WALRecordHeader {
    WALRecordType type;
    uint32_t record_size;
    uint64_t transaction_id;
    uint64_t lsn;  // Log sequence number
    uint32_t checksum;
    std::chrono::steady_clock::time_point timestamp;
    
    WALRecordHeader(WALRecordType t, uint32_t size, uint64_t txn_id, uint64_t sequence_num)
        : type(t), record_size(size), transaction_id(txn_id), lsn(sequence_num), 
          checksum(0), timestamp(std::chrono::steady_clock::now()) {}
};

// WAL record for data operations
template<typename KeyType>
struct WALDataRecord {
    WALRecordHeader header;
    uint16_t page_id;
    KeyType key;
    std::vector<uint8_t> old_data;  // For rollback
    std::vector<uint8_t> new_data;  // Redo
    
    WALDataRecord(WALRecordType type, uint64_t txn_id, uint64_t lsn, 
                  uint16_t pid, const KeyType& k)
        : header(type, sizeof(WALDataRecord), txn_id, lsn), 
          page_id(pid), key(k) {}
};

// WAL manager class
template<typename KeyType>
class WALManager {
private:
    std::string wal_file_path;
    std::ofstream wal_file;
    std::mutex wal_mutex;
    
    std::atomic<uint64_t> next_lsn;
    std::atomic<uint64_t> next_transaction_id;
    std::atomic<uint64_t> last_checkpoint_lsn;
    
    // Need buffer for batching writes
    std::vector<uint8_t> write_buffer;
    size_t buffer_size_limit;
    
    uint32_t calculateChecksum(const void* data, size_t size);
    void flushBuffer();
    
public:
    WALManager(const std::string& wal_path, size_t buffer_limit = 4096);
    ~WALManager();
    
    uint64_t beginTransaction();
    void commitTransaction(uint64_t txn_id);
    void abortTransaction(uint64_t txn_id);
    
    // Data operation logging
    uint64_t logInsert(uint64_t txn_id, uint16_t page_id, const KeyType& key, 
                       const std::vector<uint8_t>& data);
    uint64_t logDelete(uint64_t txn_id, uint16_t page_id, const KeyType& key, 
                       const std::vector<uint8_t>& old_data);
    uint64_t logUpdate(uint64_t txn_id, uint16_t page_id, const KeyType& key,
                       const std::vector<uint8_t>& old_data, 
                       const std::vector<uint8_t>& new_data);
    
    // Checkpoint management
    uint64_t writeCheckpoint();
    uint64_t getLastCheckpointLSN() const { return last_checkpoint_lsn.load(); }
    
    // Recovery operations
    void replay(uint64_t from_lsn = 0);
    void truncate(uint64_t up_to_lsn);
    
    // Utility
    void sync();  // Force write to disk
    uint64_t getCurrentLSN() const { return next_lsn.load(); }
    size_t getWALSize() const;
};
