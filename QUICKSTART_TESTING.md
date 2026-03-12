# Quick Start: Testing libtfs & tebakofs

This guide shows you exactly how to verify that everything works.

## Prerequisites Check

```bash
# Check if you have the required tools
which cmake        # Need 3.24+
which mksquashfs   # For SquashFS fixtures
which zip          # For ZIP fixtures

# If mksquashfs is missing:
# Ubuntu/Debian: sudo apt-get install squashfs-tools
# macOS: brew install squashfs
# Fedora: sudo dnf install squashfs-tools
```

## Step 1: Generate Test Fixtures (1 minute)

```bash
cd tests/fixtures
./regenerate_all.sh
```

**Expected output:**
```
>>> Regenerating ZIP fixtures...
Creating simple.zip...
Creating nested.zip...
Creating empty.zip...
Creating large.zip...
Creating corrupted.zip...
✅ ZIP fixtures regenerated successfully

>>> Regenerating SquashFS fixtures...
Creating simple.sqfs...
Creating nested.sqfs...
Creating empty.sqfs...
Creating permissions.sqfs...
Creating large.sqfs...
Creating corrupted.sqfs...
✅ SquashFS fixtures regenerated successfully

✅ All fixtures regenerated successfully!

Fixture summary:
  ZIP fixtures:      5 archives
  SquashFS fixtures: 6 archives
```

## Step 2: Build Everything (2-5 minutes)

```bash
# From project root
cd /Users/mulgogi/src/tamatebako/libdwarfs

# Configure (if not already done)
cmake -B build -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build -j$(sysctl -n hw.ncpu)
```

**What gets built:**
- `libtfs` library
- `tebakofs` CLI tool
- 5 test executables
- Test fixtures copied to build directory

## Step 3: Run Automated Verification (< 1 minute)

```bash
# From project root
./tests/verify_all.sh
```

**Expected output:**
```
==========================================
libtfs & tebakofs Verification Script
==========================================

1. Checking test fixtures...
✓ Test fixtures present

2. Checking tebakofs CLI...
✓ tebakofs CLI built

3. Checking test executables...
✓ All test executables built

4. Running unit tests...
✓ All tests passed (128 tests)

5. Testing tebakofs CLI with ZIP...
✓ ZIP ls command works
✓ ZIP cat command works

6. Testing tebakofs CLI with SquashFS...
✓ SquashFS ls command works
✓ SquashFS cat command works
✓ SquashFS stat command works (POSIX permissions preserved)

==========================================
✅ All verifications passed!
==========================================

Summary:
  • Test fixtures: Ready ✓
  • tebakofs CLI: Built ✓
  • Unit tests: All passing ✓
  • ZIP backend: Working ✓
  • SquashFS backend: Working ✓
```

## Step 4: Try the CLI Tool Manually

### Basic Commands

```bash
# Show help
./build/tebakofs help

# Show command-specific help
./build/tebakofs help ls
./build/tebakofs help extract
```

### ZIP Backend Examples

```bash
# List directory
./build/tebakofs ls tests/fixtures/zip/simple.zip

# Expected output:
/test.txt
/file2.txt

# Show file contents
./build/tebakofs cat tests/fixtures/zip/simple.zip /test.txt

# Expected output:
Hello from ZIP!

# Show archive info
./build/tebakofs info tests/fixtures/zip/nested.zip

# Expected output:
Archive: tests/fixtures/zip/nested.zip
Type: ZIP
Files: 3
Directories: 3
Total size: ...

# Directory tree
./build/tebakofs tree tests/fixtures/zip/nested.zip

# Expected output:
/
├── dir1/
│   ├── file1.txt
│   └── subdir/
│       └── file2.txt
└── dir2/
    └── file3.txt
```

### SquashFS Backend Examples

