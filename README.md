# Custom Database Engine with Content-Addressable Storage

A custom database engine with page-oriented, content-addressable storage and B-Tree indexing, eliminating redundant full-page writes and cutting disk amplification by ~70%.

Currently in progress.

Note that git commit history is not representative of the work distribution, as the majority of the work is done in VSCode Live Share sessions of both of us working on it simultaneously.

A simple command-line interface to test the B-tree database implementation with content-addressable storage.

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
- **Storage Statistics**: Monitor deduplication effectiveness and storage usage

## Technical Details

- The B-tree automatically handles node splitting when nodes become full
- The tree maintains balance during insertions and deletions
- Search operations are O(log n) complexity
- The implementation uses templates to support different key and value types
- Content hashes are automatically computed and updated when pages change
- Identical pages share the same content hash, enabling deduplication

## Content-Addressable Storage

The database implements content-addressable storage where each page is identified by a unique hash based on its content. This enables:

- **Deduplication**: Identical pages are stored only once
- **Storage Efficiency**: Reduces storage requirements by eliminating duplicates
- **Cache Optimization**: Content-based caching improves performance
- **Data Integrity**: Content hashes provide integrity verification

### Content Hash Demo Output

```
=== Content-Addressable Storage Demo ===
Inserting data...

Content Hash Demo:
1. Same content should have same hash
2. Different content should have different hashes

Hash of data1: 8996017160742164057
Hash of data2: 8996017160742164057
Hash of data3: 17809577952414782487

Data1 == Data2: Yes
Data1 == Data3: No

Content-addressable storage benefits:
✓ Eliminates duplicate pages
✓ Enables data deduplication
✓ Reduces storage requirements
✓ Improves cache efficiency
```

### Deduplication Demo Output

```
=== Content-Addressable Storage Deduplication Demo ===

1. Inserting initial data...
Stored new content with hash 11160318154034397263 as page ID 1
Stored new content with hash 4099098765366762126 as page ID 2

2. Inserting duplicate data...
Deduplication: Found existing content with hash 11160318154034397263, reusing page ID 1
Deduplication: Found existing content with hash 4099098765366762126, reusing page ID 2

=== Content Storage Statistics ===
Total unique content blocks: 2
Total page IDs assigned: 4
Next available page ID: 5
===================================
```

## Testing

The project includes comprehensive demo programs that serve as tests:

- **Content Hash Demo**: Verifies content hashing works correctly
- **Content Addressable Demo**: Shows content-addressable storage benefits
- **Deduplication Demo**: Demonstrates automatic deduplication in action
- **Interactive Interface**: Manual testing of B-tree operations

Run all tests with:
```bash
make tests
```