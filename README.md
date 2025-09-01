# Custom Database Engine by Fiona V. and Andrew S.

A custom database engine with page oriented content addressable storage, deduplication, and B+ Tree indexing, getting rid of redundant full page writes.

Note that git commit history is not representative of the work distribution, as the majority of the work is done in VSCode Live Share sessions of both of us working on it simultaneously.

A simple command-line interface to test the B+Tree database implementation with content-addressable storage, plus a FastAPI web server for REST API access.

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

### FastAPI Web Server
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

A B+ Tree is a self-balancing tree data structure optimized for disk storage and database systems. Unlike regular B-Trees, B+ Trees have a key difference: **all data is stored only in the leaf nodes**, while internal nodes contain only keys for navigation.

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

## Content-Addressable Storage

The database implements content-addressable storage where each page is identified by a unique hash based on its content rather than a fixed physical location. This approach enables automatic deduplication since identical pages produce the same hash and are stored only once, reducing storage requirements by eliminating duplicates. The system maintains a mapping from content hashes to actual page data through the `ContentStorage` class, which checks for existing content before writing new pages.

This design provides significant storage efficiency improvements, particularly during B+ Tree operations like splits and merges where similar page structures are common. Content-based addressing also enables more intelligent caching strategies since pages can be cached by their content hash, improving cache hit rates when the same content is requested under different logical page IDs. Additionally, content hashes provide built in data integrity verification, so any corruption or modification to page content results in a different hash, making tampering immediately detectable without additional overhead.

## Block-Level Cache

We used an LRU cache and a special write policy for a block level cache. What this does is it sits between our B+ Tree operations and the content addressable storage so that you can cache frequently accessed pages in memory instead of writing every page modification immediately. 
So, it looks something like `B+ Tree operations -> Block-Level Cache (LRU) -> Content-Addressable Storage -> Persistent Storage`

We chose this design because it means that frequently accessed pages stay in memory, multiple modifications to the same page can be batched, and so that there are fewer calls to our content storage. Our multi threaded writer queue also avoids bottlenecks that could happen if our B+ Tree splits or merges. 

The block level cache and multi threaded writer queue use the `PageCache` class and the `WriterQueue` class. The `PageCache` class is used for LRU based caching, dirty page tracing, eviction when the cache is full, and operations with mutex protection. The WriterQueue class is used for multi threaded background write processing, queue based batching, and async write processing.

All B+ Tree operations first go through the cache, modified pages are queued for background writing, and there is proper cleanup and flushing and destruction.

## API Endpoints

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

