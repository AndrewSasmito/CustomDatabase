# Custom Database Engine by Fiona V. and Andrew S.

A custom database engine with page oriented content addressable storage, deduplication, and B+ Tree indexing, getting rid of redundant full page writes.

Note that git commit history is not representative of the work distribution, as the majority of the work is done in VSCode Live Share sessions of both of us working on it simultaneously.

A simple command-line interface to test the B+Tree database implementation with content-addressable storage, plus a FastAPI web server for REST API access.

Continue reading the README for implementation and design details!!

## Building

```bash
make clean
make
```

## Running

### Main Database Interface
```bash
./btree_test
```

### Content Hash Demo
```bash
./content_hash_demo
```

### Content Addressable Demo
```bash
./content_addressable_demo
```

### Deduplication Demo
```bash
./deduplication_demo
```

### Run All Tests
```bash
make tests
```

### FastAPI Web Server (in progress)
```bash
cd python
source venv/bin/activate
python start_server.py
```

The FastAPI server provides:
- **REST API**: HTTP endpoints for database operations
- **Interactive Docs**: Automatic API documentation at http://localhost:8000/docs **ONCE YOUR SERVER IS RUNNING.**
- **Prometheus Metrics**: Monitoring endpoint at http://localhost:8000/metrics
- **Health Check**: System status at http://localhost:8000/health
## Available Commands (Interactive Interface)

- `insert <key> <value>` - Insert a key-value pair into the database
- `delete <key>` - Delete a key from the database
- `search <key>` - Search for a key and display its value
- `print` - Print basic tree information
- `stats` - Show storage statistics and deduplication metrics
- `quit` or `exit` - Exit the program

## Example Usage

```
=== B-Tree Database Test Interface ===
Commands:
  insert <key> <value>  - Insert a key-value pair
  delete <key>          - Delete a key
  search <key>          - Search for a key
  print                 - Print tree structure
  stats                 - Show storage statistics
  quit                  - Exit
=====================================

> insert 1 apple
Inserted: 1 -> apple

> insert 2 banana
Inserted: 2 -> banana

> stats
=== Content Storage Statistics ===
Total unique content blocks: 3
Total page IDs assigned: 3
Next available page ID: 4
Total keys stored: 3
Total data bytes: 72
===================================

> search 1
Found key: 1 -> apple

> quit
Goodbye!
```

## Features

- **B+ Tree Implementation**: Uses a B+ tree with configurable maximum keys per node
- **Key-Value Storage**: Stores integer keys with string values
- **Basic Operations**: Insert, delete, and search operations
- **Error Handling**: Graceful handling of missing keys and invalid operations
- **Content-Addressable Storage**: Pages are identified by content hash for deduplication
- **Block Level Cache**: B+ tree operations go through the cache first
- **FastAPI REST Interface**: Modern web API with automatic documentation
- **Prometheus Metrics**: Real-time monitoring of database operations and performance

## How does a B+ Tree work?

<img width="701" height="231" alt="image" src="https://github.com/user-attachments/assets/bf3ab7cb-3dd9-4680-8c2a-facec1e4bdc9" />

A B+ Tree is a self-balancing tree data structure optimized for disk storage and database systems. Unlike regular B-Trees, B+ Trees have a key difference: **all data is stored only in the leaf nodes**, while internal nodes contain only keys for navigation. The key is knowing that each node is a Page<KeyType> which can include keys, children, data, is_leaf boolean, and header.page_id. All of these pages live in the page cache + content storage, and before modifying a page/node, we use the WAL to log these operations. Note that this is just a page struct, not actual virtual pages that live in your OS.

The page format is [header][keys][pointers or values]

The keys in a B+Tree must also be sorted. In this smaller database, we use insertion sort to preserve this property, however this can be improved in the future by for example using heaps.

Since we also have a cache using shared pointers, when we want to change cached nodes we use copy on write, since mutating them directly violates cache coherence. So, we update it using copy on write, then add it back to the cache to be used in order to not interfere with any current reads.
### Structure:
- **Internal nodes**: Store keys and child pointers for navigation
- **Leaf nodes**: Store keys and actual data values
- **Leaf linking**: All leaf nodes are linked together for sequential access

