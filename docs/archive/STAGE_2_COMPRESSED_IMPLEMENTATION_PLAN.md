# Stage 2: Compressed Implementation Plan - ZIP & SquashFS

**Date**: 2025-12-22
**Goal**: Complete ZIP backend and implement SquashFS backend with static compilation
**Constraint**: Use source code directly, CMake-only orchestration, static linking

---

## Current Status

### ✅ Completed (Week 1 Day 1)
- [x] Abstract FileSystem interface
- [x] FileHandle interface
- [x] DirectoryIterator interface
- [x] CMake builds successfully

### 🚧 In Progress (Week 1 Day 2)
- [ ] BackendFactory static factory implementation
- [ ] Format detection (magic numbers)

### 📋 Planned
- [ ] ZIP backend (Stage 2)
- [ ] SquashFS backend (Stage 3)

---

## Implementation Strategy

### Critical Requirements

1. **Static Compilation**: Use source code, not dynamic libraries
2. **CMake Only**: All build orchestration via CMake
3. **ExternalProject_Add**: Fetch and build dependencies from source
4. **No vcpkg dynamic linking**: Build dependencies statically in-tree

---

## Phase 1: Complete Backend Factory (Day 2)

**Timeline**: 1 day  
**Goal**: Implement static factory with format detection

### Tasks

#### 1. Implement BackendFactory
- [ ] Create `include/tebako/fs/backend_factory.h` (API defined in Week 1 Day 2 prompt)
- [ ] Create `src/backend_factory.cpp` with:
  - [ ] `create_from_file()` - Auto-detection
  - [ ] `create_dwarfs()` - Explicit DwarFS
  - [ ] `create_zip()` - Placeholder (returns nullptr)
  - [ ] `create_squashfs()` - Placeholder (returns nullptr)
  - [ ] `is_dwarfs_format()` - Magic: "DWARFS"
  - [ ] `is_zip_format()` - Magic: 0x50 0x4B 0x03 0x04
  - [ ] `is_squashfs_format()` - Magic: "hsqs" or "sqsh"
  - [ ] `read_magic_bytes()` - Helper
  - [ ] `has_extension()` - Case-insensitive helper

#### 2. Tests
- [ ] Create `tests/test_backend_factory.cpp`
- [ ] Test factory creation
- [ ] Test magic number detection
- [ ] Test auto-detection
- [ ] Test extension fallback

#### 3. CMake Integration
- [ ] Add `src/backend_factory.cpp` to library sources
- [ ] Add `tests/test_backend_factory.cpp` to test suite
- [ ] Verify build

**Success Criteria**:
- [x] All tests passing
- [x] Factory creates DwarFS backend
- [x] Format detection working
- [x] ZIP/SquashFS placeholders in place

---

## Phase 2: ZIP Backend Implementation (Days 3-6)

**Timeline**: 4 days  
**Goal**: Full ZIP backend with static libzip

### Day 3: libzip Static Integration

#### 1. Add libzip as CMake ExternalProject

File: `CMakeLists.txt`

```cmake
include(ExternalProject)

# libzip source (use specific version for stability)
set(LIBZIP_VERSION "1.10.1")
set(LIBZIP_URL "https://libzip.org/download/libzip-${LIBZIP_VERSION}.tar.xz")

ExternalProject_Add(
    _libzip
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/_libzip
    URL ${LIBZIP_URL}
    URL_HASH SHA256=dc3c8d5b4c8bbd09626864f6bcf93de701540f761782e1d0...  # actual hash
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DBUILD_SHARED_LIBS=OFF          # Static compilation
        -DENABLE_COMMONCRYPTO=OFF        # No system crypto
        -DENABLE_GNUTLS=OFF
        -DENABLE_MBEDTLS=OFF
        -DENABLE_OPENSSL=OFF             # Or ON if needed for AES
        -DENABLE_WINDOWS_CRYPTO=OFF
        -DENABLE_BZIP2=OFF               # Minimize dependencies
        -DENABLE_LZMA=OFF
        -DENABLE_ZSTD=OFF
        -DBUILD_TOOLS=OFF                # No command-line tools
        -DBUILD_REGRESS=OFF              # No tests
        -DBUILD_EXAMPLES=OFF
        -DBUILD_DOC=OFF
    BUILD_BYPRODUCTS
        <INSTALL_DIR>/lib/libzip.a
        <INSTALL_DIR>/lib/cmake/libzip/libzip-config.cmake
)

# Get install directory
ExternalProject_Get_Property(_libzip INSTALL_DIR)
set(LIBZIP_INCLUDE_DIR ${INSTALL_DIR}/include)
set(LIBZIP_LIBRARY ${INSTALL_DIR}/lib/libzip.a)

# Create imported target
add_library(libzip STATIC IMPORTED)
set_target_properties(libzip PROPERTIES
    IMPORTED_LOCATION ${LIBZIP_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${LIBZIP_INCLUDE_DIR}
)
add_dependencies(libzip _libzip)

# Link against libtfs
target_link_libraries(libtfs PRIVATE libzip)
```

