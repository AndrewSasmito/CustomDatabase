# B-Tree Database Test Interface

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

## Available Commands

- `insert <key> <value>` - Insert a key-value pair into the database
- `delete <key>` - Delete a key from the database
- `search <key>` - Search for a key and display its value
- `print` - Print basic tree information
- `quit` or `exit` - Exit the program

## Example Usage

```
=== B-Tree Database Test Interface ===
Commands:
  insert <key> <value>  - Insert a key-value pair
  delete <key>          - Delete a key
  search <key>          - Search for a key
  print                 - Print tree structure
  quit                  - Exit
=====================================

> insert 1 apple
Inserted: 1 -> apple

> insert 2 banana
Inserted: 2 -> banana

> search 1
Found key: 1 -> apple

> search 3
Key not found: 3

> delete 1
Deleted key: 1

> search 1
Key not found: 1

> quit
Goodbye!
```

## Features

- **B+ Tree Implementation**: Uses a B+ tree with configurable maximum keys per node (default: 3 for easy testing)
- **Key-Value Storage**: Stores integer keys with string values
- **Basic Operations**: Insert, delete, and search operations
- **Error Handling**: Graceful handling of missing keys and invalid operations
- **Content-Addressable Storage**: Pages are identified by content hash for deduplication

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