# Stage 2 Day 5: SquashFS Backend Core Implementation - Continuation Prompt

**Date**: 2025-12-23 (Next Business Day)
**Mode**: Code
**Duration**: 1 day (8 hours)
**Prerequisites**: Week 1 Complete ✅

---

## Context

You are continuing Stage 2 implementation of the Tebako filesystem library (libtfs). Week 1 (Days 1-4) successfully delivered a production-ready ZIP backend with comprehensive testing and documentation. Now you're beginning Week 2 by implementing the SquashFS backend.

**Week 1 Achievements**:
- ✅ Day 1: VFS abstraction layer, BackendFactory
- ✅ Day 2: ZIP backend implementation (795 lines)
- ✅ Day 3: Comprehensive unit testing (47 tests)
- ✅ Day 4: Integration testing (13 tests) + documentation

**Day 5 Objective**: Implement core SquashFS backend functionality following the proven ZIP backend pattern.

---

## What Has Been Completed

### VFS Infrastructure ✅

- **FileSystem Interface**: [`include/tebako/fs/filesystem.h`](../include/tebako/fs/filesystem.h)
- **FileHandle Interface**: [`include/tebako/fs/file_handle.h`](../include/tebako/fs/file_handle.h)
- **DirectoryIterator Interface**: [`include/tebako/fs/directory_iterator.h`](../include/tebako/fs/directory_iterator.h)
- **BackendFactory**: [`include/tebako/fs/backend_factory.h`](../include/tebako/fs/backend_factory.h)

### ZIP Backend Pattern ✅

Complete reference implementation:
- **Header**: [`include/tebako/fs/backends/zip_backend.h`](../include/tebako/fs/backends/zip_backend.h)
- **Implementation**: [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp)
- **Tests**: [`tests/test_zip_backend.cpp`](../tests/test_zip_backend.cpp)

### Build System ✅

- squashfs-tools-ng in [`vcpkg.json`](../vcpkg.json)
- vcpkg overlay at [`vcpkg-overlay/ports/squashfs-tools-ng/`](../vcpkg-overlay/ports/squashfs-tools-ng/)
- CMakeLists.txt configured for tests

---

## Your Task: Day 5 - SquashFS Backend Core Implementation

### Objective

Implement SquashFSBackend, SquashFSFileHandle, and SquashFSDirectoryIterator classes following the ZIP backend architecture pattern. By end of day, have a functional SquashFS backend ready for testing.

### Success Criteria

- ✅ SquashFSBackend class implements FileSystem interface
- ✅ SquashFSFileHandle class implements FileHandle interface
- ✅ SquashFSDirectoryIterator class implements DirectoryIterator interface
- ✅ Backend integrated with BackendFactory
- ✅ Basic manual testing passes
- ✅ All code compiles without errors
- ✅ Code follows architectural patterns from ZIP backend

---

## Implementation Steps

### Step 1: Study ZIP Backend Architecture (1 hour)

Before writing code, thoroughly understand the pattern:

1. **Read the ZIP backend implementation**:
   ```bash
   # Study these files in order:
   cat include/tebako/fs/backends/zip_backend.h
   cat src/backends/zip_backend.cpp
   ```

2. **Key architectural patterns to note**:
   - Thread safety via `std::shared_mutex`
   - RAII for resource management
   - Path normalization (strip mount point prefix)
   - Error handling (return nullptr/false/-1, no exceptions)
   - Three-class structure (Backend, FileHandle, Iterator)

3. **Note the squashfs-tools-ng API differences**:
   ```bash
   # Review squashfs-tools-ng headers
   find /path/to/vcpkg/installed -name "*.h" | grep squashfs
   ```

### Step 2: Create SquashFS Backend Header (1.5 hours)

Create [`include/tebako/fs/backends/squashfs_backend.h`](../include/tebako/fs/backends/squashfs_backend.h):

**Required classes** (follow ZIP pattern):

