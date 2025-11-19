#include "wal.h"
#include <iostream>
#include <iomanip>
#include <cstring>

/*
 Ensure we are able to write to a file and have enough space on our buffer
 This is what appends records to our binary WAL file, and the WAL file can:
    - buffer writes in memory and flush to disk when needed
    - replay WAL from specific checkpoint/LSN
    - keeps track of next lsn and transaction ids
 The function below is just a constructor which opens the WAL file in binary append mode
 and initializes its counters (next_ls, transaction id, etc).
*/
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

/*
 When the WAL is destroyed we need to flush any pending buffers,
 and close the file, which is what happens below.
*/
template<typename KeyType>
WALManager<KeyType>::~WALManager() {
    if (wal_file.is_open()) {
        flushBuffer();
        wal_file.close();
    }
}

/*
 In order to see if logs are corrupt, we need to have this function
 so we are able to calculate the Checksum over the record bytes. If
 the data is corrupt, then
*/
template<typename KeyType>
uint32_t WALManager<KeyType>::calculateChecksum(const void* data, size_t size) {
    uint32_t checksum = 0;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        checksum = (checksum << 1) ^ bytes[i];
    }
    return checksum;
}

/*
 Write in memory write_buffer to WAL and then flush it 
 to disk. This is called when the buffer limit is reached.
*/
template<typename KeyType>
void WALManager<KeyType>::flushBuffer() {
    if (!write_buffer.empty()) {
        wal_file.write(reinterpret_cast<const char*>(write_buffer.data()), write_buffer.size());
        wal_file.flush();
        write_buffer.clear();
        std::cout << "WAL: Flushed buffer to disk" << std::endl;
    }
}

/*
 When we want to start a transaction, this allocates a new
 transaction ID and returns it
*/
template<typename KeyType>
uint64_t WALManager<KeyType>::beginTransaction() {
    uint64_t txn_id = next_transaction_id.fetch_add(1);
    std::cout << "WAL: Started transaction " << txn_id << std::endl;
    return txn_id;
}

/*
 When committing a transaction, we need a new LSN,
 we create a commit record, find its checksum, and then
 writes it to the buffer, which is then flushed.
*/
template<typename KeyType>
void WALManager<KeyType>::commitTransaction(uint64_t txn_id) {
    // We don't want this to be interrupted so lock the mutex
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    uint64_t lsn = next_lsn.fetch_add(1);
    WALRecordHeader commit_record(WALRecordType::COMMIT, sizeof(WALRecordHeader), txn_id, lsn);
    // checkSum is used to create the hash
    commit_record.checksum = calculateChecksum(&commit_record, sizeof(commit_record) - sizeof(commit_record.checksum));
    
    const uint8_t* record_bytes = reinterpret_cast<const uint8_t*>(&commit_record);
    write_buffer.insert(write_buffer.end(), record_bytes, record_bytes + sizeof(commit_record));
    
    if (write_buffer.size() >= buffer_size_limit) {
        flushBuffer();
    }
    
    flushBuffer();
    
    std::cout << "WAL: Committed transaction " << txn_id << " (LSN: " << lsn << ")" << std::endl;
}

/*
 Add an abort record to the WAL for the given transaction ID.
*/
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