#### 2. Verify libzip Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target _libzip
# Should produce build/_libzip/lib/libzip.a
```

**Success Criteria**:
- [x] libzip builds statically
- [x] No dynamic library dependencies
- [x] Headers available for compilation

---

### Day 4: ZipBackend Core Implementation

#### 1. Create Backend Files

File: `include/tebako/fs/backends/zip_backend.h`

```cpp
#pragma once

#include <tebako/fs/filesystem.h>
#include <memory>
#include <string>
#include <shared_mutex>

// Forward declare libzip types to avoid exposing in header
struct zip;
struct zip_file;

namespace tebako {
namespace fs {

/**
 * @brief ZIP filesystem backend using libzip
 */
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

  // Format detection
  static bool can_handle(const std::string& path);

 private:
  struct zip* archive_;  // libzip handle
  std::string archive_path_;
  std::string mount_point_;
  mutable std::shared_mutex mutex_;

  // Helper: resolve path to ZIP index
  int64_t locate_entry(const std::string& path) const;
  
  // Helper: strip mount point prefix
  std::string strip_mount_point(const std::string& path) const;
};

}  // namespace fs
}  // namespace tebako
```

File: `src/backends/zip_backend.cpp`

```cpp
#include <tebako/fs/backends/zip_backend.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <zip.h>  // libzip
#include <cstring>
#include <filesystem>

namespace tebako {
namespace fs {

// ===================================================================
// ZipFileHandle - Concrete file handle for ZIP entries
// ===================================================================

class ZipFileHandle : public FileHandle {
 public:
  ZipFileHandle(struct zip* archive, int64_t index, const std::string& path)
      : archive_(archive), index_(index), path_(path), position_(0) {
    // Get file stats
    struct zip_stat st;
    zip_stat_init(&st);
    zip_stat_index(archive_, index_, 0, &st);
    size_ = st.size;
    
    // Open file for reading
    file_ = zip_fopen_index(archive_, index_, 0);
  }

  ~ZipFileHandle() override {
    if (file_) {
      zip_fclose(file_);
    }
  }

  ssize_t read(void* buffer, size_t count) override {
    if (!file_) return -1;
    
    ssize_t bytes_read = zip_fread(file_, buffer, count);
    if (bytes_read > 0) {
      position_ += bytes_read;
    }
    return bytes_read;
  }

  off_t seek(off_t offset, int whence) override {
    // ZIP doesn't support seeking - must close and reopen
    // This is a limitation of libzip streaming API
    
    off_t new_position;
    switch (whence) {
      case SEEK_SET: new_position = offset; break;
      case SEEK_CUR: new_position = position_ + offset; break;
      case SEEK_END: new_position = size_ + offset; break;
      default: return -1;
    }
    
    if (new_position < 0 || new_position > size_) {
      return -1;
    }
    
    // Close current handle
    if (file_) {
      zip_fclose(file_);
    }
    
    // Reopen and skip to position
    file_ = zip_fopen_index(archive_, index_, 0);
    if (!file_) return -1;
    
    // Skip to position by reading and discarding
    const size_t SKIP_BUFFER_SIZE = 8192;
    uint8_t skip_buffer[SKIP_BUFFER_SIZE];
    off_t remaining = new_position;
    
    while (remaining > 0) {
      size_t to_skip = std::min(remaining, (off_t)SKIP_BUFFER_SIZE);
      ssize_t skipped = zip_fread(file_, skip_buffer, to_skip);
      if (skipped <= 0) break;
      remaining -= skipped;
    }
    
    position_ = new_position;
    return position_;
  }

  off_t tell() const override { return position_; }
  bool eof() const override { return position_ >= size_; }
  void close() override {
    if (file_) {
      zip_fclose(file_);
      file_ = nullptr;
    }
  }

  std::string path() const override { return path_; }
  int64_t size() const override { return size_; }