```cpp
namespace tebako {
namespace fs {

// Forward declarations
class SquashFSFileHandle;
class SquashFSDirectoryIterator;

/**
 * @brief SquashFS filesystem backend using squashfs-tools-ng
 */
class SquashFSBackend : public FileSystem {
 public:
  SquashFSBackend();
  ~SquashFSBackend() override;

  // Lifecycle (implement these)
  bool mount(const std::string& archive_path,
             const std::string& mount_point) override;
  void unmount() override;
  bool is_mounted() const override;

  // File operations (implement these)
  std::unique_ptr<FileHandle> open(const std::string& path,
                                    int flags) override;
  bool exists(const std::string& path) const override;
  bool is_file(const std::string& path) const override;
  bool is_directory(const std::string& path) const override;

  // Directory operations (implement these)
  std::unique_ptr<DirectoryIterator> list_directory(
      const std::string& path) override;

  // Metadata operations (implement these)
  int64_t file_size(const std::string& path) const override;
  time_t modification_time(const std::string& path) const override;
  mode_t permissions(const std::string& path) const override;

  // Backend info (implement these)
  std::string backend_name() const override;
  std::string backend_version() const override;
  std::string archive_path() const override;
  std::string mount_point() const override;

 private:
  // TODO: Add squashfs-tools-ng handle type here
  // sqfs_t* sqfs_;  // or whatever the correct type is

  mutable std::shared_mutex mutex_;
  std::string archive_path_;
  std::string mount_point_;
  bool mounted_;

  // Helper methods
  std::string normalize_path(const std::string& path) const;
  // TODO: Add other helpers as needed
};

/**
 * @brief File handle for reading from SquashFS archives
 */
class SquashFSFileHandle : public FileHandle {
 public:
  SquashFSFileHandle(/* parameters */);
  ~SquashFSFileHandle() override;

  // Implement FileHandle interface
  ssize_t read(void* buffer, size_t size) override;
  off_t seek(off_t offset, int whence) override;
  off_t tell() const override;
  bool eof() const override;
  int64_t size() const override;
  void close() override;
  std::string path() const override;

 private:
  // TODO: Add squashfs file handle
  // TODO: Add state tracking
};

/**
 * @brief Directory iterator for SquashFS archives
 */
class SquashFSDirectoryIterator : public DirectoryIterator {
 public:
  SquashFSDirectoryIterator(/* parameters */);
  ~SquashFSDirectoryIterator() override;

  // Implement DirectoryIterator interface
  bool has_next() override;
  DirectoryEntry next() override;
  void reset() override;

 private:
  // TODO: Add squashfs directory reading state
};

}  // namespace fs
}  // namespace tebako
```

**Key Points**:
- Copy structure from [`zip_backend.h`](../include/tebako/fs/backends/zip_backend.h)
- Replace `zip_*` types with `sqfs_*` types
- Keep the same public API (FileSystem interface)
- Use same thread safety pattern (shared_mutex)

### Step 3: Implement SquashFSBackend Core (2 hours)

Create [`src/backends/squashfs_backend.cpp`](../src/backends/squashfs_backend.cpp):

**Implementation order**:

1. **Constructor/Destructor** (15 min)
   ```cpp
   SquashFSBackend::SquashFSBackend()
       : mounted_(false) {
     // Initialize
   }

   SquashFSBackend::~SquashFSBackend() {
     if (mounted_) {
       unmount();
     }
   }
   ```

2. **mount() method** (30 min)
   - Open SquashFS archive using squashfs-tools-ng API
   - Store archive handle
   - Set mounted_ = true
   - Store paths
   - Use `std::unique_lock` for write access

3. **unmount() method** (15 min)
   - Close SquashFS archive
   - Release resources
   - Reset state

4. **Backend info methods** (15 min)
   - `backend_name()` returns "SquashFS"
   - `backend_version()` returns squashfs-tools-ng version
   - `archive_path()` returns stored path
   - `mount_point()` returns stored mount point

5. **Helper methods** (30 min)
   - `normalize_path()` - copy from ZIP backend
   - Path validation
   - SquashFS API wrappers

6. **exists(), is_file(), is_directory()** (30 min)
   - Use squashfs-tools-ng API to check paths
   - Use `std::shared_lock` for read access
   - Return appropriate boolean

### Step 4: Implement SquashFSFileHandle (1.5 hours)

**Implementation order**:

1. **Constructor** (20 min)
   - Open file via squashfs-tools-ng
   - Initialize position tracking
   - Store file metadata

2. **read() method** (30 min)
   - Read from SquashFS file
   - Update position
   - Handle EOF
   - Return bytes read

3. **seek() method** (20 min)
   - SquashFS likely has native seek support (unlike ZIP)
   - Calculate target position based on whence
   - Use SquashFS seek API
   - Update position tracking

4. **tell(), eof(), size(), close(), path()** (20 min)
   - Implement straightforward accessors
   - Proper resource cleanup in close()

### Step 5: Implement SquashFSDirectoryIterator (1 hour)

**Implementation order**:

1. **Constructor** (20 min)
   - Open directory in SquashFS
   - Initialize iteration state
   - Store directory handle

2. **has_next() method** (15 min)
   - Check if more entries available
   - Handle end of directory

3. **next() method** (20 min)
   - Read next directory entry
   - Fill DirectoryEntry structure
   - Advance iterator

4. **reset() method** (5 min)
   - Reset iteration to beginning

### Step 6: Integrate with BackendFactory (30 minutes)

Update [`src/backend_factory.cpp`](../src/backend_factory.cpp):

