# ZIP Test Fixtures

Test fixtures for ZIP backend validation.

## Fixtures

### simple.zip
- **Purpose**: Basic read/write operations
- **Size**: ~340 bytes
- **Contents**:
  ```
  test.txt       - "Hello from ZIP!\n" (16 bytes)
  file2.txt      - "Second file\n" (12 bytes)
  ```
- **Tests**: File reading, basic operations, concurrent access

### nested.zip
- **Purpose**: Directory traversal and nested paths
- **Size**: ~953 bytes
- **Structure**:
  ```
  dir1/
    file1.txt        - "File 1\n"
    subdir/
      file2.txt      - "File 2\n"
  dir2/
    file3.txt        - "File 3\n"
  ```
- **Tests**: Directory iteration, nested path resolution

### empty.zip
- **Purpose**: Edge cases (empty files/directories)
- **Size**: ~326 bytes
- **Contents**:
  ```
  empty_file.txt   - 0 bytes
  empty_dir/       - empty directory
  ```
- **Tests**: Zero-length files, empty directories

### large.zip
- **Purpose**: Performance testing
- **Size**: ~10 MB
- **Contents**:
  ```
  large.txt        - 10 MB random data
  many_files/
    file1.txt through file100.txt - "File N\n"
  ```
- **Tests**: Large file reading, many files iteration, performance benchmarks

### corrupted.zip
- **Purpose**: Error handling
- **Size**: ~340 bytes (corrupted)
- **Contents**: Copy of simple.zip with 100 bytes overwritten at offset 100
- **Tests**: Corrupted archive detection, error handling

## Regenerating

```bash
cd tests/fixtures/zip
./create_fixtures.sh
```

## Prerequisites

- `zip` command (usually pre-installed)
- `dd` command (for test data generation)

## Testing

```bash
# Unit tests
cd build && ctest -R test_zip_backend --verbose

# Integration tests
ctest -R test_zip_integration --verbose

# Manual CLI testing
./build/tebakofs ls tests/fixtures/zip/simple.zip
./build/tebakofs cat tests/fixtures/zip/simple.zip /test.txt
./build/tebakofs tree tests/fixtures/zip/nested.zip
./build/tebakofs extract tests/fixtures/zip/simple.zip /tmp/test-zip
```

## Notes

- ZIP format does not preserve POSIX permissions
- Default permissions: 0644 for files, 0755 for directories
- Seek operations use close/reopen pattern (5-20 ms overhead)