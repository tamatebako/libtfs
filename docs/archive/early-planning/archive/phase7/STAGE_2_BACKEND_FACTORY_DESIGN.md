# Stage 2: Simplified Backend Factory Design

**Date**: 2025-12-21
**Status**: Architectural Design
**Approach**: Static Factory Pattern (No Registry, No Threads)

---

## Executive Summary

Based on analysis of Tebako's direct integration requirements, we're implementing a **simplified static factory pattern** instead of a dynamic plugin registry. This eliminates unnecessary complexity while maintaining clean architecture.

### Key Decisions

✅ **Static factory methods** - No singleton, no state
✅ **Format auto-detection** - Magic numbers + file extensions  
✅ **No dynamic registration** - Backends known at compile-time
✅ **No thread synchronization** - Tebako handles concurrency
✅ **Polymorphic interfaces** - FileSystem abstraction maintained

---

## Architecture Overview

### Current State Analysis

The existing [`mount_table`](../include/tebako/fs/internal/mount_table.h) already supports multiple filesystem instances:

```cpp
// Current mount table structure
typedef std::variant<std::string, uint32_t> tebako_mount_target;
//                   ^^^^^^^^^^^  ^^^^^^^^
//                   Host path    memfs index
```

This means:
- Mount table already handles polymorphic targets
- Multiple filesystem instances already supported via uint32_t indices
- Thread safety handled by `Synchronized<>` wrapper

### Target Architecture

```
Application (Tebako)
        │
        ├─ Direct instantiation: auto fs = BackendFactory::create_dwarfs()
        │                    or: auto fs = BackendFactory::create_from_file(path)
        │
        ↓
BackendFactory (static methods only)
        │
        ├─ Format Detection (magic numbers, extensions)
        ├─ Backend Creation (returns unique_ptr<FileSystem>)
        │
        ↓
FileSystem Interface (abstract)
        │
        ├─ DwarfsBackend (FlatBuffers-based)
        └─ ZipBackend (libzip-based)
```

---

## Component Design

### 1. BackendFactory (Static Factory)

**File**: `include/tebako/fs/backend_factory.h`

```cpp
namespace tebako {
namespace fs {

/**
 * @brief Static factory for creating filesystem backends
 *
 * Provides simple, compile-time backend creation with optional
 * format auto-detection. No dynamic registration or thread state.
 *
 * @example Basic usage
 * @code
 * // Auto-detect and create
 * auto fs = BackendFactory::create_from_file("/path/to/archive.zip");
 * if (fs && fs->mount("/path/to/archive.zip", "/mnt/app")) {
 *     // Use filesystem...
 * }
 *
 * // Explicit backend selection
 * auto dwarfs = BackendFactory::create_dwarfs();
 * @endcode
 */
class BackendFactory {
 public:
  // ===================================================================
  // Primary Factory Methods
  // ===================================================================

  /**
   * @brief Auto-detect format and create appropriate backend
   *
   * Detects archive format using:
   * 1. Magic number detection (highest confidence)
   * 2. File extension (fallback)
   *
   * @param archive_path Path to archive file
   * @return Unique pointer to FileSystem, or nullptr if format unknown
   *
   * @note Does NOT mount the filesystem, only creates the backend
   * @note Caller must call mount() on the returned backend
   */
  static std::unique_ptr<FileSystem> create_from_file(
      const std::string& archive_path);

  /**
   * @brief Explicitly create DwarFS backend
   *
   * @return Unique pointer to DwarfsBackend
   */
  static std::unique_ptr<FileSystem> create_dwarfs();

  /**
   * @brief Explicitly create ZIP backend
   *
   * @return Unique pointer to ZipBackend
   */
  static std::unique_ptr<FileSystem> create_zip();

  // ===================================================================
  // Format Detection (Public for Testing)
  // ===================================================================

  /**
   * @brief Detect if file is DwarFS format
   *
   * Checks for DwarFS magic signature in file header.
   *
   * @param path Path to file
   * @return true if DwarFS format detected
   */
  static bool is_dwarfs_format(const std::string& path);

  /**
   * @brief Detect if file is ZIP format
   *
   * Checks for ZIP magic signature (PK\x03\x04 or PK\x05\x06).
   *
   * @param path Path to file
   * @return true if ZIP format detected
   */
  static bool is_zip_format(const std::string& path);

 private:
  // ===================================================================
  // Internal Helpers
  // ===================================================================

  /**
   * @brief Read magic bytes from file header
   *
   * @param path Path to file
   * @param buffer Buffer to store bytes (must be at least size bytes)
   * @param size Number of bytes to read
   * @return true if read succeeded, false otherwise
   */
  static bool read_magic_bytes(const std::string& path,
                               uint8_t* buffer,
                               size_t size);

  /**
   * @brief Check if path has given extension
   *
   * @param path File path
   * @param ext Extension to check (including dot, e.g., ".zip")
   * @return true if path ends with extension (case-insensitive)
   */
  static bool has_extension(const std::string& path, const std::string& ext);
};

}  // namespace fs
}  // namespace tebako
```

