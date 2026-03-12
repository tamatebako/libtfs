# Stage 2 Continuation: Day 2+ Implementation

**Current Status**: Day 1 Complete ✅  
**Next Phase**: ZIP Backend Core Implementation  
**Target**: Complete multi-backend VFS with ZIP and SquashFS support  
**Timeline**: 7 days remaining (Days 2-8)  

---

## Context Summary

You are continuing Stage 2 implementation of the Tebako filesystem library (libtfs), which adds support for ZIP and SquashFS archive formats alongside the existing DwarFS support.

### Completed (Day 1) ✅

1. **vcpkg Overlay**: Created custom port for squashfs-tools-ng at [`vcpkg-overlay/squashfs-tools-ng/`](../vcpkg-overlay/squashfs-tools-ng/)
2. **Dependencies**: Updated [`vcpkg.json`](../vcpkg.json) with libzip and squashfs-tools-ng
3. **BackendFactory**: Implemented static factory with format auto-detection
   - Header: [`include/tebako/fs/backend_factory.h`](../include/tebako/fs/backend_factory.h)
   - Implementation: [`src/backend_factory.cpp`](../src/backend_factory.cpp)
   - Tests: [`tests/test_backend_factory.cpp`](../tests/test_backend_factory.cpp)
4. **Build Integration**: Updated [`CMakeLists.txt`](../CMakeLists.txt) with factory sources and tests

### Architecture Overview

**Core Interfaces** (Already Defined):
- [`FileSystem`](../include/tebako/fs/filesystem.h): Abstract backend interface
- [`FileHandle`](../include/tebako/fs/file_handle.h): File reading interface
- [`DirectoryIterator`](../include/tebako/fs/directory_iterator.h): Directory traversal interface

**Factory Pattern**:
- Static factory (no singleton, no state)
- Format auto-detection via magic numbers + extensions
- Thread-safe stateless design

**Format Detection Magic Bytes**:
```cpp
DwarFS:    "DWARFS" (6 bytes) at offset 0
ZIP:       0x50,0x4B,0x03,0x04 (PK\x03\x04)
SquashFS:  0x68,0x73,0x71,0x73 ("hsqs" LE) or 0x73,0x71,0x73,0x68 ("sqsh" BE)
```

---

## Your Task: Day 2 - ZIP Backend Core Implementation

### Objective
Implement a fully functional ZIP backend that satisfies the [`FileSystem`](../include/tebako/fs/filesystem.h) interface using libzip.

### Prerequisites Checklist
- ✅ VFS interfaces defined
- ✅ BackendFactory ready
- ✅ vcpkg configured with libzip
- ✅ Test infrastructure in place
- ✅ Build system configured

### Implementation Steps

#### Step 1: Create ZipBackend Header

**File**: `include/tebako/fs/backends/zip_backend.h`

**Requirements**:
1. Include [`filesystem.h`](../include/tebako/fs/filesystem.h) for FileSystem interface
2. Forward declare libzip types (avoid exposing libzip in public API)
3. Implement all FileSystem interface methods
4. Use `std::shared_mutex` for thread safety (read/write locking)
5. Private helper methods for path manipulation

**Key Design Points**:
- ZIP doesn't support native seeking → implement seek by close/reopen + skip
- ZIP doesn't store POSIX permissions reliably → provide sensible defaults
- Thread-safe: use `std::shared_lock` for reads, `std::unique_lock` for writes
- RAII: Constructor/destructor manage resources, no manual cleanup needed

