# B-Tree Database Test Interface

Currently in progress.

Note that git commit history is not representative of the work distribution, as the majority of the work is done in VSCode Live Share sessions of both of us working on it simultaneously.

A simple command-line interface to test the B-tree database implementation.

## Building

```bash
make clean
make
```

## Running

```bash
./btree_test
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

## Technical Details

- The B-tree automatically handles node splitting when nodes become full
- The tree maintains balance during insertions and deletions
- Search operations are O(log n) complexity
- The implementation uses templates to support different key and value types