 private:
  struct zip* archive_;
  struct zip_file* file_;
  int64_t index_;
  std::string path_;
  off_t position_;
  int64_t size_;
};

// ===================================================================
// ZipDirectoryIterator - Concrete iterator for ZIP entries
// ===================================================================

class ZipDirectoryIterator : public DirectoryIterator {
 public:
  ZipDirectoryIterator(struct zip* archive, const std::string& dir_path)
      : archive_(archive), dir_path_(dir_path), current_(0) {
    
    // Normalize directory path
    std::string normalized = dir_path_;
    if (!normalized.empty() && normalized.back() != '/') {
      normalized += '/';
    }
    
    // Scan ZIP entries to build directory listing
    int64_t num_entries = zip_get_num_entries(archive_, 0);
    for (int64_t i = 0; i < num_entries; ++i) {
      const char* name = zip_get_name(archive_, i, 0);
      if (!name) continue;
      
      std::string entry_name(name);
      
      // Check if entry is in this directory
      if (entry_name.find(normalized) == 0) {
        std::string relative = entry_name.substr(normalized.length());
        
        // Skip if it's a subdirectory entry (contains /)
        size_t slash_pos = relative.find('/');
        if (slash_pos == std::string::npos || 
            slash_pos == relative.length() - 1) {
          
          // Get entry stats
          struct zip_stat st;
          zip_stat_init(&st);
          zip_stat_index(archive_, i, 0, &st);
          
          DirectoryEntry entry;
          entry.name = relative;
          if (!entry.name.empty() && entry.name.back() == '/') {
            entry.name.pop_back();  // Remove trailing slash
          }
          entry.is_directory = (slash_pos != std::string::npos);
          entry.size = st.size;
          entry.mtime = st.mtime;
          
          entries_.push_back(entry);
        }
      }
    }
  }

  bool has_next() const override {
    return current_ < entries_.size();
  }

  DirectoryEntry next() override {
    return entries_[current_++];
  }

  void reset() override {
    current_ = 0;
  }

 private:
  struct zip* archive_;
  std::string dir_path_;
  std::vector<DirectoryEntry> entries_;
  size_t current_;
};

// ===================================================================
// ZipBackend Implementation
// ===================================================================

ZipBackend::ZipBackend() : archive_(nullptr) {}

ZipBackend::~ZipBackend() {
  unmount();
}

bool ZipBackend::mount(const std::string& archive_path,
                      const std::string& mount_point) {
  std::unique_lock lock(mutex_);
  
  if (archive_) {
    return false;  // Already mounted
  }
  
  int error;
  archive_ = zip_open(archive_path.c_str(), ZIP_RDONLY, &error);
  if (!archive_) {
    return false;
  }
  
  archive_path_ = archive_path;
  mount_point_ = mount_point;
  return true;
}

void ZipBackend::unmount() {
  std::unique_lock lock(mutex_);
  
  if (archive_) {
    zip_close(archive_);
    archive_ = nullptr;
  }
  
  archive_path_.clear();
  mount_point_.clear();
}

bool ZipBackend::is_mounted() const {
  std::shared_lock lock(mutex_);
  return archive_ != nullptr;
}

std::unique_ptr<FileHandle> ZipBackend::open(const std::string& path,
                                             int flags) {
  std::shared_lock lock(mutex_);
  
  if (!archive_) return nullptr;
  
  int64_t index = locate_entry(path);
  if (index < 0) return nullptr;
  
  return std::make_unique<ZipFileHandle>(archive_, index, path);
}

bool ZipBackend::exists(const std::string& path) const {
  std::shared_lock lock(mutex_);
  if (!archive_) return false;
  return locate_entry(path) >= 0;
}

bool ZipBackend::is_file(const std::string& path) const {
  std::shared_lock lock(mutex_);
  if (!archive_) return false;
  
  int64_t index = locate_entry(path);
  if (index < 0) return false;
  
  const char* name = zip_get_name(archive_, index, 0);
  return name && name[strlen(name) - 1] != '/';
}

bool ZipBackend::is_directory(const std::string& path) const {
  std::shared_lock lock(mutex_);
  if (!archive_) return false;
  
  int64_t index = locate_entry(path);
  if (index < 0) return false;
  
  const char* name = zip_get_name(archive_, index, 0);
  return name && name[strlen(name) - 1] == '/';
}

std::unique_ptr<DirectoryIterator> ZipBackend::list_directory(
    const std::string& path) {
  std::shared_lock lock(mutex_);
  
  if (!archive_) return nullptr;
  
  std::string zip_path = strip_mount_point(path);
  return std::make_unique<ZipDirectoryIterator>(archive_, zip_path);
}

int64_t ZipBackend::file_size(const std::string& path) const {
  std::shared_lock lock(mutex_);
  if (!archive_) return -1;
  
  int64_t index = locate_entry(path);
  if (index < 0) return -1;
  
  struct zip_stat st;
  zip_stat_init(&st);
  if (zip_stat_index(archive_, index, 0, &st) != 0) {
    return -1;
  }
  
  return st.size;
}

time_t ZipBackend::modification_time(const std::string& path) const {
  std::shared_lock lock(mutex_);
  if (!archive_) return 0;
  
  int64_t index = locate_entry(path);
  if (index < 0) return 0;
  
  struct zip_stat st;
  zip_stat_init(&st);
  if (zip_stat_index(archive_, index, 0, &st) != 0) {
    return 0;
  }
  
  return st.mtime;
}

mode_t ZipBackend::permissions(const std::string& path) const {
  // ZIP doesn't store POSIX permissions reliably
  // Return default read-only permissions
  return is_directory(path) ? 0555 : 0444;
}

std::string ZipBackend::backend_version() const {
  return "libzip-1.10.1";  // Update as needed
}

bool ZipBackend::can_handle(const std::string& path) {
  // Check magic number
  uint8_t buffer[4];
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return false;
  
  file.read(reinterpret_cast<char*>(buffer), 4);
  if (file.gcount() != 4) return false;
  
  // ZIP magic: PK\x03\x04
  return (buffer[0] == 0x50 && buffer[1] == 0x4B &&
          buffer[2] == 0x03 && buffer[3] == 0x04);
}

// Private helpers

int64_t ZipBackend::locate_entry(const std::string& path) const {
  std::string zip_path = strip_mount_point(path);
  
  // Remove leading slash
  if (!zip_path.empty() && zip_path[0] == '/') {
    zip_path = zip_path.substr(1);
  }
  
  return zip_name_locate(archive_, zip_path.c_str(), 0);
}

std::string ZipBackend::strip_mount_point(const std::string& path) const {
  if (path.find(mount_point_) == 0) {
    return path.substr(mount_point_.length());
  }
  return path;
}

}  // namespace fs
}  // namespace tebako
```

#### 2. Update BackendFactory

```cpp
// In backend_factory.cpp
#include <tebako/fs/backends/zip_backend.h>