**Structure**:
```cpp
namespace tebako {
namespace fs {

class ZipBackend : public FileSystem {
 public:
  ZipBackend();
  ~ZipBackend() override;
  
  // Lifecycle (FileSystem interface)
  bool mount(const std::string& archive_path, const std::string& mount_point) override;
  void unmount() override;
  bool is_mounted() const override;
  
  // File operations (FileSystem interface)
  std::unique_ptr<FileHandle> open(const std::string& path, int flags) override;
  bool exists(const std::string& path) const override;
  bool is_file(const std::string& path) const override;
  bool is_directory(const std::string& path) const override;
  
  // Directory operations (FileSystem interface)
  std::unique_ptr<DirectoryIterator> list_directory(const std::string& path) override;
  
  // Metadata (FileSystem interface)
  int64_t file_size(const std::string& path) const override;
  time_t modification_time(const std::string& path) const override;
  mode_t permissions(const std::string& path) const override;
  
  // Backend info (FileSystem interface)
  std::string backend_name() const override { return "ZIP"; }
  std::string backend_version() const override;
  std::string archive_path() const override { return archive_path_; }
  std::string mount_point() const override { return mount_point_; }
  
 private:
  struct zip* archive_;  // Forward declared libzip type
  std::string archive_path_;
  std::string mount_point_;
  mutable std::shared_mutex mutex_;
  
  int64_t locate_entry(const std::string& path) const;
  std::string strip_mount_point(const std::string& path) const;
};

}  // namespace fs
}  // namespace tebako
```

#### Step 2: Implement ZipFileHandle Class

**Location**: `src/backends/zip_backend.cpp` (nested class or separate)

**Requirements**:
1. Implement [`FileHandle`](../include/tebako/fs/file_handle.h) interface
2. Use `zip_fopen_index()` to open files
3. Use `zip_fread()` for reading
4. Implement seek by close/reopen + skip (ZIP limitation)
5. Track current position for `tell()` and `eof()`

**Key Methods**:
```cpp
class ZipFileHandle : public FileHandle {
 public:
  ZipFileHandle(zip* archive, int64_t index, const std::string& path);
  ~ZipFileHandle() override;
  
  ssize_t read(void* buffer, size_t count) override;
  off_t seek(off_t offset, int whence) override;
  off_t tell() const override;
  bool eof() const override;
  void close() override;
  std::string path() const override;
  int64_t size() const override;
  
 private:
  zip_file* file_;
  zip* archive_;
  int64_t index_;
  std::string path_;
  int64_t size_;
  int64_t current_pos_;
  bool eof_;
};
```

#### Step 3: Implement ZipDirectoryIterator Class

**Location**: `src/backends/zip_backend.cpp` (nested class or separate)

**Requirements**:
1. Implement [`DirectoryIterator`](../include/tebako/fs/directory_iterator.h) interface
2. Scan ZIP entries to build directory listing
3. Filter to show only immediate children (not recursive)
4. Handle directory entries (those ending with `/`)
5. Exclude `.` and `..` pseudo-entries

**Key Methods**:
```cpp
class ZipDirectoryIterator : public DirectoryIterator {
 public:
  ZipDirectoryIterator(zip* archive, const std::string& dir_path);
  
  bool has_next() const override;
  DirectoryEntry next() override;
  void reset() override;
  
 private:
  std::vector<DirectoryEntry> entries_;
  size_t current_index_;
};
```

#### Step 4: Implement ZipBackend Core Methods

**File**: `src/backends/zip_backend.cpp`

**Critical Implementation Notes**:

**mount() method**:
```cpp
bool ZipBackend::mount(const std::string& archive_path,
                       const std::string& mount_point) {
  std::unique_lock lock(mutex_);
  
  if (archive_) return false;  // Already mounted
  
  int error;
  archive_ = zip_open(archive_path.c_str(), ZIP_RDONLY, &error);
  if (!archive_) return false;
  
  archive_path_ = archive_path;
  mount_point_ = mount_point;
  return true;
}
```

**unmount() method**:
```cpp
void ZipBackend::unmount() {
  std::unique_lock lock(mutex_);
  
  if (archive_) {
    zip_close(archive_);
    archive_ = nullptr;
  }
  
  archive_path_.clear();
  mount_point_.clear();
}
```

**open() method**:
```cpp
std::unique_ptr<FileHandle> ZipBackend::open(const std::string& path, int flags) {
  std::shared_lock lock(mutex_);
  
  if (!archive_) return nullptr;
  
  auto rel_path = strip_mount_point(path);
  int64_t index = locate_entry(rel_path);
  if (index < 0) return nullptr;
  
  return std::make_unique<ZipFileHandle>(archive_, index, path);
}
```

**exists() method**:
```cpp
bool ZipBackend::exists(const std::string& path) const {
  std::shared_lock lock(mutex_);
  
  if (!archive_) return false;
  
  auto rel_path = strip_mount_point(path);
  return locate_entry(rel_path) >= 0;
}
```

