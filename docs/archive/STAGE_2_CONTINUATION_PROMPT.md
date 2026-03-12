# Stage 2 Implementation: ZIP & SquashFS Backends

**Mode**: Code
**Task**: Implement multi-backend VFS with ZIP and SquashFS support
**Timeline**: 7-8 days
**See**: [`STAGE_2_CONTINUATION_PLAN.md`](STAGE_2_CONTINUATION_PLAN.md) for detailed tracking

---

## Context

You are implementing Stage 2 of the Tebako filesystem library (libtfs), which adds support for ZIP and SquashFS archive formats alongside the existing DwarFS support. This involves:

1. Creating a unified VFS abstraction (already designed)
2. Implementing a static factory for backend creation
3. Adding ZIP backend via `libzip` (vcpkg)
4. Adding SquashFS backend via `squashfs-tools-ng` (vcpkg overlay)
5. Comprehensive testing and documentation

**Architecture**: Object-oriented, MECE, separation of concerns, extensible via Open/Closed principle.

**Critical**: DwarFS format detection uses "DWARFS" (6 bytes) at offset 0 - this was confirmed through source code analysis at `/Users/mulgogi/src/external/dwarfs`.

---

## Current Project State

### Completed Work
- ✅ VFS interfaces designed: [`FileSystem`](../include/tebako/fs/filesystem.h), [`FileHandle`](../include/tebako/fs/file_handle.h), [`DirectoryIterator`](../include/tebako/fs/directory_iterator.h)
- ✅ Architecture finalized with static factory pattern
- ✅ Format detection specifications confirmed
- ✅ Dependency strategy: pure vcpkg

### Existing Codebase
- DwarFS implementation exists but not yet refactored to VFS interface
- Mount table exists: [`mount_table.h`](../include/tebako/fs/internal/mount_table.h)
- CMake build system active: [`CMakeLists.txt`](../CMakeLists.txt)
- Test framework using Google Test

---

## Implementation Tasks (8 Days)

### Day 1: vcpkg Overlay + BackendFactory

#### Task 1.1: Create vcpkg Overlay for squashfs-tools-ng

Create `vcpkg-overlay/squashfs-tools-ng/vcpkg.json`:
```json
{
  "name": "squashfs-tools-ng",
  "version": "1.3.2",
  "description": "New set of tools for working with SquashFS images",
  "homepage": "https://github.com/AgentD/squashfs-tools-ng",
  "license": "GPL-3.0-or-later",
  "dependencies": [
    {
      "name": "vcpkg-cmake",
      "host": true
    },
    {
      "name": "vcpkg-cmake-config",
      "host": true
    },
    "zlib",
    "lz4",
    "xz-utils",
    "zstd"
  ]
}
```

Create `vcpkg-overlay/squashfs-tools-ng/portfile.cmake`:
```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO AgentD/squashfs-tools-ng
    REF "v${VERSION}"
    SHA512 <compute-actual-hash>
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_SHARED_LIBS=OFF
        -DWITH_LZO=OFF
        -DWITH_LZ4=ON
        -DWITH_XZ=ON
        -DWITH_ZSTD=ON
        -DWITH_ZLIB=ON
        -DWITH_PTHREAD=ON
        -DBUILD_TOOLS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/squashfs-tools-ng)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE.md"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
     RENAME copyright)
```

Create `vcpkg-overlay/squashfs-tools-ng/usage`:
```
squashfs-tools-ng provides CMake targets:

    find_package(squashfs-tools-ng CONFIG REQUIRED)
    target_link_libraries(main PRIVATE squashfs-tools-ng::squashfs)
```

#### Task 1.2: Update Root vcpkg.json

Add to `dependencies` array in [`vcpkg.json`](../vcpkg.json):
```json
"libzip",
"squashfs-tools-ng"
```

#### Task 1.3: Implement BackendFactory

Create [`include/tebako/fs/backend_factory.h`](../include/tebako/fs/backend_factory.h):
- Static factory class (no singleton, no state)
- Methods: `create_from_file()`, `create_dwarfs()`, `create_zip()`, `create_squashfs()`
- Format detection: `is_dwarfs_format()`, `is_zip_format()`, `is_squashfs_format()`
- **CRITICAL**: DwarFS magic is "DWARFS" (6 bytes) at offset 0

Create [`src/backend_factory.cpp`](../src/backend_factory.cpp):
- Implement all factory methods
- Magic number detection with proper byte comparisons
- Case-insensitive extension checking as fallback

#### Task 1.4: Factory Tests