std::unique_ptr<FileSystem> BackendFactory::create_zip() {
  return std::make_unique<ZipBackend>();
}
```

#### 3. CMake Updates

```cmake
# Add ZIP backend sources
target_sources(libtfs PRIVATE
    src/backends/zip_backend.cpp
)
```

**Success Criteria**:
- [x] ZipBackend compiles
- [x] Can mount ZIP archives
- [x] Can open files
- [x] Can list directories

---

### Day 5-6: ZIP Testing & Integration

#### 1. Create Test Files

```bash
# Create test ZIP archive
cd tests/test_files
echo "Test content" > test.txt
mkdir subdir
echo "Nested file" > subdir/nested.txt
zip test_archive.zip test.txt subdir/nested.txt
```

#### 2. Write Tests

File: `tests/test_zip_backend.cpp`

```cpp
#include <gtest/gtest.h>
#include <tebako/fs/backends/zip_backend.h>
#include <filesystem>

namespace fs = std::filesystem;
using namespace tebako::fs;

class ZipBackendTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_zip_ = fs::path(__FILE__).parent_path() / "test_files" / "test_archive.zip";
  }
  
  fs::path test_zip_;
};

TEST_F(ZipBackendTest, MountUnmount) {
  auto backend = std::make_unique<ZipBackend>();
  
  EXPECT_FALSE(backend->is_mounted());
  EXPECT_TRUE(backend->mount(test_zip_.string(), "/mnt"));
  EXPECT_TRUE(backend->is_mounted());
  
  backend->unmount();
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(ZipBackendTest, FileExists) {
  auto backend = std::make_unique<ZipBackend>();
  backend->mount(test_zip_.string(), "/mnt");
  
  EXPECT_TRUE(backend->exists("/mnt/test.txt"));
  EXPECT_TRUE(backend->exists("/mnt/subdir/nested.txt"));
  EXPECT_FALSE(backend->exists("/mnt/nonexistent.txt"));
}

TEST_F(ZipBackendTest, IsFile) {
  auto backend = std::make_unique<ZipBackend>();
  backend->mount(test_zip_.string(), "/mnt");
  
  EXPECT_TRUE(backend->is_file("/mnt/test.txt"));
  EXPECT_FALSE(backend->is_file("/mnt/subdir"));
}

TEST_F(ZipBackendTest, IsDirectory) {
  auto backend = std::make_unique<ZipBackend>();
  backend->mount(test_zip_.string(), "/mnt");
  
  EXPECT_TRUE(backend->is_directory("/mnt/subdir"));
  EXPECT_FALSE(backend->is_directory("/mnt/test.txt"));
}

TEST_F(ZipBackendTest, ReadFile) {
  auto backend = std::make_unique<ZipBackend>();
  backend->mount(test_zip_.string(), "/mnt");
  
  auto handle = backend->open("/mnt/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);
  
  char buffer[256] = {0};
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer) - 1);
  
  EXPECT_GT(bytes_read, 0);
  EXPECT_STREQ(buffer, "Test content\n");
}

