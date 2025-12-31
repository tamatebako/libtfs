# Testing Guide for libtfs

This guide shows how to build, run, and verify all tests for the libtfs library and tebakofs CLI tool.

## Quick Start

```bash
# 1. Ensure fixtures exist
cd tests/fixtures && ./regenerate_all.sh && cd ../..

# 2. Configure and build
cmake -B build -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
cmake --build build

# 3. Run all tests
cd build && ctest --verbose

# 4. Test CLI manually
./build/tebakofs help
./build/tebakofs ls tests/fixtures/zip/simple.zip
```

## Prerequisites

### Required Tools

1. **CMake** 3.24+
2. **C++17 compiler** (gcc 9+, clang 10+, MSVC 2019+)
3. **vcpkg** (for dependency management)
4. **Google Test** (installed via vcpkg)

### For Test Fixtures

5. **zip** command (usually pre-installed)
6. **mksquashfs** from squashfs-tools:
   - Ubuntu/Debian: `sudo apt-get install squashfs-tools`
   - macOS: `brew install squashfs`
   - Fedora/RHEL: `sudo dnf install squashfs-tools`

## Step-by-Step Testing

### Step 1: Verify/Regenerate Test Fixtures

```bash
# Check if fixtures exist
ls -lh tests/fixtures/zip/*.zip
ls -lh tests/fixtures/squashfs/*.sqfs

# Regenerate if needed
cd tests/fixtures
./regenerate_all.sh
cd ../..
```

**Expected Output:**
```
>>> Regenerating ZIP fixtures...
Creating simple.zip...
Creating nested.zip...
...
✅ ZIP fixtures regenerated successfully

>>> Regenerating SquashFS fixtures...
Creating simple.sqfs...
Creating nested.sqfs...
...
✅ SquashFS fixtures regenerated successfully

✅ All fixtures regenerated successfully!
```

### Step 2: Configure Build

```bash
# Set vcpkg root (if not already set)
export VCPKG_ROOT=/path/to/vcpkg

# Configure with tests enabled
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
```

**Expected Output:**
```
-- Found version.txt with 0.11.0
-- LIBDWARFS_WR_VERSION: 0.11.0
...
-- Configuring done
-- Generating done
-- Build files written to: .../build
```

### Step 3: Build

```bash
cmake --build build -j$(nproc)
```

**Expected Output:**
```
[ 10%] Building CXX object CMakeFiles/tfs.dir/src/backend_factory.cpp.o
[ 20%] Building CXX object CMakeFiles/tfs.dir/src/backends/zip_backend.cpp.o
[ 30%] Building CXX object CMakeFiles/tfs.dir/src/backends/squashfs_backend.cpp.o
...
[100%] Built target tebakofs
```

### Step 4: Run Unit Tests

#### All Unit Tests

```bash
cd build
ctest --verbose
```

#### Backend-Specific Tests

```bash
# ZIP backend tests (47 tests)
ctest -R test_zip_backend --verbose

# SquashFS backend tests (47 tests)
ctest -R test_squashfs_backend --verbose

# Factory tests
ctest -R test_backend_factory --verbose
```

**Expected Output (example for ZIP):**
```
test 1
    Start 1: test_zip_backend

1: Test command: /path/to/build/test_zip_backend
1: Test timeout computed to be: 1500
1: [==========] Running 47 tests from 2 test suites.
1: [----------] Global test environment set-up.
1: [----------] 8 tests from ZipBackendTest
1: [ RUN      ] ZipBackendTest.ConstructorCreatesUnmountedBackend
1: [       OK ] ZipBackendTest.ConstructorCreatesUnmountedBackend (0 ms)
...
1: [==========] 47 tests from 2 test suites ran. (XXX ms total)
1: [  PASSED  ] 47 tests.
1/1 Test #1: test_zip_backend .................   Passed    X.XX sec
```

### Step 5: Run Integration Tests

