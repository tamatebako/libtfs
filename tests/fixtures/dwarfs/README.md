# DwarFS Test Fixtures

This directory contains test archives for the DwarFS backend tests.

## Archives

### simple.dwarfs
Basic test archive with:
- 2 files in root (`hello.txt`, `test.txt`)
- 1 subdirectory with 1 file (`subdir/nested.txt`)

**Purpose**: Basic functionality testing (mount, file reading, directory listing)

### nested.dwarfs
Deep directory structure:
- Multiple nested levels (`a/b/c/d/`)
- Files at various levels (`root.txt`, `a/b/c/d/deep.txt`)

**Purpose**: Deep directory traversal and path handling

### permissions.dwarfs
POSIX permissions testing:
- Executable file (`executable.sh` - 755)
- Regular file (`readable.txt` - 644)
- Read-only file (`readonly.txt` - 444)

**Purpose**: Permission preservation and metadata accuracy

### large.dwarfs
Performance testing:
- 1MB file (`1mb.bin`)
- 10MB file (`10mb.bin`)

**Purpose**: Large file reading and performance benchmarking

### empty.dwarfs
Edge case testing:
- Empty archive (no files)

**Purpose**: Edge case handling and empty filesystem mounting

### corrupted.dwarfs
Error handling testing:
- Corrupted DwarFS archive (invalid magic bytes)

**Purpose**: Error handling and invalid input rejection

## Generation

### Prerequisites

Install DwarFS tools:

```bash
# macOS
brew install dwarfs

# Ubuntu/Debian
sudo apt-get install dwarfs

# Or build from deps (if configured)
# mkdwarfs should be in deps/bin/ after building
```

### Creating Test Archives

```bash
cd tests/fixtures/dwarfs
./create_fixtures.sh
```

This will:
1. Read source data from `../../test_data/dwarfs_source/`
2. Generate all DwarFS archives with compression level 9
3. Create corrupted archive for error testing

### Manual Creation

If you need to recreate specific archives:

```bash
# Simple archive
mkdwarfs -i ../../test_data/dwarfs_source/simple \
         -o simple.dwarfs \
         -l 9

# Nested archive
mkdwarfs -i ../../test_data/dwarfs_source/nested \
         -o nested.dwarfs \
         -l 9

# Permissions archive
mkdwarfs -i ../../test_data/dwarfs_source/permissions \
         -o permissions.dwarfs \
         -l 9

# Large archive
mkdwarfs -i ../../test_data/dwarfs_source/large \
         -o large.dwarfs \
         -l 9

# Empty archive
mkdwarfs -i ../../test_data/dwarfs_source/empty \
         -o empty.dwarfs

# Corrupted archive (from simple)
cp simple.dwarfs corrupted.dwarfs
dd if=/dev/zero of=corrupted.dwarfs bs=1 count=100 seek=100 conv=notrunc
```

## Test Data Structure

Source data is organized in `tests/test_data/dwarfs_source/`:

```
dwarfs_source/
├── simple/
│   ├── hello.txt          # "Hello, DwarFS!"
│   ├── test.txt           # "Test file content"
│   └── subdir/
│       └── nested.txt     # "Nested file"
├── nested/
│   ├── root.txt           # "Root file"
│   └── a/b/c/d/
│       └── deep.txt       # "Deep file"
├── permissions/
│   ├── executable.sh      # chmod 755
│   ├── readable.txt       # chmod 644
│   └── readonly.txt       # chmod 444
├── large/
│   ├── 1mb.bin           # 1 MB of zeros
│   └── 10mb.bin          # 10 MB of zeros
└── empty/
    # (empty directory)
```

## Verification

After generation, verify archives:

```bash
# List archive contents
dwarfsck --checksum simple.dwarfs

# Mount and inspect
mkdir -p /tmp/dwarfs_test
dwarfs simple.dwarfs /tmp/dwarfs_test
ls -la /tmp/dwarfs_test
umount /tmp/dwarfs_test
```

## Usage in Tests

These fixtures are automatically copied to the build directory during CMake configuration and used by:

- `test_dwarfs_backend` - Unit tests for DwarfsBackend class
- `test_dwarfs_integration` - Integration tests for end-to-end workflows
- `test_c_api` - C API tests (if DwarFS support enabled)

## Notes

- Archives are generated with compression level 9 for maximum compression
- Permissions are preserved in the DwarFS format
- Empty archive tests edge cases in directory iteration
- Corrupted archive tests error handling robustness
- All archives should be kept small for fast test execution