TEST_F(ZipBackendTest, ListDirectory) {
  auto backend = std::make_unique<ZipBackend>();
  backend->mount(test_zip_.string(), "/mnt");
  
  auto it = backend->list_directory("/mnt");
  ASSERT_NE(it, nullptr);
  
  std::vector<std::string> entries;
  while (it->has_next()) {
    auto entry = it->next();
    entries.push_back(entry.name);
  }
  
  EXPECT_EQ(entries.size(), 2);
  EXPECT_NE(std::find(entries.begin(), entries.end(), "test.txt"), entries.end());
  EXPECT_NE(std::find(entries.begin(), entries.end(), "subdir"), entries.end());
}

TEST_F(ZipBackendTest, FileSize) {
  auto backend = std::make_unique<ZipBackend>();
  backend->mount(test_zip_.string(), "/mnt");
  
  int64_t size = backend->file_size("/mnt/test.txt");
  EXPECT_GT(size, 0);
  EXPECT_EQ(size, 13);  // "Test content\n"
}
```

**Success Criteria**:
- [x] All ZIP tests passing
- [x] File operations work correctly
- [x] Directory iteration works
- [x] Integration with mount table

---

## Phase 3: SquashFS Backend (Days 7-12)

**Timeline**: 6 days  
**Goal**: Full SquashFS backend with static squashfs-tools-ng

### Day 7-8: squashfs-tools-ng Static Integration

#### 1. Add squashfs-tools-ng as CMake ExternalProject

```cmake
# squashfs-tools-ng source
set(SQFS_VERSION "1.3.1")
set(SQFS_URL "https://github.com/AgentD/squashfs-tools-ng/archive/refs/tags/v${SQFS_VERSION}.tar.gz")

ExternalProject_Add(
    _squashfs_tools_ng
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/_squashfs
    URL ${SQFS_URL}
    URL_HASH SHA256=...  # actual hash
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DBUILD_SHARED_LIBS=OFF          # Static
        -DWITH_LZO=OFF                   # Minimize deps
        -DWITH_LZ4=ON                    # Common compression
        -DWITH_XZ=ON                     # LZMA compression
        -DWITH_ZSTD=ON                   # Modern compression
        -DWITH_ZLIB=ON                   # Basic compression
        -DWITH_PTHREAD=ON                # Threading support
        -DBUILD_TOOLS=OFF                # No CLI tools
    BUILD_BYPRODUCTS
        <INSTALL_DIR>/lib/libsquashfs.a
)

ExternalProject_Get_Property(_squashfs_tools_ng INSTALL_DIR)
set(SQFS_INCLUDE_DIR ${INSTALL_DIR}/include)
set(SQFS_LIBRARY ${INSTALL_DIR}/lib/libsquashfs.a)