Create [`tests/test_backend_factory.cpp`](../tests/test_backend_factory.cpp):
- Test factory creation methods
- Test magic number detection (create temp files with magic bytes)
- Test auto-detection logic
- Test extension fallback
- Test error cases (non-existent files, unknown formats)

#### Task 1.5: Update CMakeLists.txt

Add to library sources:
```cmake
target_sources(tfs PRIVATE
    src/backend_factory.cpp
)
```

Add test executable:
```cmake
add_executable(test_backend_factory
    tests/test_backend_factory.cpp
)
target_link_libraries(test_backend_factory PRIVATE tfs ${GTestMain})
gtest_add_tests(TARGET test_backend_factory)
```

**Day 1 Success Criteria**:
- ✅ `vcpkg install` succeeds for both libzip and squashfs-tools-ng
- ✅ BackendFactory compiles without warnings
- ✅ All factory tests pass
- ✅ Format detection verified with test files

---

### Day 2: ZIP Backend Core Implementation

#### Task 2.1: ZIP Backend Header

Create [`include/tebako/fs/backends/zip_backend.h`](../include/tebako/fs/backends/zip_backend.h):
```cpp
#pragma once

#include <tebako/fs/filesystem.h>
#include <memory>
#include <shared_mutex>

// Forward declare libzip types
struct zip;
struct zip_file;

namespace tebako {
namespace fs {

class ZipBackend : public FileSystem {
 public:
  ZipBackend();
  ~ZipBackend() override;

  // Lifecycle
  bool mount(const std::string& archive_path,
            const std::string& mount_point) override;
  void unmount() override;
  bool is_mounted() const override;

  // File operations
  std::unique_ptr<FileHandle> open(const std::string& path,
                                  int flags) override;
  bool exists(const std::string& path) const override;
  bool is_file(const std::string& path) const override;
  bool is_directory(const std::string& path) const override;

  // Directory operations
  std::unique_ptr<DirectoryIterator> list_directory(
      const std::string& path) override;

  // Metadata
  int64_t file_size(const std::string& path) const override;
  time_t modification_time(const std::string& path) const override;
  mode_t permissions(const std::string& path) const override;

  // Backend info
  std::string backend_name() const override { return "ZIP"; }
  std::string backend_version() const override;
  std::string archive_path() const override { return archive_path_; }
  std::string mount_point() const override { return mount_point_; }

  static bool can_handle(const std::string& path);

 private:
  struct zip* archive_;
  std::string archive_path_;
  std::string mount_point_;
  mutable std::shared_mutex mutex_;

  int64_t locate_entry(const std::string& path) const;
  std::string strip_mount_point(const std::string& path) const;
};

}  // namespace fs
}  // namespace tebako
```

#### Task 2.2: ZIP Backend Implementation

Create [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp):

**Structure**:
1. `ZipFileHandle` class (implements `FileHandle`)
   - Constructor: Opens ZIP entry via `zip_fopen_index()`
   - `read()`: Uses `zip_fread()`
   - `seek()`: **Important**: ZIP doesn't support native seeking - must close/reopen and skip forward
   - `tell()`, `eof()`, `close()`, `path()`, `size()`

2. `ZipDirectoryIterator` class (implements `DirectoryIterator`)
   - Constructor: Scans ZIP entries, builds directory listing
   - Must handle directory entries (trailing `/`)
   - Filters entries to show only immediate children
   - `has_next()`, `next()`, `reset()`