```bash
# ZIP integration tests (13 tests)
ctest -R test_zip_integration --verbose

# SquashFS integration tests (13 tests)
ctest -R test_squashfs_integration --verbose
```

**Expected Output:**
```
1: [==========] Running 13 tests from 1 test suite.
1: [----------] Global test environment set-up.
1: [----------] 13 tests from BackendFactoryZipTest
1: [ RUN      ] BackendFactoryZipTest.DetectsZipByMagicBytes
1: [       OK ] BackendFactoryZipTest.DetectsZipByMagicBytes (1 ms)
...
1: [==========] 13 tests from 1 test suite ran. (XX ms total)
1: [  PASSED  ] 13 tests.
```

### Step 6: Test CLI Tool

#### Help and Info

```bash
# Show help
./build/tebakofs help

# Show command help
./build/tebakofs help ls
./build/tebakofs help extract
```

#### ZIP Backend Testing

```bash
# List contents
./build/tebakofs ls tests/fixtures/zip/simple.zip

# List with details
./build/tebakofs ls -l tests/fixtures/zip/simple.zip

# List recursively
./build/tebakofs ls -rl tests/fixtures/zip/nested.zip

# Show archive info
./build/tebakofs info tests/fixtures/zip/simple.zip

# Display file
./build/tebakofs cat tests/fixtures/zip/simple.zip /test.txt

# Show directory tree
./build/tebakofs tree tests/fixtures/zip/nested.zip

# Show file metadata
./build/tebakofs stat tests/fixtures/zip/simple.zip /test.txt

# Extract to temp directory
./build/tebakofs extract tests/fixtures/zip/simple.zip /tmp/test-zip

# Extract specific files
./build/tebakofs extract tests/fixtures/zip/nested.zip /dir1/file1.txt /dir2/file3.txt

# Search for files
./build/tebakofs find tests/fixtures/zip/nested.zip "*.txt"
```

#### SquashFS Backend Testing

```bash
# List contents
./build/tebakofs ls tests/fixtures/squashfs/simple.sqfs

# List with details (shows real permissions!)
./build/tebakofs ls -l tests/fixtures/squashfs/permissions.sqfs

# List recursively
./build/tebakofs ls -rl tests/fixtures/squashfs/nested.sqfs

# Show archive info
./build/tebakofs info tests/fixtures/squashfs/simple.sqfs

# Display file
./build/tebakofs cat tests/fixtures/squashfs/simple.sqfs /test.txt

# Show directory tree
./build/tebakofs tree tests/fixtures/squashfs/nested.sqfs

# Show file metadata (real POSIX permissions!)
./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /readonly.txt
./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /script.sh
./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /private.txt

# Extract to temp directory
./build/tebakofs extract tests/fixtures/squashfs/simple.sqfs /tmp/test-squashfs

# Extract specific files
./build/tebakofs extract tests/fixtures/squashfs/nested.sqfs /dir1/file1.txt

# Search for files
./build/tebakofs find tests/fixtures/squashfs/nested.sqfs "*.txt"
```

#### Performance Testing

```bash
# Time large file reading (SquashFS should be faster)
time ./build/tebakofs cat tests/fixtures/zip/large.zip /large.txt > /dev/null
time ./build/tebakofs cat tests/fixtures/squashfs/large.sqfs /large.txt > /dev/null

# Compare listing performance
time ./build/tebakofs ls tests/fixtures/zip/large.zip /many_files > /dev/null
time ./build/tebakofs ls tests/fixtures/squashfs/large.sqfs /many_files > /dev/null
```

## Expected Test Results

### Unit Tests Summary

| Test Suite | Tests | Expected Result |
|------------|-------|-----------------|
| test_backend_factory | 8 | All pass |
| test_zip_backend | 47 | All pass |
| test_zip_integration | 13 | All pass |
| test_squashfs_backend | 47 | All pass |
| test_squashfs_integration | 13 | All pass |
| **Total** | **128** | **100% pass** |

### CLI Tests - Expected Outputs

#### `tebakofs ls tests/fixtures/zip/simple.zip`
```
/test.txt
/file2.txt
```