### Operations:
- **Search**: Start at root, compare keys to navigate down to the correct leaf
- **Insert**: Find target leaf, insert key-value pair, split if full
- **Delete**: Find target leaf, remove key-value pair, merge if underflow

### Benefits:
- **Consistent height**: All leaves at same level, ensuring O(log n) operations
- **Sequential access**: Leaf linking enables efficient range queries
- **Disk optimization**: Fewer disk reads since data is only in leaves
- **Cache friendly**: Internal nodes can stay in memory while leaves are on disk

## Technical Details

- The B+ tree automatically handles node splitting when nodes become full
- The tree maintains balance during insertions and deletions
- Search operations are O(log n) complexity
- The implementation uses templates to support different key and value types
- Content hashes are automatically computed and updated when pages change
- Identical pages share the same content hash, enabling deduplication
- There is an LRU cache as part of the block level cache to optimize content-addressable storage (more on that later)

## What do pages look like in our project?
Each page is just a bunch of bytes that maps to a node in our B+Tree. In other words, it is a fixed size region of bytes in a file where a single B+Tree node is stored. The page IDs are used to load in the page from our content addressable storage. That is why sometimes in our code, you will see:
```c++
auto child = page_cache.getPage(page_id);
```
Page layout:
```
+--------------------------------------------------------------+
| PageHeader (fixed-size)                                      |
|   - page_id                                                  |
|   - is_leaf                                                  |
|   - key_count                                                |
|   - etc...                                                   |
+--------------------------------------------------------------+
| Sorted Keys array                                            |
|   k0, k1, k2, ... (variable-sized, based on KeyType)         |
+--------------------------------------------------------------+
| Pointers or Values                                           |
|   If internal: child page_ids                                |
|   If leaf: serialized values                                 |
+--------------------------------------------------------------+
| Free space (unused bytes up to PAGE_SIZE, because key_count  |
| can change.)                                                 |
+--------------------------------------------------------------+
```
## How do we actually do operations based off of this?
Let's go over search, insert, and delete operations.

Search: Traverse the B+Tree until you reach your leaf node, and return the correct value

```walk page → decide branch based on keys → load next page by id → repeat until leaf```

Insert: Similar to traversal, except when you reach your leaf, insert into this position, and do whatever you need (bubbling up, etc) to preserve the B+Tree properties. Then, insertion writes key/value into the leaf page buffer (data), keeps it sorted, and enqueues it for disk write.

```walk page → decide branch based on keys → load next page by id → repeat until leaf → insert node```

Delete: Similar to traversal, except when you reach your leaf, you delete it, and do whatever you need (for example merging sibling nodes) to preserve the B+Tree properties. Then, deletion removes the key from the keys vector, removes the corresponding value bytes from data, and flushes the modified page via the cache & writer queue.

```walk page → decide branch based on keys → load next page by id → repeat until leaf → delete node```

## Content-Addressable Storage

The database implements content-addressable storage where each page is identified by a unique hash based on its content rather than a fixed physical location.

This approach allows automatic deduplication, since identical pages produce the same hash and are stored only once, so we don't store repeats which in tur reduces storage requirements. This also greatly reduces disk amplification, which is defined as "a phenomenon where the amount of data physically written to a storage device is greater than the amount of data intended to be written by the host". However, it means whenever we modify a page we must update the hash.

We have a mapping from content hashes to actual page data through the `ContentStorage` class, which checks for existing content before writing new pages.

We have two data structures in this class, which are:

  ```c++
std::unordered_map<std::string, std::shared_ptr<Page<KeyType>>> content_map;
std::unordered_map<uint16_t, std::string> page_to_hash;
```

They are self explanatory, and are used to map pages to its hash and vice versa. We use shared pointers in content_map and throughout our codebase so we can automatically free memory when its no longer being referenced, and because the cache, B+Trees, etc. can all refer to the same pages. TLDR, we can ignore manual memory management; ownership is shared and the page is freed automatically when no one references it.