```bash
# List with details (shows real permissions!)
./build/tebakofs ls -l tests/fixtures/squashfs/permissions.sqfs

# Expected output:
-r--r--r--        15 B  2025-12-22 16:17:00  /readonly.txt
-rwxr-xr-x        19 B  2025-12-22 16:17:00  /script.sh
-rw-------        14 B  2025-12-22 16:17:00  /private.txt
drwx------         0 B  2025-12-22 16:17:00  /restricted_dir

# Show file metadata
./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /script.sh

# Expected output:
File: /script.sh
Type: file
Size: 19 B (19 bytes)
Modified: Sat Dec 22 16:17:00 2025

# Show contents
./build/tebakofs cat tests/fixtures/squashfs/simple.sqfs /test.txt

# Expected output:
Hello from SquashFS!
```

### Advanced Commands

```bash
# List recursively
./build/tebakofs ls -rl tests/fixtures/squashfs/nested.sqfs

# Extract entire archive
./build/tebakofs extract tests/fixtures/zip/simple.zip /tmp/test-extract

# Extract specific files
./build/tebakofs extract tests/fixtures/squashfs/nested.sqfs /dir1/file1.txt /dir2/file3.txt

# Search for files
./build/tebakofs find tests/fixtures/zip/nested.zip "*.txt"
```

## Step 5: Run Full Test Suite

```bash
cd build

# Run all tests with verbose output
ctest --verbose

# Run specific test suites
ctest -R test_zip_backend --verbose
ctest -R test_squashfs_backend --verbose
ctest -R test_zip_integration --verbose
ctest -R test_squashfs_integration --verbose
```

**Expected results:**
- **128 tests total**
- **100% pass rate**
- **Test suites**: `test_backend_factory` (8), `test_zip_backend` (47), `test_zip_integration` (13), `test_squashfs_backend` (47), `test_squashfs_integration` (13)

## Performance Comparison

Compare ZIP vs SquashFS performance:

```bash
# Large file read (SquashFS should be ~2x faster)
time ./build/tebakofs cat tests/fixtures/zip/large.zip /large.txt > /dev/null
time ./build/tebakofs cat tests/fixtures/squashfs/large.sqfs /large.txt > /dev/null

# Directory listing (SquashFS should be faster)
time ./build/tebakofs ls tests/fixtures/zip/large.zip /many_files > /dev/null
time ./build/tebakofs ls tests/fixtures/squashfs/large.sqfs /many_files > /dev/null
```

## Troubleshooting

### If fixtures are missing:
```bash
cd tests/fixtures && ./regenerate_all.sh
```

### If build fails:
```bash
rm -rf build
cmake -B build -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### If tests fail:
```bash
# Check fixtures are in build directory
ls build/tests/fixtures/zip/
ls build/tests/fixtures/squashfs/

# Rebuild and re-copy fixtures
cmake --build build
```

## What You've Verified

After completing these steps, you've confirmed:

✅ **Test Fixtures**: All 11 test archives generated correctly  
✅ **libtfs Library**: Builds successfully with ZIP and SquashFS backends  
✅ **tebakofs CLI**: Fully functional with all commands  
✅ **Unit Tests**: 94 backend tests passing (47 ZIP + 47 SquashFS)  
✅ **Integration Tests**: 26 factory tests passing (13 ZIP + 13 SquashFS)  
✅ **ZIP Backend**: File reading, seek support, directory traversal  
✅ **SquashFS Backend**: Native seek, POSIX permissions, better performance  

## Next Steps

1. **Explore the CLI**: Try different commands and options
2. **Read Documentation**: 
   - [`docs/backends/ZIP_BACKEND.adoc`](docs/backends/ZIP_BACKEND.adoc)
   - [`docs/backends/SQUASHFS_BACKEND.adoc`](docs/backends/SQUASHFS_BACKEND.adoc)
3. **Review Tests**: Look at test code to understand usage patterns
4. **Performance Tuning**: Profile your specific use cases

## Support

- **Full Testing Guide**: [`tests/TESTING_GUIDE.md`](tests/TESTING_GUIDE.md)
- **Fixture Documentation**: [`tests/fixtures/README.md`](tests/fixtures/README.md)
- **Main Documentation**: [`README.adoc`](README.adoc)