3. `ZipBackend` class:
   - `mount()`: Open with `zip_open()`, validate archive
   - `unmount()`: Close with `zip_close()`
   - `is_mounted()`: Check `archive_` pointer
   - `open()`: Locate entry, return `ZipFileHandle`
   - `exists()`: Use `locate_entry()` with `zip_name_locate()`
   - `is_file()` / `is_directory()`: Check for trailing `/` in name
   - `list_directory()`: Return `ZipDirectoryIterator`
   - `file_size()` / `modification_time()`: Use `zip_stat_index()`
   - `permissions()`: Return defaults (ZIP doesn't store POSIX permissions reliably)

**Thread Safety**: Use `std::shared_mutex` for read/write locking.

**Day 2 Success Criteria**:
- ✅ ZIP backend compiles
- ✅ Can mount and unmount a simple ZIP file
- ✅ Basic file read operations work
- ✅ No crashes or undefined behavior

---

### Day 3: ZIP Backend Testing

#### Task 3.1: Create Test Archives

In `tests/test_files/`, create test ZIP archives:
```bash
cd tests/test_files
mkdir -p zip_test_content/subdir
echo "Test file content" > zip_test_content/test.txt
echo "Nested content" > zip_test_content/subdir/nested.txt
zip -r test.zip zip_test_content/
```

#### Task 3.2: Implement Comprehensive Tests

Create [`tests/test_zip_backend.cpp`](../tests/test_zip_backend.cpp):

**Test Categories**:
1. **Lifecycle Tests**:
   - Mount/unmount success
   - Double mount prevention
   - Unmount cleanup

2. **File Operation Tests**:
   - `exists()` for files and directories
   - `is_file()` / `is_directory()` distinction
   - `open()` and read full file
   - `open()` and read partial file
   - Seek operations
   - Multiple concurrent file handles

3. **Directory Tests**:
   - `list_directory()` correctness
   - Empty directory handling
   - Deep directory structures
   - Directory entry metadata

4. **Metadata Tests**:
   - `file_size()` accuracy
   - `modification_time()` retrieval
   - `permissions()` defaults

5. **Error Handling Tests**:
   - Non-existent files
   - Invalid paths
   - Corrupted ZIP archives
   - Permission denied scenarios

6. **Thread Safety Tests**:
   - Concurrent reads from multiple threads
   - Mount/unmount during active reads
   - Stress test with 100+ concurrent threads

#### Task 3.3: Memory Leak Testing

Run valgrind on all tests:
```bash
valgrind --leak-check=full --show-leak-kinds=all \
  build/tests/test_zip_backend
```

Fix any leaks discovered.

**Day 3 Success Criteria**:
- ✅ All ZIP backend tests pass (100%)
- ✅ Zero memory leaks
- ✅ Thread sanitizer clean
- ✅ Coverage ≥95%

---

### Day 4: ZIP Integration

#### Task 4.1: Factory Integration

Update [`src/backend_factory.cpp`](../src/backend_factory.cpp):
```cpp
#include <tebako/fs/backends/zip_backend.h>

std::unique_ptr<FileSystem> BackendFactory::create_zip() {
  return std::make_unique<ZipBackend>();
}
```

#### Task 4.2: Multi-Backend Tests

Create [`tests/test_multi_backend.cpp`](../tests/test_multi_backend.cpp):
- Mount DwarFS at `/mnt/dwarfs`
- Mount ZIP at `/mnt/zip`
- Test path resolution
- Test independent operations
- Test no resource conflicts

#### Task 4.3: Performance Benchmarking

Create simple benchmark:
- Time 1000 file opens
- Time 1000 directory listings
- Compare to raw libzip performance
- Target: < 10% overhead

**Day 4 Success Criteria**:
- ✅ ZIP backend fully integrated
- ✅ Multi-backend mounting works
- ✅ Performance acceptable
- ✅ No regressions in existing tests

---

### Day 5: SquashFS Backend Core

#### Task 5.1: SquashFS Backend Header

Create [`include/tebako/fs/backends/squashfs_backend.h`](../include/tebako/fs/backends/squashfs_backend.h):
Similar structure to `ZipBackend`, but with squashfs-tools-ng types:
```cpp
// Forward declares
struct sqfs_file_t;
struct sqfs_super_t;
struct sqfs_dir_reader_t;
struct sqfs_inode_t;
```

#### Task 5.2: SquashFS Backend Implementation

Create [`src/backends/squashfs_backend.cpp`](../src/backends/squashfs_backend.cpp):

**Key SquashFS API Functions** (from squashfs-tools-ng):
- `sqsh_archive_open()` - Open archive
- `sqsh_inode_from_path()` - Lookup by path
- `sqsh_file_reader_new()` - Create file reader
- `sqsh_directory_iterator_new()` - Iterate directory
- `sqsh_archive_close()` - Close archive

**Structure**:
1. `SquashFSFileHandle` class
2. `SquashFSDirectoryIterator` class
3. `SquashFSBackend` class

**Note**: SquashFS has better native support than ZIP:
- Native random access (no seek workaround needed)
- Proper POSIX permissions
- Efficient directory iteration

**Day 5 Success Criteria**:
- ✅ SquashFS backend compiles
- ✅ Can mount/unmount SquashFS images
- ✅ Basic operations work

---

### Day 6: SquashFS Testing

#### Task 6.1: Create Test Archives

```bash
cd tests/test_files
# Requires squashfs-tools-ng installed
sqfstar test.sqfs test_filesystem/
```

#### Task 6.2: Comprehensive Tests

Create [`tests/test_squashfs_backend.cpp`](../tests/test_squashfs_backend.cpp):
- Parallel structure to ZIP tests
- All same test categories
- SquashFS-specific tests (compression types, etc.)

**Day 6 Success Criteria**:
- ✅ All SquashFS tests pass
- ✅ Zero memory leaks
- ✅ Thread-safe
- ✅ Performance good

---

### Day 7: Full Integration

#### Task 7.1: Complete Factory

Update factory to include SquashFS:
```cpp
std::unique_ptr<FileSystem> BackendFactory::create_squashfs() {
  return std::make_unique<SquashFSBackend>();
}
```

#### Task 7.2: Comprehensive Integration Tests

Extend [`tests/test_multi_backend.cpp`](../tests/test_multi_backend.cpp):
- Mount all three backends simultaneously
- Test path resolution with priority
- Stress test with high concurrency
- Memory usage analysis

#### Task 7.3: Performance Benchmarking

- Benchmark all three backends
- Compare against raw library performance
- Optimize if needed

**Day 7 Success Criteria**:
- ✅ All backends work together
- ✅ All tests pass
- ✅ Performance targets met
- ✅ Memory usage acceptable

---

### Day 8: Documentation & Release

#### Task 8.1: Update README.adoc

Add sections for:
- Stage 2 features (ZIP, SquashFS)
- Backend architecture overview
- Usage examples
- Build instructions with vcpkg overlay

#### Task 8.2: Create Developer Guide

New file: `docs/BACKEND_DEVELOPER_GUIDE.md`
- How to add a new backend
- Interface requirements
- Testing requirements
- Integration checklist

#### Task 8.3: Archive Old Documentation

Move to `docs/archive/`:
- `STAGE_2_IMPLEMENTATION.md`
- `STAGE_2_WEEK1_DAY2_PROMPT.md`
- Any temporary planning docs
- Outdated design docs

#### Task 8.4: Final Verification

```bash
# Clean build
rm -rf build deps

# Configure
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=${PWD}/vcpkg-overlay \
  -DWITH_TESTS=ON

# Build
cmake --build build -j$(nproc)

# Test
cd build && ctest --verbose

# Verify
ldd build/libtfs.a  # Should be static
```

**Day 8 Success Criteria**:
- ✅ Documentation complete
- ✅ All platforms build successfully
- ✅ All tests pass
- ✅ Ready for production

---

## Critical Implementation Notes

### Format Detection Magic Bytes

```cpp
// CONFIRMED via /Users/mulgogi/src/external/dwarfs source analysis
constexpr uint8_t DWARFS_MAGIC[] = {'D', 'W', 'A', 'R', 'F', 'S'};  // 6 bytes at offset 0
constexpr uint8_t ZIP_LOCAL_MAGIC[] = {0x50, 0x4B, 0x03, 0x04};    // PK\x03\x04
constexpr uint8_t SQUASHFS_MAGIC_LE[] = {0x68, 0x73, 0x71, 0x73};  // "hsqs"
constexpr uint8_t SQUASHFS_MAGIC_BE[] = {0x73, 0x71, 0x73, 0x68};  // "sqsh"
```

### Thread Safety Pattern

All backends must use `std::shared_mutex`:
```cpp
// Read operations
std::shared_lock lock(mutex_);

// Write operations (mount/unmount)
std::unique_lock lock(mutex_);
```

### Error Handling

- Use `std::error_code` for operations that can fail
- Never throw from destructors
- Always validate parameters
- Provide clear error messages

### Memory Management

- Use RAII for all resources
- `std::unique_ptr` for exclusive ownership
- `std::shared_ptr` only when needed
- Never use raw `new`/`delete`

---

## Success Metrics

- **All tests pass**: 100% pass rate
- **No memory leaks**: valgrind clean
- **Thread-safe**: ThreadSanitizer clean
- **Performance**: < 10% overhead
- **Coverage**: ≥95% for new code
- **Documentation**: Complete and accurate

---

## Resources

- **DwarFS Source**: `/Users/mulgogi/src/external/dwarfs/`
- **libzip Docs**: https://libzip.org/documentation/
- **squashfs-tools-ng**: https://github.com/AgentD/squashfs-tools-ng
- **vcpkg Docs**: https://vcpkg.io/en/
- **Continuation Plan**: [`STAGE_2_CONTINUATION_PLAN.md`](STAGE_2_CONTINUATION_PLAN.md)

---

**Begin**: Day 1, Task 1.1 - Create vcpkg overlay
**End Goal**: Full multi-backend VFS with ZIP and SquashFS support
**Timeline**: 8 days
**Mode**: Code (implementation)