**locate_entry() helper**:
```cpp
int64_t ZipBackend::locate_entry(const std::string& path) const {
  // Use zip_name_locate() to find entry by name
  // Return index or -1 if not found
}
```

**strip_mount_point() helper**:
```cpp
std::string ZipBackend::strip_mount_point(const std::string& path) const {
  // Remove mount_point_ prefix from path
  // Handle both "/mnt/app/file.txt" -> "file.txt"
}
```

#### Step 5: Update BackendFactory

**File**: [`src/backend_factory.cpp`](../src/backend_factory.cpp:99)

Update the `create_zip()` method to return the actual backend:

```cpp
std::unique_ptr<FileSystem> BackendFactory::create_zip() {
  return std::make_unique<ZipBackend>();
}
```

Add include:
```cpp
#include <tebako/fs/backends/zip_backend.h>
```

#### Step 6: Update CMakeLists.txt

**File**: [`CMakeLists.txt`](../CMakeLists.txt:871)

Add ZIP backend sources:
```cmake
add_library(tfs STATIC
    # ... existing sources ...
    "src/backend_factory.cpp"
    "src/backends/zip_backend.cpp"  # NEW
    # ... existing headers ...
)
```

Create backends directory:
```bash
mkdir -p src/backends
```

#### Step 7: Link libzip

**In CMakeLists.txt**, add:
```cmake
# Find libzip (provided by vcpkg)
find_package(libzip CONFIG REQUIRED)

# Link with tfs library
target_link_libraries(tfs PUBLIC libzip::zip)
```

### Success Criteria for Day 2

- ✅ ZIP backend compiles without errors or warnings
- ✅ Can successfully mount a ZIP archive
- ✅ Can read file contents from ZIP
- ✅ Can list directory contents
- ✅ Can query file metadata
- ✅ Thread-safe concurrent access
- ✅ Proper resource cleanup (no leaks)
- ✅ Code follows architectural principles (OOP, MECE, separation of concerns)

### Testing Strategy (Day 3)

After Day 2 implementation, Day 3 will focus on comprehensive testing:

1. **Create Test Archives**: Generate test ZIP files with known structure
2. **Lifecycle Tests**: Mount/unmount, double-mount prevention
3. **File Operation Tests**: Read full files, partial reads, seek operations
4. **Directory Tests**: List directories, nested directories, empty directories
5. **Metadata Tests**: File size, modification time, permissions
6. **Error Handling**: Non-existent files, corrupted archives, edge cases
7. **Thread Safety**: Concurrent reads from multiple threads

---

## Architectural Principles (CRITICAL)

### Object-Oriented Design
- Each class has **one responsibility**
- Clear inheritance hierarchy: ZipBackend → FileSystem
- Encapsulation: libzip details hidden from public API
- Polymorphism: All backends satisfy FileSystem interface

### MECE (Mutually Exclusive, Collectively Exhaustive)
- Each backend handles one format exclusively
- All supported formats collectively covered
- No overlapping responsibilities

### Separation of Concerns
- ZipBackend: ZIP-specific logic
- ZipFileHandle: File reading logic
- ZipDirectoryIterator: Directory traversal logic
- BackendFactory: Backend creation logic

### Open/Closed Principle
- Open for extension: Add SquashFS backend without modifying ZIP backend
- Closed for modification: Existing code stable when adding features

### Thread Safety
- Use `std::shared_mutex` for read/write locking
- `std::shared_lock` for read operations (allows concurrent reads)
- `std::unique_lock` for write operations (exclusive access)
- No race conditions, no deadlocks

### Memory Management
- Use RAII: Constructor acquires, destructor releases
- Use `std::unique_ptr` for owned objects
- Use `std::shared_ptr` only when necessary
- No raw `new`/`delete`

---

## Common Pitfalls to Avoid

### ❌ DON'T
- Don't expose libzip types in public headers
- Don't use raw pointers for ownership
- Don't assume ZIP has native seek support
- Don't forget thread safety locks
- Don't leak zip_file* or zip* handles
- Don't use global state or singletons

