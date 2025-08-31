# Custom Database Engine by Fiona V. and Andrew S.

A custom database engine with page oriented content addressable storage, deduplication, and B-Tree indexing, getting rid of redundant full page writes.

Currently in progress.

Note that git commit history is not representative of the work distribution, as the majority of the work is done in VSCode Live Share sessions of both of us working on it simultaneously.

A simple command line interface is used to test the B-tree database implementation with content addressable storage.

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

### Run All Tests
```bash
make tests
```

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

## Technical Details

- The B-tree automatically handles node splitting when nodes become full
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
So, it looks something like `B-Tree operations -> Block-Level Cache (LRU) -> Content-Addressable Storage -> Persistent Storage`

We chose this design because it means that frequently accessed pages stay in memory, multiple modifications to the same page can be batched, and so that there are fewer calls to our content storage. Our multi threaded writer queue also avoids bottlenecks that could happen if our B-Tree splits or merges. 

The block level cache and multi threaded writer queue use the `PageCache` class and the `WriterQueue` class. The `PageCache` class is used for LRU based caching, dirty page tracing, eviction when the cache is full, and operations with mutex protection. The WriterQueue class is used for multi threaded background write processing, queue based batching, and async write processing.

All B+ Tree operations first go through the cache, modified pages are queued for background writing, and there is proper cleanup and flushing and destruction.
## Testing

The project includes comprehensive demo programs that serve as tests:

- **Content Hash Demo**: Verifies content hashing works correctly
- **Content Addressable Demo**: Shows content-addressable storage benefits
- **Deduplication Demo**: Demonstrates automatic deduplication in action
- **Block-Level Cache Demo**
- **Interactive Interface**: Manual testing of B-tree operations

Run all tests with:
```bash
make tests
```