# Create imported target
add_library(squashfs STATIC IMPORTED)
set_target_properties(squashfs PROPERTIES
    IMPORTED_LOCATION ${SQFS_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${SQFS_INCLUDE_DIR}
)
add_dependencies(squashfs _squashfs_tools_ng)

# Link
target_link_libraries(libtfs PRIVATE squashfs)
```

#### 2. Verify Build

```bash
cmake --build build --target _squashfs_tools_ng
# Should produce build/_squashfs/lib/libsquashfs.a
```

**Success Criteria**:
- [x] squashfs-tools-ng builds statically
- [x] Headers available
- [x] No dynamic dependencies

---

### Day 9-10: SquashFSBackend Core

#### 1. Create Backend Files

File: `include/tebako/fs/backends/squashfs_backend.h`

```cpp
#pragma once

#include <tebako/fs/filesystem.h>
#include <memory>
#include <string>
#include <shared_mutex>

// Forward declare squashfs types
struct sqfs_super_t;
struct sqfs_inode_t;
struct sqfs_file_t;
struct sqfs_dir_reader_t;

namespace tebako {
namespace fs {

/**
 * @brief SquashFS filesystem backend using squashfs-tools-ng
 */
class SquashFSBackend : public FileSystem {
 public:
  SquashFSBackend();
  ~SquashFSBackend() override;

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
  std::string backend_name() const override { return "SquashFS"; }
  std::string backend_version() const override;
  std::string archive_path() const override { return archive_path_; }
  std::string mount_point() const override { return mount_point_; }

  // Format detection
  static bool can_handle(const std::string& path);

 private:
  struct sqfs_file_t* file_;           // File handle
  struct sqfs_super_t* super_;         // Superblock
  struct sqfs_dir_reader_t* dr_;       // Directory reader
  
  std::string archive_path_;
  std::string mount_point_;
  mutable std::shared_mutex mutex_;

  // Helper: lookup inode by path
  struct sqfs_inode_t* lookup_inode(const std::string& path) const;
  
  // Helper: strip mount point
  std::string strip_mount_point(const std::string& path) const;
};

}  // namespace fs
}  // namespace tebako
```

File: `src/backends/squashfs_backend.cpp`

```cpp
#include <tebako/fs/backends/squashfs_backend.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <sqsh.h>  // squashfs-tools-ng

namespace tebako {
namespace fs {

// Implementation follows similar pattern to ZipBackend
// Uses squashfs-tools-ng API for:
// - sqsh_super_init() - Read superblock
// - sqsh_inode_from_path() - Lookup by path
// - sqsh_file_reader_new() - Read file data
// - sqsh_dir_reader_new() - Iterate directory

// ... detailed implementation ...

}  // namespace fs
}  // namespace tebako
```

### Day 11-12: SquashFS Testing

Similar test structure to ZIP backend.

**Success Criteria**:
- [x] SquashFS mounts successfully
- [x] File operations work
- [x] Directory iteration works
- [x] All tests passing

---

## Phase 4: Integration & Documentation (Days 13-14)

### Day 13: Final Integration

#### 1. Update Mount Table
- [ ] Extend mount_table to support FileSystem pointers
- [ ] Add convenience functions for mounting by path

#### 2. Public API
- [ ] Create`tebako_fs_mount_auto()` - Auto-detect and mount
- [ ] Update existing mount functions

### Day 14: Documentation

#### 1. Update README.adoc
- [ ] Document ZIP support
- [ ] Document SquashFS support
- [ ] Update build instructions
- [ ] Add usage examples

#### 2. Archive Old Docs
- [ ] Move completed task docs to `docs/archive/`
- [ ] Keep active design docs

---

## Build Verification Checklist

### Static Compilation Verification

```bash
# Build everything
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Verify static linkage
ldd build/libtfs.a  # Should say "not a dynamic executable"
nm build/libtfs.a | grep -i "zip\|squash"  # Should show symbols

# Run tests
cd build
ctest --verbose
```

### Success Criteria
- [x] All dependencies built statically
- [x] No dynamic library dependencies
- [x] All tests passing
- [x] Documentation updated

---

## Timeline Summary

| Days | Phase | Deliverable |
|------|-------|-------------|
| 1 | Factory | BackendFactory with detection |
| 2-3 | libzip | Static libzip integration |
| 4 | ZIP Core | ZipBackend implementation |
| 5-6 | ZIP Tests | Complete ZIP testing |
| 7-8 | squashfs-tools-ng | Static integration |
| 9-10 | SquashFS Core | SquashFSBackend implementation |
| 11-12 | SquashFS Tests | Complete testing |
| 13 | Integration | Mount table, public API |
| 14 | Documentation | README, archive old docs |

**Total**: 14 days (2 weeks)

---

## Risk Mitigation

### Risk: libzip Build Issues
**Mitigation**: Use specific version 1.10.1, disable optional features

### Risk: squashfs-tools-ng Complexity
**Mitigation**: Start with read-only support, use example code from project

### Risk: Static Linking Conflicts
**Mitigation**: Build each dependency in isolated ExternalProject

---

**Document Version**: 1.0  
**Last Updated**: 2025-12-22  
**Next Review**: After Day 7 (mid-point)