/*
 Now we have functions to log data operations such as insert, delete, update.
 Each of these functions creates a WALDataRecord with metadata, fills in the details, and
 adds it to the write buffer. If the buffer is full, it might automatically flush.
 I will add comments to this function as an example.
*/
template<typename KeyType>
uint64_t WALManager<KeyType>::logInsert(uint64_t txn_id, uint16_t page_id, const KeyType& key, 
                                        const std::vector<uint8_t>& data) {
    // All of these functions will lock the mutex since our writer queue is shared
    std::lock_guard<std::mutex> lock(wal_mutex);
    // Get the next LSN for this record
    uint64_t lsn = next_lsn.fetch_add(1);
    // Create the WALDataRecord for INSERT
    WALDataRecord<KeyType> record(WALRecordType::INSERT, txn_id, lsn, page_id, key);
    record.new_data = data;
    
    // Calculate actual record size including variable data
    record.header.record_size = sizeof(WALDataRecord<KeyType>) + data.size();
    // Calculate checksum over the record (excluding checksum field itself)
    record.header.checksum = calculateChecksum(&record, sizeof(record) - sizeof(record.header.checksum));
    
    // Append the record bytes to the write buffer
    const uint8_t* record_bytes = reinterpret_cast<const uint8_t*>(&record);
    write_buffer.insert(write_buffer.end(), record_bytes, record_bytes + sizeof(record));
    
    // Append the actual data payload
    write_buffer.insert(write_buffer.end(), data.begin(), data.end());
    
    // Check if the buffer is full, if so then flush.
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

/*
 A checkpoint shows us that up to this LSN, all data has been 
 flushed to the main data files. This makes recovery more efficient
 because we only have to care about logs after the last checkpoint.
*/
template<typename KeyType>
uint64_t WALManager<KeyType>::writeCheckpoint() {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    // Make sure to flush any pending writes first
    flushBuffer();
    
    uint64_t lsn = next_lsn.fetch_add(1);
    uint64_t checkpoint_txn = next_transaction_id.fetch_add(1);
    
    // Create checkpoint record
    WALRecordHeader checkpoint_record(WALRecordType::CHECKPOINT, sizeof(WALRecordHeader), checkpoint_txn, lsn);
    checkpoint_record.checksum = calculateChecksum(&checkpoint_record, sizeof(checkpoint_record) - sizeof(checkpoint_record.checksum));
    
    // Write checkpoint record directly to file
    wal_file.write(reinterpret_cast<const char*>(&checkpoint_record), sizeof(checkpoint_record));
    wal_file.flush();
    
    // Update last checkpoint LSN, so we can use this during recovery
    last_checkpoint_lsn.store(lsn);
    
    std::cout << "WAL: Wrote checkpoint at LSN " << lsn << std::endl;
    return lsn;
}

/*
 This just forces a flush of any buffered WAL records to the disk.
*/
template<typename KeyType>
void WALManager<KeyType>::sync() {
    // Lock the mutex to avoid concurrent writes
    std::lock_guard<std::mutex> lock(wal_mutex);
    flushBuffer();
}

/*
 Currently a stub function, since in a real function we would
 delete all of the WAL records up to a specified LSN (typically checkpoint LSN).
*/
template<typename KeyType>
void WALManager<KeyType>::truncate(uint64_t up_to_lsn) {
    std::lock_guard<std::mutex> lock(wal_mutex);
    
    std::cout << "WAL: Truncated up to LSN " << up_to_lsn << std::endl;
}

/*
 Just get the current size of the WAL file, which is used
 for monitoring purposes.
*/
template<typename KeyType>
size_t WALManager<KeyType>::getWALSize() const {
    std::ifstream file(wal_file_path, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        return file.tellg();
    }
    return 0;
}

/*
 This is mainly for debugging, we read through the WAL file from a given LSN.
 We read the headers then payloads, and print information about each record with
 an LSN at or after from_lsn.
*/
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
                // uint16_t page_id; KeyType key; followed by vector internals and variable payload.
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

/*
 This function replays the WAL from a given LSN but also invokes
 REDO handlers for data records to apply changes to the db state.
*/
template<typename KeyType>
void WALManager<KeyType>::replay(uint64_t from_lsn, const typename WALManager<KeyType>::RedoHandlers& handlers) {
    std::cout << "WAL: Replaying (with REDO) from LSN " << from_lsn << std::endl;
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
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file) {
            break;
        }

        if (header.lsn > max_seen_lsn) max_seen_lsn = header.lsn;
        if (header.transaction_id > max_seen_txn) max_seen_txn = header.transaction_id;

        if (header.record_size < sizeof(WALRecordHeader)) {
            std::cerr << "WAL: Corrupt record encountered (record_size < header). Stopping replay." << std::endl;
            break;
        }

        const auto remaining_record_bytes = static_cast<std::streamoff>(header.record_size - sizeof(WALRecordHeader));

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

                // Compute how many bytes remain in the record payload after page_id+key
                std::streamoff bytes_read_after_header = static_cast<std::streamoff>(sizeof(page_id) + sizeof(KeyType));
                std::streamoff variable_bytes = remaining_record_bytes - bytes_read_after_header;

                std::vector<uint8_t> var_payload;
                if (ok && variable_bytes > 0) {
                    var_payload.resize(static_cast<size_t>(variable_bytes));
                    file.read(reinterpret_cast<char*>(var_payload.data()), variable_bytes);
                    if (!file) ok = false;
                }

                if (ok && header.lsn >= from_lsn) {
                    if (header.type == WALRecordType::INSERT) {
                        if (handlers.on_insert) handlers.on_insert(page_id, key, var_payload);
                    } else if (header.type == WALRecordType::DELETE) {
                        if (handlers.on_delete) handlers.on_delete(page_id, key, var_payload);
                    } else { // UPDATE
                        // Heuristic split of var_payload into old/new halves is not reliable without lengths.
                        // Pass entire payload as new_data and leave old_data empty unless the caller knows layout.
                        if (handlers.on_update) handlers.on_update(page_id, key, std::vector<uint8_t>{}, var_payload);
                    }
                }

                if (!ok) {
                    file.clear();
                    file.seekg(after_header_pos + remaining_record_bytes);
                }
                break;
            }
            default: {
                file.seekg(remaining_record_bytes, std::ios::cur);
                break;
            }
        }
    }

    if (max_seen_lsn >= from_lsn && max_seen_lsn >= next_lsn.load()) {
        next_lsn.store(max_seen_lsn + 1);
    }
    if (max_seen_txn >= next_transaction_id.load()) {
        next_transaction_id.store(max_seen_txn + 1);
    }
    last_checkpoint_lsn.store(last_ckpt);

    std::cout << "WAL: Replay (with REDO) finished. next_lsn=" << next_lsn.load()
              << ", next_txn_id=" << next_transaction_id.load()
              << ", last_checkpoint_lsn=" << last_checkpoint_lsn.load() << std::endl;
}

template class WALManager<int>;
template class WALManager<std::string>;