```cpp
#include <tebako/fs/backends/squashfs_backend.h>

std::unique_ptr<FileSystem> BackendFactory::create_squashfs() {
  return std::make_unique<SquashFSBackend>();
}

// In create_from_file(), handle SquashFS detection:
if (is_squashfs_format(archive_path)) {
  return create_squashfs();
}
```

### Step 7: Update Build System (30 minutes)

Update [`CMakeLists.txt`](../CMakeLists.txt):

```cmake
# Add SquashFS backend source
target_sources(tfs PUBLIC
  "src/backends/squashfs_backend.cpp"
  "include/tebako/fs/backends/squashfs_backend.h"
)

# Link squashfs-tools-ng
find_package(squashfs-tools-ng CONFIG REQUIRED)
target_link_libraries(tfs PUBLIC squashfs-tools-ng::squashfs)
```

### Step 8: Manual Testing (1 hour)

Create a simple test program:

```cpp
// test_squashfs_manual.cpp
#include <tebako/fs/backend_factory.h>
#include <iostream>

int main() {
  using namespace tebako::fs;

  // Create backend
  auto backend = BackendFactory::create_squashfs();
  std::cout << "Backend: " << backend->backend_name() << std::endl;

  // Mount (requires a test .sqfs file)
  if (backend->mount("test.sqfs", "/mnt/test")) {
    std::cout << "Mounted successfully" << std::endl;

    // Test exists
    if (backend->exists("/mnt/test")) {
      std::cout << "Root exists" << std::endl;
    }

    // Test directory listing
    auto iter = backend->list_directory("/mnt/test");
    if (iter) {
      std::cout << "Listing root:" << std::endl;
      while (iter->has_next()) {
        auto entry = iter->next();
        std::cout << "  " << entry.name << std::endl;
      }
    }

    backend->unmount();
  }

  return 0;
}
```

Build and test:
```bash
cmake --build build
./build/test_squashfs_manual
```

---

## Architecture Compliance Checklist

- ✅ Follows ZIP backend three-class pattern
- ✅ Uses `std::shared_mutex` for thread safety
- ✅ RAII for all resource management
- ✅ No exceptions (return error codes)
- ✅ Path normalization (strip mount point)
- ✅ Clear separation of concerns
- ✅ Backend, FileHandle, Iterator are independent
- ✅ Implements full FileSystem interface

---

## Common Pitfalls to Avoid

1. **Don't hardcode paths** - Always use mount_point_ prefix
2. **Don't forget thread safety** - Use shared_mutex consistently
3. **Don't leak resources** - Use RAII, close in destructors
4. **Don't throw exceptions** - Return error codes
5. **Don't assume seek works like ZIP** - SquashFS may have native seek
6. **Don't skip error handling** - Check all API return values
7. **Don't copy ZIP exactly** - Adapt to SquashFS API differences

---

## Reference Files

### Must Read Before Starting

1. [`include/tebako/fs/backends/zip_backend.h`](../include/tebako/fs/backends/zip_backend.h) - Header pattern
2. [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp) - Implementation pattern
3. [`include/tebako/fs/filesystem.h`](../include/tebako/fs/filesystem.h) - Interface to implement

### Supporting Documentation

- [`docs/STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md) - Architecture overview
- [`docs/backends/ZIP_BACKEND.adoc`](backends/ZIP_BACKEND.adoc) - Pattern reference
- [`docs/STAGE_2_WEEK2_CONTINUATION_PLAN.md`](STAGE_2_WEEK2_CONTINUATION_PLAN.md) - Week 2 plan

---

## Deliverables Checklist

By end of Day 5, you should have:

- ✅ `include/tebako/fs/backends/squashfs_backend.h` created (~200 lines)
- ✅ `src/backends/squashfs_backend.cpp` created (~600 lines)
- ✅ BackendFactory updated to include SquashFS
- ✅ CMakeLists.txt updated with SquashFS target
- ✅ Manual test program working
- ✅ All code compiles without errors
- ✅ Basic functionality verified (mount, list, read)

---

## Next Steps After Day 5

**Day 6**: SquashFS Testing & Documentation
- Create 5 test fixtures
- Write 47 comprehensive unit tests
- Add integration tests
- Create SQUASHFS_BACKEND.adoc documentation
- Update README.adoc

---

## Tips for Success

1. **Start by copying ZIP backend** - It's a proven pattern
2. **Study squashfs-tools-ng API** - Understand differences from libzip
3. **Test incrementally** - Build and test after each major method
4. **Ask questions** - If SquashFS API is unclear, document and move on
5. **Keep it simple** - Don't over-engineer, follow the pattern
6. **Focus on functionality** - Optimization comes in Day 7

---

**Document Version**: 1.0
**Created**: 2025-12-22
**Mode**: Code
**Estimated Time**: 8 hours
**Start with**: Study ZIP backend architecture