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
    return checksum; // update the hash value but why? What is this for?
}

template<typename KeyType> // write buffer to the file, then clear it
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
    std::ifstream file(wal_file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "WAL: Failed to open WAL file for replay: " << wal_file_path << std::endl;
        return;
    }

    uint64_t max_seen_lsn = 0;
    uint64_t max_seen_txn = 0;
    uint64_t last_ckpt = last_checkpoint_lsn.load();

    while (true) {
        WALRecordHeader header(WALRecordType::ABORT, 0, 0, 0);
        // Read just the header portion at the start of each record
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file) {
            break;
        }

        // Track maxima for internal counters
        if (header.lsn > max_seen_lsn) max_seen_lsn = header.lsn;
        if (header.transaction_id > max_seen_txn) max_seen_txn = header.transaction_id;

        // Shouldn't happen but bail out to avoid infinite loop
        if (header.record_size < sizeof(WALRecordHeader)) {
            std::cerr << "WAL: Corrupt record encountered (record_size < header). Stopping replay." << std::endl;
            break;
        }

        const auto remaining_record_bytes = static_cast<std::streamoff>(header.record_size - sizeof(WALRecordHeader));

        // Process only records at or after from_lsn (still need to move cursor regardless)
        switch (header.type) {
            case WALRecordType::CHECKPOINT: {
                if (header.lsn >= from_lsn) {
                    std::cout << "WAL: [LSN " << header.lsn << "] CHECKPOINT" << std::endl;
                }
                last_ckpt = header.lsn;
                break;
            }
            case WALRecordType::COMMIT: {
                if (header.lsn >= from_lsn) {
                    std::cout << "WAL: [LSN " << header.lsn << "] COMMIT txn=" << header.transaction_id << std::endl;
                }
                break;
            }
            case WALRecordType::ABORT: {
                if (header.lsn >= from_lsn) {
                    std::cout << "WAL: [LSN " << header.lsn << "] ABORT txn=" << header.transaction_id << std::endl;
                }
                break;
            }
            case WALRecordType::INSERT:
            case WALRecordType::DELETE:
            case WALRecordType::UPDATE: {
                // For data records, the bytes immediately after the header are:
                //   uint16_t page_id; KeyType key; followed by vector internals and variable payload.
                uint16_t page_id = 0;
                KeyType key{};

                std::streampos after_header_pos = file.tellg();
                bool ok = true;

                file.read(reinterpret_cast<char*>(&page_id), sizeof(page_id));
                if (!file) ok = false;
 
                if (ok) {
                    file.read(reinterpret_cast<char*>(&key), sizeof(KeyType));
                    if (!file) ok = false;
                }

                if (ok && header.lsn >= from_lsn) {
                    const char* t = (header.type == WALRecordType::INSERT) ? "INSERT" :
                                    (header.type == WALRecordType::DELETE) ? "DELETE" : "UPDATE";
                    std::cout << "WAL: [LSN " << header.lsn << "] " << t
                              << " pid=" << page_id << " key=" << key << std::endl;
                }

                // Seek to the end of this record (skip remaining payload and vector internals)
                // Current position is after header + sizeof(page_id) + sizeof(KeyType)
                if (ok) {
                    std::streamoff bytes_read_after_header = static_cast<std::streamoff>(sizeof(page_id) + sizeof(KeyType));
                    std::streamoff to_skip = remaining_record_bytes - bytes_read_after_header;
                    if (to_skip > 0) {
                        file.seekg(to_skip, std::ios::cur);
                    }
                } else {
                    // Failed to read expected fields, try to realign by skipping remaining_record_bytes
                    file.clear();
                    file.seekg(after_header_pos + remaining_record_bytes);
                }
                break;
            }
            default: {
                // Unknown type so skip the rest of this record safely
                file.seekg(remaining_record_bytes, std::ios::cur);
                break;
            }
        }
    }

    // Update internal counters based on what weve seen
    if (max_seen_lsn >= from_lsn && max_seen_lsn >= next_lsn.load()) {
        next_lsn.store(max_seen_lsn + 1);
    }
    if (max_seen_txn >= next_transaction_id.load()) {
        next_transaction_id.store(max_seen_txn + 1);
    }
    last_checkpoint_lsn.store(last_ckpt);

    std::cout << "WAL: Replay finished. next_lsn=" << next_lsn.load()
              << ", next_txn_id=" << next_transaction_id.load()
              << ", last_checkpoint_lsn=" << last_checkpoint_lsn.load() << std::endl;
}

template class WALManager<int>;
template class WALManager<std::string>;