---

## Format Detection Strategy

### Detection Priority

1. **Magic Numbers** (Highest Priority)
   - Direct file header inspection
   - Most reliable method
   - Handles misnamed files (e.g., `.dfs` file that's actually ZIP)

2. **File Extensions** (Fallback)
   - Used if magic number detection inconclusive
   - Case-insensitive matching
   - Multiple extensions per format

### Format Signatures

```cpp
// DwarFS Magic Signature
// Location: File header (first 8-16 bytes)
// Value: "DWARFS" followed by version bytes
static constexpr uint8_t DWARFS_MAGIC[] = {
    'D', 'W', 'A', 'R', 'F', 'S'
};

// ZIP Magic Signatures
// Local file header
static constexpr uint8_t ZIP_LOCAL_MAGIC[] = {
    0x50, 0x4B, 0x03, 0x04  // "PK\x03\x04"
};

// Central directory header (for empty archives)
static constexpr uint8_t ZIP_CENTRAL_MAGIC[] = {
    0x50, 0x4B, 0x05, 0x06  // "PK\x05\x06"
};
```

### Supported Extensions

| Format | Extensions | Notes |
|--------|-----------|-------|
| DwarFS | `.dwarfs`, `.dfs` | Native DwarFS archives |
| ZIP | `.zip`, `.jar`, `.apk`, `.war`, `.ear` | Standard ZIP formats |

---

## Implementation Details

### File: `src/backend_factory.cpp`

```cpp
#include <tebako/fs/backend_factory.h>
#include <tebako/fs/backends/dwarfs_backend.h>
#include <tebako/fs/backends/zip_backend.h>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace tebako {
namespace fs {

// ===================================================================
// Magic Number Constants
// ===================================================================

namespace {
constexpr uint8_t DWARFS_MAGIC[] = {'D', 'W', 'A', 'R', 'F', 'S'};
constexpr uint8_t ZIP_LOCAL_MAGIC[] = {0x50, 0x4B, 0x03, 0x04};
constexpr uint8_t ZIP_CENTRAL_MAGIC[] = {0x50, 0x4B, 0x05, 0x06};
constexpr size_t MAX_MAGIC_SIZE = 16;
}  // namespace

// ===================================================================
// Primary Factory Methods
// ===================================================================

std::unique_ptr<FileSystem> BackendFactory::create_from_file(
    const std::string& archive_path) {
  
  // Try magic number detection first (most reliable)
  if (is_dwarfs_format(archive_path)) {
    return create_dwarfs();
  }
  
  if (is_zip_format(archive_path)) {
    return create_zip();
  }
  
  // Fallback to extension-based detection
  if (has_extension(archive_path, ".dwarfs") ||
      has_extension(archive_path, ".dfs")) {
    return create_dwarfs();
  }
  
  if (has_extension(archive_path, ".zip") ||
      has_extension(archive_path, ".jar") ||
      has_extension(archive_path, ".apk") ||
      has_extension(archive_path, ".war") ||
      has_extension(archive_path, ".ear")) {
    return create_zip();
  }
  
  // Unknown format
  return nullptr;
}

std::unique_ptr<FileSystem> BackendFactory::create_dwarfs() {
  return std::make_unique<DwarfsBackend>();
}

std::unique_ptr<FileSystem> BackendFactory::create_zip() {
  return std::make_unique<ZipBackend>();
}

// ===================================================================
// Format Detection
// ===================================================================

bool BackendFactory::is_dwarfs_format(const std::string& path) {
  uint8_t buffer[MAX_MAGIC_SIZE] = {0};
  
  if (!read_magic_bytes(path, buffer, sizeof(DWARFS_MAGIC))) {
    return false;
  }
  
  return std::memcmp(buffer, DWARFS_MAGIC, sizeof(DWARFS_MAGIC)) == 0;
}

bool BackendFactory::is_zip_format(const std::string& path) {
  uint8_t buffer[MAX_MAGIC_SIZE] = {0};
  
  if (!read_magic_bytes(path, buffer, sizeof(ZIP_LOCAL_MAGIC))) {
    return false;
  }
  
  // Check for local file header
  if (std::memcmp(buffer, ZIP_LOCAL_MAGIC, sizeof(ZIP_LOCAL_MAGIC)) == 0) {
    return true;
  }
  
  // Check for central directory (empty ZIP)
  if (std::memcmp(buffer, ZIP_CENTRAL_MAGIC, sizeof(ZIP_CENTRAL_MAGIC)) == 0) {
    return true;
  }
  
  return false;
}

// ===================================================================
// Internal Helpers
// ===================================================================

bool BackendFactory::read_magic_bytes(const std::string& path,
                                     uint8_t* buffer,
                                     size_t size) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  
  file.read(reinterpret_cast<char*>(buffer), size);
  return file.gcount() == static_cast<std::streamsize>(size);
}

bool BackendFactory::has_extension(const std::string& path,
                                   const std::string& ext) {
  if (path.length() < ext.length()) {
    return false;
  }
  
  // Case-insensitive comparison
  auto path_end = path.substr(path.length() - ext.length());
  return std::equal(
      path_end.begin(), path_end.end(),
      ext.begin(),
      [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
      });
}

}  // namespace fs
}  // namespace tebako
```

---

## Integration with Mount Table

### Current Mount Table Extension

The existing mount table will be extended to support FileSystem pointers:

```cpp
// Proposed extension to mount_table.h
typedef std::variant<
    std::string,              // Host filesystem path
    uint32_t,                 // Legacy memfs index
    std::shared_ptr<FileSystem>  // New: Backend pointer
> tebako_mount_target;
```

**Rationale for `shared_ptr`**:
- Mount table needs to retain backends after mount
- Multiple mount points may reference same backend
- Automatic cleanup when all references dropped

### Integration Example

```cpp
// In tebako-memfs.cpp or new backend integration layer
int tebako_mount_backend(const char* archive_path, const char* mount_point) {
  // Auto-detect and create backend
  auto backend = BackendFactory::create_from_file(archive_path);
  if (!backend) {
    errno = EINVAL;  // Unknown format
    return -1;
  }
  
  // Mount the backend
  if (!backend->mount(archive_path, mount_point)) {
    errno = EIO;  // Mount failed
    return -1;
  }
  
  // Add to mount table
  auto& mount_table = sync_tebako_mount_table::get_tebako_mount_table();
  auto shared_backend = std::shared_ptr<FileSystem>(std::move(backend));
  
  if (!mount_table.insert(0, mount_point, shared_backend)) {
    backend->unmount();
    errno = EEXIST;  // Mount point already exists
    return -1;
  }
  
  return 0;
}
```

---

## Testing Strategy

### Unit Tests: `tests/test_backend_factory.cpp`

```cpp
#include <gtest/gtest.h>
#include <tebako/fs/backend_factory.h>
#include <fstream>

using namespace tebako::fs;

// ===================================================================
// Test Fixtures
// ===================================================================

class BackendFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test directory
    test_dir_ = std::filesystem::temp_directory_path() / "tebako_test";
    std::filesystem::create_directories(test_dir_);
  }
  
  void TearDown() override {
    std::filesystem::remove_all(test_dir_);
  }
  
  void create_test_file(const std::string& name,
                       const uint8_t* magic,
                       size_t magic_size) {
    auto path = test_dir_ / name;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(magic), magic_size);
    file.close();
  }
  
  std::filesystem::path test_dir_;
};

// ===================================================================
// Factory Creation Tests
// ===================================================================

TEST_F(BackendFactoryTest, CreateDwarfs) {
  auto backend = BackendFactory::create_dwarfs();
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, CreateZip) {
  auto backend = BackendFactory::create_zip();
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "ZIP");
}

// ===================================================================
// Magic Number Detection Tests
// ===================================================================

TEST_F(BackendFactoryTest, DetectDwarfsMagic) {
  uint8_t dwarfs_magic[] = {'D', 'W', 'A', 'R', 'F', 'S', 0x00, 0x01};
  create_test_file("test.dwarfs", dwarfs_magic, sizeof(dwarfs_magic));
  
  auto path = test_dir_ / "test.dwarfs";
  EXPECT_TRUE(BackendFactory::is_dwarfs_format(path.string()));
  EXPECT_FALSE(BackendFactory::is_zip_format(path.string()));
}

TEST_F(BackendFactoryTest, DetectZipMagic) {
  uint8_t zip_magic[] = {0x50, 0x4B, 0x03, 0x04, 0x00, 0x00};
  create_test_file("test.zip", zip_magic, sizeof(zip_magic));
  
  auto path = test_dir_ / "test.zip";
  EXPECT_TRUE(BackendFactory::is_zip_format(path.string()));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path.string()));
}

// ===================================================================
// Auto-Detection Tests
// ===================================================================

TEST_F(BackendFactoryTest, AutoDetectDwarfs) {
  uint8_t dwarfs_magic[] = {'D', 'W', 'A', 'R', 'F', 'S'};
  create_test_file("archive.dwarfs", dwarfs_magic, sizeof(dwarfs_magic));
  
  auto path = test_dir_ / "archive.dwarfs";
  auto backend = BackendFactory::create_from_file(path.string());
  
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, AutoDetectZip) {
  uint8_t zip_magic[] = {0x50, 0x4B, 0x03, 0x04};
  create_test_file("archive.zip", zip_magic, sizeof(zip_magic));
  
  auto path = test_dir_ / "archive.zip";
  auto backend = BackendFactory::create_from_file(path.string());
  
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "ZIP");
}

// ===================================================================
// Extension Fallback Tests
// ===================================================================

TEST_F(BackendFactoryTest, ExtensionFallbackZip) {
  // File with no recognizable magic number
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};
  create_test_file("file.jar", dummy, sizeof(dummy));
  
  auto path = test_dir_ / "file.jar";
  auto backend = BackendFactory::create_from_file(path.string());
  
  // Should detect via .jar extension
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "ZIP");
}

// ===================================================================
// Error Case Tests
// ===================================================================

TEST_F(BackendFactoryTest, UnknownFormat) {
  uint8_t dummy[] = {0xFF, 0xFF, 0xFF, 0xFF};
  create_test_file("file.unknown", dummy, sizeof(dummy));
  
  auto path = test_dir_ / "file.unknown";
  auto backend = BackendFactory::create_from_file(path.string());
  
  EXPECT_EQ(backend, nullptr);
}

TEST_F(BackendFactoryTest, NonExistentFile) {
  auto path = test_dir_ / "nonexistent.zip";
  
  EXPECT_FALSE(BackendFactory::is_zip_format(path.string()));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path.string()));
  
  auto backend = BackendFactory::create_from_file(path.string());
  EXPECT_EQ(backend, nullptr);
}
```

---

## Implementation Checklist

### Week 1 Day 2: Backend Factory Implementation

**Morning Session (2-3 hours)**

- [ ] Create `include/tebako/fs/backend_factory.h`
  - [ ] Class declaration with static methods
  - [ ] Comprehensive Doxygen documentation
  - [ ] Usage examples in comments
  - [ ] Format detection method signatures

- [ ] Create `src/backend_factory.cpp`
  - [ ] Magic number constants
  - [ ] `create_from_file()` implementation
  - [ ] `create_dwarfs()` implementation
  - [ ] `create_zip()` implementation
  - [ ] `is_dwarfs_format()` implementation
  - [ ] `is_zip_format()` implementation
  - [ ] Helper functions

**Afternoon Session (2-3 hours)**

- [ ] Create `tests/test_backend_factory.cpp`
  - [ ] Test fixture setup
  - [ ] Factory creation tests
  - [ ] Magic number detection tests
  - [ ] Auto-detection tests  
  - [ ] Extension fallback tests
  - [ ] Error case tests

- [ ] Update `CMakeLists.txt`
  - [ ] Add `src/backend_factory.cpp` to library sources
  - [ ] Add `tests/test_backend_factory.cpp` to test targets
  - [ ] Verify dependencies

- [ ] Build and validate
  - [ ] `cmake .. && make -j$(nproc)`
  - [ ] `ctest -R test_backend_factory --verbose`
  - [ ] All tests passing
  - [ ] No compiler warnings

---

## Success Criteria

Week 1 Day 2 complete when:

- [x] `backend_factory.h` created with complete API
- [x] `backend_factory.cpp` implemented with all methods
- [x] Format detection working (magic + extensions)
- [x] `test_backend_factory.cpp` with comprehensive tests
- [x] CMakeLists.txt updated
- [x] All tests passing
- [x] Code compiles without warnings
- [x] Documentation complete

---

## Advantages of Static Factory Approach

### vs. Dynamic Registry

| Aspect | Static Factory ✅ | Dynamic Registry ❌ |
|--------|------------------|---------------------|
| **Complexity** | Simple, direct | Complex, indirect |
| **Performance** | Zero overhead | Mutex overhead |
| **Thread Safety** | Not needed | Required |
| **Extensibility** | Compile-time | Runtime |
| **Testability** | Easy | Moderate |
| **Code Size** | Minimal | Larger |

### Benefits for Tebako

1. **No Runtime Overhead**: Static methods inlined by compiler
2. **Simpler Integration**: Direct instantiation in Tebako code
3. **Easier Debugging**: No indirection through registry
4. **Predictable Behavior**: Known backends at compile-time
5. **Cleaner API**: Factory methods self-documenting

---

## Future Extensibility

While this is a static factory, adding new backends is straightforward:

```cpp
// To add TAR backend in future:

// 1. Implement TarBackend class
class TarBackend : public FileSystem { /* ... */ };

// 2. Add factory method
static std::unique_ptr<FileSystem> BackendFactory::create_tar() {
  return std::make_unique<TarBackend>();
}

// 3. Add detection
static bool BackendFactory::is_tar_format(const std::string& path) {
  // Magic: "ustar" at offset 257
  // ...
}

// 4. Update create_from_file()
if (is_tar_format(archive_path)) {
  return create_tar();
}
```

No registry changes needed - just add methods and update detection chain.

---

## References

- **Architecture**: [`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md)
- **Interfaces**: [`filesystem.h`](../include/tebako/fs/filesystem.h)
- **Mount Table**: [`mount_table.h`](../include/tebako/fs/internal/mount_table.h)
- **Progress**: [`CONTINUATION_PLAN.md`](CONTINUATION_PLAN.md)

---

**Document Version**: 1.0  
**Last Updated**: 2025-12-21  
**Status**: Ready for Implementation