This design gives us significant storage efficiency improvements, particularly during B+ Tree operations like splits and merges where similar page structures are common. Content-based addressing also enables more intelligent caching strategies since pages can be cached by their content hash, improving cache hit rates when the same content is requested under different logical page IDs. As for where it is used, the block level cache depends on this CAS (content addressable storage), which is used heavily throughout operations.

## Block-Level Cache

We used an LRU cache and a special write policy for a block level cache. What this does is it sits between our B+ Tree operations and the content addressable storage so that you can cache frequently accessed pages in memory instead of writing every page modification immediately. 
So, it looks something like `B+ Tree operations -> Block-Level Cache (LRU) -> Content-Addressable Storage -> Persistent Storage`

We chose this design because it means that frequently accessed pages stay in memory, multiple modifications to the same page can be batched, and so that there are fewer calls to our content storage. Our multi threaded writer queue also avoids bottlenecks that could happen if our B+ Tree splits or merges. 

The block level cache and multi threaded writer queue use the `PageCache` class and the `WriterQueue` class. The `PageCache` class is used for LRU based caching, dirty page tracing, eviction when the cache is full, and operations with mutex protection. The WriterQueue class is used for multi threaded background write processing, queue based batching, and async write processing.

All B+ Tree operations first go through the cache, modified pages are queued for background writing, and there is proper cleanup and flushing and destruction.

<img width="650" height="538" alt="image" src="https://github.com/user-attachments/assets/f3bc8d47-58e3-4e9e-ae7b-839848800d8b" />
<img width="650" height="447" alt="image" src="https://github.com/user-attachments/assets/c8bfef66-f3e9-445e-acad-46507709a63e" />
<img width="650" height="556" alt="image" src="https://github.com/user-attachments/assets/2f190399-d968-46e0-9d66-55b75ae8dfc5" />

## Writer Queue (multi threaded)

Instead of this:

```insert() → modify page → immediately write to “disk”```

We want this:

```
insert() → modify page → putPage() → enqueueWrite(page_id, page) → return fast
           background threads: writerWorker() → storePage(page) → clearDirtyFlag()
```

Why? Because writing to storage can be slow, and doing it synchronously could block main operations like inserts and queries to the DB. So, we try to aim for asynchronous modifications which we can do with threads.

In our writer queue, we start a specified number of C++ library writer threads (in this database its currently 2 but can be adjusted up to your preference). Each thread pulls in a batch of waiting writes to then process, and then writes it to the content addressable storage. The thread processes are trivial and they loop while running is true or there’s still work for them to do, and the efficiency happens because each thread can call writerWorker at the same time. 

In order to preserve mutual exclusion, the cache has its own mutex, and we use a simple condition variable pattern when getting batches and enqueuing writes.

We also have a job scheduler in `job_scheduler.cpp` that could be used to schedule higher level threads that can relate to other parts of the DB, such as the write ahead log (in progress). However, for the writer threads, we just use a FIFO writer queue.


## API Endpoints (in progress)

The FastAPI server provides the following REST endpoints:

- `GET /` - API information and status
- `POST /insert` - Insert a key-value pair
- `GET /search/{key}` - Search for a key
- `DELETE /remove/{key}` - Remove a key
- `GET /stats` - Database statistics
- `GET /health` - Health check
- `GET /metrics` - Prometheus metrics
- `POST /demo/populate` - Populate demo data
- `DELETE /demo/clear` - Clear all data

### Example API Usage

```bash
# Insert data
curl -X POST "http://localhost:8000/insert" \
     -H "Content-Type: application/json" \
     -d '{"key": "user:1", "value": "Alice"}'

# Search data
curl http://localhost:8000/search/user:1

# Get metrics
curl http://localhost:8000/metrics
```

## Testing

The project includes comprehensive demo programs that serve as tests:

- **Content Hash Demo**: Verifies content hashing works correctly
- **Content Addressable Demo**: Shows content-addressable storage benefits
- **Deduplication Demo**: Demonstrates automatic deduplication in action
- **Block-Level Cache Demo**: Tests LRU cache and write-back functionality
- **Interactive Interface**: Manual testing of B+ Tree operations
- **FastAPI Server**: REST API testing with automatic documentation

Run all tests with:
```bash
make tests
```