### ✅ DO
- Use forward declarations for libzip types
- Use `std::unique_ptr` and `std::shared_ptr`
- Implement seek via close/reopen + skip
- Use `std::shared_mutex` for concurrent access
- Close all handles in destructors
- Use stateless factory pattern

---

## Implementation Checklist

### Day 2: ZIP Backend Core
- [ ] Create `include/tebako/fs/backends/zip_backend.h`
- [ ] Implement `src/backends/zip_backend.cpp`
- [ ] Implement `ZipFileHandle` class
- [ ] Implement `ZipDirectoryIterator` class
- [ ] Update `BackendFactory::create_zip()`
- [ ] Update `CMakeLists.txt` with ZIP sources
- [ ] Link libzip library
- [ ] Verify compilation (no errors/warnings)
- [ ] Basic manual testing (mount, read, list)

### Day 3: ZIP Backend Testing
- [ ] Create test ZIP archives
- [ ] Implement lifecycle tests
- [ ] Implement file operation tests
- [ ] Implement directory operation tests
- [ ] Implement metadata tests
- [ ] Implement error handling tests
- [ ] Implement thread safety tests
- [ ] Run valgrind (zero leaks)
- [ ] All tests passing (100%)

### Day 4: ZIP Integration
- [ ] Integrate with BackendFactory
- [ ] Multi-backend integration tests
- [ ] Mixed DwarFS/ZIP mounts
- [ ] Performance benchmarking
- [ ] Documentation

### Days 5-6: SquashFS Backend
- [ ] Similar implementation to ZIP backend
- [ ] SquashFS-specific optimizations
- [ ] Comprehensive testing
- [ ] Integration

### Day 7: Integration & Optimization
- [ ] All three backends working together
- [ ] Stress testing
- [ ] Performance optimization
- [ ] Memory usage analysis

### Day 8: Documentation & Release
- [ ] Update README.adoc
- [ ] Create Backend Developer Guide
- [ ] Archive outdated docs
- [ ] Final verification builds
- [ ] Release notes

---

## Build Commands

### Configure with vcpkg overlay
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=${PWD}/vcpkg-overlay \
  -DWITH_TESTS=ON
```

### Build
```bash
cmake --build build -j$(nproc)
```

### Test specific test
```bash
cd build && ctest -R test_backend_factory --verbose
```

### Memory check
```bash
valgrind --leak-check=full --show-leak-kinds=all \
  build/tests/test_backend_factory
```

---

## References

### Documentation
- Stage 2 Plan: [`STAGE_2_CONTINUATION_PLAN.md`](STAGE_2_CONTINUATION_PLAN.md)
- Day 1 Status: [`STAGE_2_DAY1_COMPLETION_STATUS.md`](STAGE_2_DAY1_COMPLETION_STATUS.md)
- Factory Design: [`STAGE_2_BACKEND_FACTORY_DESIGN.md`](STAGE_2_BACKEND_FACTORY_DESIGN.md)
- VFS Design: [`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md)

### Source Code
- FileSystem Interface: [`filesystem.h`](../include/tebako/fs/filesystem.h)
- FileHandle Interface: [`file_handle.h`](../include/tebako/fs/file_handle.h)
- DirectoryIterator Interface: [`directory_iterator.h`](../include/tebako/fs/directory_iterator.h)
- BackendFactory: [`backend_factory.h`](../include/tebako/fs/backend_factory.h) | [`backend_factory.cpp`](../src/backend_factory.cpp)
- Factory Tests: [`test_backend_factory.cpp`](../tests/test_backend_factory.cpp)

### External Libraries
- libzip documentation: https://libzip.org/documentation/
- libzip API: https://libzip.org/documentation/libzip.html
- squashfs-tools-ng: https://github.com/AgentD/squashfs-tools-ng

---

## Contact & Support

**Mode**: Code  
**Task**: Implement ZIP Backend (Day 2)  
**Timeline**: 1 day for core implementation  
**Next**: Day 3 testing, Day 4 integration  

**Ready to Begin**: All prerequisites met ✅  
**Start with**: Create `include/tebako/fs/backends/zip_backend.h`  

---

**Document Version**: 1.0  
**Last Updated**: 2025-12-22  
**Status**: Ready for Day 2 Implementation  
**Continuation from**: Day 1 Complete ✅