#### `tebakofs ls -l tests/fixtures/squashfs/permissions.sqfs`
```
-r--r--r--        15 B  2025-12-22 16:17:00  /readonly.txt
-rwxr-xr-x        19 B  2025-12-22 16:17:00  /script.sh
-rw-------        14 B  2025-12-22 16:17:00  /private.txt
drwx------         0 B  2025-12-22 16:17:00  /restricted_dir
```

#### `tebakofs cat tests/fixtures/zip/simple.zip /test.txt`
```
Hello from ZIP!
```

#### `tebakofs info tests/fixtures/squashfs/simple.sqfs`
```
Archive: tests/fixtures/squashfs/simple.sqfs
Type: SquashFS
Files: 2
Directories: 0
Total size: 33 B (33 bytes)
```

## Troubleshooting

### Test Failures

#### "No such file or directory" for fixtures
```bash
# Solution: Regenerate fixtures
cd tests/fixtures && ./regenerate_all.sh
```

#### "mksquashfs: command not found"
```bash
# Solution: Install squashfs-tools
# Ubuntu/Debian:
sudo apt-get install squashfs-tools

# macOS:
brew install squashfs

# Fedora/RHEL:
sudo dnf install squashfs-tools
```

#### Build errors with vcpkg
```bash
# Solution: Update vcpkg
cd ${VCPKG_ROOT}
git pull
./bootstrap-vcpkg.sh  # or .bat on Windows

# Clean and rebuild
rm -rf build
cmake -B build -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

#### Tests fail to find fixtures
```bash
# Solution: Ensure fixtures are copied to build directory
# This happens automatically via CMakeLists.txt, but verify:
ls build/tests/fixtures/zip/
ls build/tests/fixtures/squashfs/

# If missing, rebuild:
cmake --build build
```

### CLI Tool Issues

#### "tebakofs: command not found"
```bash
# Solution: Use full path
./build/tebakofs ls archive.zip

# Or add to PATH
export PATH=$PATH:$(pwd)/build
tebakofs ls archive.zip
```

#### "Error: Failed to open archive"
```bash
# Check file exists
ls -l tests/fixtures/zip/simple.zip

# Check format is supported
file tests/fixtures/zip/simple.zip

# Try with verbose output
./build/tebakofs -v ls tests/fixtures/zip/simple.zip
```

## Continuous Integration

For CI/CD pipelines:

```bash
#!/bin/bash
set -e

# Install dependencies
sudo apt-get update
sudo apt-get install -y cmake build-essential squashfs-tools zip

# Setup vcpkg
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=$(pwd)/vcpkg

# Generate fixtures
cd tests/fixtures && ./regenerate_all.sh && cd ../..

# Build and test
cmake -B build -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

## Coverage Testing

To generate code coverage reports:

```bash
# Configure with coverage
cmake -B build -DWITH_TESTS=ON -DWITH_COVERAGE=ON \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake

# Build and run tests
cmake --build build
cd build && ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --list coverage.info
```

## Performance Benchmarking

```bash
# Run performance tests
cd build
ctest -R Performance --verbose

# Or run specific performance tests
./test_zip_backend --gtest_filter="*Performance*"
./test_squashfs_backend --gtest_filter="*Performance*"
```

## Next Steps

After all tests pass:

1. **Commit changes** including fixture scripts
2. **Update documentation** if new tests added
3. **Review coverage report** for gaps
4. **Run on multiple platforms** (Linux, macOS, Windows)
5. **Profile performance** if needed

## Additional Resources

- [Fixtures README](fixtures/README.md) - Detailed fixture documentation
- [ZIP Backend Docs](../docs/backends/ZIP_BACKEND.adoc) - ZIP implementation details
- [SquashFS Backend Docs](../docs/backends/SQUASHFS_BACKEND.adoc) - SquashFS implementation details
- [Main Testing Docs](../docs/TESTING.adoc) - Comprehensive testing strategy