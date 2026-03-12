# Stage 2 Week 1 Day 2: Backend Factory Implementation

**Date**: 2025-12-22
**Current Status**: Week 1 Day 1 Complete ✅
**Next Task**: Implement Backend Factory (Simplified Static Approach)

---

## Context: What's Been Done

### Week 1 Day 1 Complete (2025-12-21) ✅

Successfully created the foundational VFS interface layer:

1. **[`include/tebako/fs/filesystem.h`](../include/tebako/fs/filesystem.h)** (216 lines)
   - Abstract base class for all filesystem backends
   - 13 pure virtual methods covering lifecycle, file ops, directory ops, metadata
   - Fully documented with Doxygen comments and examples
   - Thread-safety requirements specified

2. **[`include/tebako/fs/file_handle.h`](../include/tebako/fs/file_handle.h)** (139 lines)
   - Abstract file handle for POSIX-like read operations
   - Methods: read(), seek(), tell(), eof(), close(), path(), size()

3. **[`include/tebako/fs/directory_iterator.h`](../include/tebako/fs/directory_iterator.h)** (141 lines)
   - DirectoryEntry struct: name, is_directory, size, mtime
   - Iterator interface: has_next(), next(), reset()

**Architecture Verified:**
- ✅ Pure OOP design with abstract base classes
- ✅ POSIX-compatible API
- ✅ CMake builds successfully
- ✅ Ready for backend implementations

---

## Architectural Decision: Simplified Static Factory

**IMPORTANT**: Based on Tebako integration requirements, we're using a **simplified static factory pattern** instead of a dynamic plugin registry.

### Why Not Dynamic Registry?

❌ **Not Needed**:
- No dynamic plugin loading
- No runtime backend registration
- No multi-threaded concurrent registration
- Backends known at compile-time

✅ **What We Need**:
- Simple backend instantiation
- Format auto-detection
- Zero runtime overhead
- Direct integration with Tebako

### Static Factory Benefits

| Feature | Static Factory ✅ | Dynamic Registry ❌ |
|---------|------------------|---------------------|
| Complexity | Minimal | High |
| Thread Safety | Not needed | Required (mutexes) |
| Performance | Zero overhead | Mutex + indirection |
| Code Size | Small | Large |
| Testability | Easy | Moderate |

---

## What We're Building Today

### Backend Factory Architecture

```
Application (Tebako)
        │
        ├─ BackendFactory::create_from_file(path)  → auto-detects format
        ├─ BackendFactory::create_dwarfs()         → explicit DwarFS
        └─ BackendFactory::create_zip()            → explicit ZIP
                │
                ↓
        Returns: unique_ptr<FileSystem>
                │
                ├─ DwarfsBackend
                └─ ZipBackend
```

### Key Principles

1. **No State**: Factory has no member variables, only static methods
2. **No Singleton**: No `instance()` method, no global state
3. **No Threads**: No mutexes, no thread synchronization
4. **Compile-Time**: All backends known at compile time
5. **Simple**: Just functions that return backend instances

---

## Files to Create

### 1. Header: `include/tebako/fs/backend_factory.h`

**Complete API specification**:

```cpp
#pragma once

#include <tebako/fs/filesystem.h>
#include <memory>
#include <string>
#include <cstdint>

namespace tebako {
namespace fs {

/**
 * @brief Static factory for creating filesystem backends
 *
 * Provides compile-time backend creation with optional format auto-detection.
 * All methods are static - no instance creation, no state, no threads.
 *
 * @example Auto-detect format
 * @code
 * auto fs = BackendFactory::create_from_file("/path/to/archive.zip");
 * if (fs && fs->mount("/path/to/archive.zip", "/mnt/app")) {
 *     // Use filesystem...
 *     fs->unmount();
 * }
 * @endcode
 *
 * @example Explicit backend
 * @code
 * auto dwarfs = BackendFactory::create_dwarfs();
 * dwarfs->mount("data.dwarfs", "/mnt/data");
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
   * Detection strategy:
   * 1. Check magic number in file header (most reliable)
   * 2. Check file extension (fallback)
   *
   * @param archive_path Path to archive file
   * @return unique_ptr to FileSystem backend, or nullptr if format unknown
   *
   * @note Does NOT mount the filesystem, only creates the backend instance
   * @note Caller must call mount() on returned backend
   * @note Returns nullptr for unknown/unsupported formats
   */
  static std::unique_ptr<FileSystem> create_from_file(
      const std::string& archive_path);

  /**
   * @brief Create DwarFS backend explicitly
   *
   * @return unique_ptr to DwarfsBackend
   */
  static std::unique_ptr<FileSystem> create_dwarfs();

  /**
   * @brief Create ZIP backend explicitly
   *
   * @return unique_ptr to ZipBackend
   * @note ZIP backend will be implemented in Week 2
   */
  static std::unique_ptr<FileSystem> create_zip();

  // ===================================================================
  // Format Detection (Public for Testing)
  // ===================================================================

  /**
   * @brief Detect if file is DwarFS format
   *
   * Checks for DwarFS magic signature: "DWARFS" in file header
   *
   * @param path Path to file
   * @return true if DwarFS format detected, false otherwise
   */
  static bool is_dwarfs_format(const std::string& path);

  /**
   * @brief Detect if file is ZIP format
   *
   * Checks for ZIP magic signatures:
   * - Local file header: PK\x03\x04
   * - Central directory: PK\x05\x06
   *
   * @param path Path to file
   * @return true if ZIP format detected, false otherwise
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
   * @return true if read succeeded, false on error
   */
  static bool read_magic_bytes(const std::string& path,
                               uint8_t* buffer,
                               size_t size);

  /**
   * @brief Check if path has given extension (case-insensitive)
   *
   * @param path File path
   * @param ext Extension to check (including dot, e.g., ".zip")
   * @return true if path ends with extension
   */
  static bool has_extension(const std::string& path,
                           const std::string& ext);
};

}  // namespace fs
}  // namespace tebako
```

---

### 2. Implementation: `src/backend_factory.cpp`

**Implementation structure**:

```cpp
#include <tebako/fs/backend_factory.h>
#include <tebako/fs/backends/dwarfs_backend.h>
// #include <tebako/fs/backends/zip_backend.h>  // Week 2

#include <algorithm>
#include <cstring>
#include <fstream>

namespace tebako {
namespace fs {

// ===================================================================
// Magic Number Constants
// ===================================================================

namespace {
// DwarFS magic signature
constexpr uint8_t DWARFS_MAGIC[] = {'D', 'W', 'A', 'R', 'F', 'S'};

// ZIP magic signatures
constexpr uint8_t ZIP_LOCAL_MAGIC[] = {0x50, 0x4B, 0x03, 0x04};  // "PK\x03\x04"
constexpr uint8_t ZIP_CENTRAL_MAGIC[] = {0x50, 0x4B, 0x05, 0x06};  // "PK\x05\x06"

constexpr size_t MAX_MAGIC_SIZE = 16;
}  // anonymous namespace

// ===================================================================
// Primary Factory Methods
// ===================================================================

std::unique_ptr<FileSystem> BackendFactory::create_from_file(
    const std::string& archive_path) {
  
  // Priority 1: Try magic number detection (most reliable)
  if (is_dwarfs_format(archive_path)) {
    return create_dwarfs();
  }
  
  if (is_zip_format(archive_path)) {
    return create_zip();
  }
  
  // Priority 2: Fallback to extension-based detection
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
  // Will be implemented in Week 2
  // For now, return nullptr to make tests compilable
  return nullptr;
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
  
  // Extract last N characters where N = ext.length()
  auto path_end = path.substr(path.length() - ext.length());
  
  // Case-insensitive comparison
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

### 3. Unit Tests: `tests/test_backend_factory.cpp`

**Test coverage required**:

```cpp
#include <gtest/gtest.h>
#include <tebako/fs/backend_factory.h>

#include <filesystem>
#include <fstream>

using namespace tebako::fs;
namespace fs = std::filesystem;

// ===================================================================
// Test Fixture
// ===================================================================

class BackendFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = fs::temp_directory_path() / "tebako_factory_test";
    fs::create_directories(test_dir_);
  }
  
  void TearDown() override {
    fs::remove_all(test_dir_);
  }
  
  // Helper: Create test file with magic bytes
  void create_test_file(const std::string& name,
                       const uint8_t* magic,
                       size_t magic_size) {
    auto path = test_dir_ / name;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(magic), magic_size);
    file.close();
  }
  
  fs::path test_dir_;
};

// ===================================================================
// Factory Creation Tests
// ===================================================================

TEST_F(BackendFactoryTest, CreateDwarfsBackend) {
  auto backend = BackendFactory::create_dwarfs();
  
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(BackendFactoryTest, CreateZipBackend) {
  auto backend = BackendFactory::create_zip();
  
  // Week 1: ZIP not implemented yet, should return nullptr
  // Week 2: This test will change
  EXPECT_EQ(backend, nullptr);
}

// ===================================================================
// Magic Number Detection Tests
// ===================================================================

TEST_F(BackendFactoryTest, DetectDwarfsMagicNumber) {
  uint8_t dwarfs_magic[] = {'D', 'W', 'A', 'R', 'F', 'S', 0x00, 0x01};
  create_test_file("test.dat", dwarfs_magic, sizeof(dwarfs_magic));
  
  auto path = test_dir_ / "test.dat";
  
  EXPECT_TRUE(BackendFactory::is_dwarfs_format(path.string()));
  EXPECT_FALSE(BackendFactory::is_zip_format(path.string()));
}

TEST_F(BackendFactoryTest, DetectZipLocalHeader) {
  uint8_t zip_magic[] = {0x50, 0x4B, 0x03, 0x04, 0x00, 0x00};
  create_test_file("test.dat", zip_magic, sizeof(zip_magic));
  
  auto path = test_dir_ / "test.dat";
  
  EXPECT_TRUE(BackendFactory::is_zip_format(path.string()));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path.string()));
}

TEST_F(BackendFactoryTest, DetectZipCentralDirectory) {
  uint8_t zip_central[] = {0x50, 0x4B, 0x05, 0x06, 0x00, 0x00};
  create_test_file("empty.zip", zip_central, sizeof(zip_central));
  
  auto path = test_dir_ / "empty.zip";
  
  EXPECT_TRUE(BackendFactory::is_zip_format(path.string()));
}

// ===================================================================
// Auto-Detection Tests
// ===================================================================

TEST_F(BackendFactoryTest, AutoDetectDwarfsByMagic) {
  uint8_t dwarfs_magic[] = {'D', 'W', 'A', 'R', 'F', 'S'};
  create_test_file("archive.bin", dwarfs_magic, sizeof(dwarfs_magic));
  
  auto path = test_dir_ / "archive.bin";
  auto backend = BackendFactory::create_from_file(path.string());
  
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, AutoDetectZipByMagic) {
  uint8_t zip_magic[] = {0x50, 0x4B, 0x03, 0x04};
  create_test_file("archive.bin", zip_magic, sizeof(zip_magic));
  
  auto path = test_dir_ / "archive.bin";
  auto backend = BackendFactory::create_from_file(path.string());
  
  // Week 1: create_zip() returns nullptr
  // This test documents expected behavior
  EXPECT_EQ(backend, nullptr);
}

// ===================================================================
// Extension Fallback Tests
// ===================================================================

TEST_F(BackendFactoryTest, FallbackToDwarfsExtension) {
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};
  create_test_file("file.dwarfs", dummy, sizeof(dummy));
  
  auto path = test_dir_ / "file.dwarfs";
  auto backend = BackendFactory::create_from_file(path.string());
  
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, FallbackToDfsExtension) {
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};
  create_test_file("file.dfs", dummy, sizeof(dummy));
  
  auto path = test_dir_ / "file.dfs";
  auto backend = BackendFactory::create_from_file(path.string());
  
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, FallbackToZipExtension) {
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};
  create_test_file("file.zip", dummy, sizeof(dummy));
  
  auto path = test_dir_ / "file.zip";
  auto backend = BackendFactory::create_from_file(path.string());
  
  // Week 1: ZIP not implemented
  EXPECT_EQ(backend, nullptr);
}

TEST_F(BackendFactoryTest, FallbackToJarExtension) {
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};
  create_test_file("app.jar", dummy, sizeof(dummy));
  
  auto path = test_dir_ / "app.jar";
  auto backend = BackendFactory::create_from_file(path.string());
  
  // JAR is ZIP format, Week 1: not implemented
  EXPECT_EQ(backend, nullptr);
}

// ===================================================================
// Error Case Tests
// ===================================================================

TEST_F(BackendFactoryTest, UnknownFormatReturnsNull) {
  uint8_t unknown[] = {0xFF, 0xFF, 0xFF, 0xFF};
  create_test_file("unknown.dat", unknown, sizeof(unknown));
  
  auto path = test_dir_ / "unknown.dat";
  auto backend = BackendFactory::create_from_file(path.string());
  
  EXPECT_EQ(backend, nullptr);
}

TEST_F(BackendFactoryTest, NonExistentFileReturnsFalse) {
  auto path = test_dir_ / "nonexistent.zip";
  
  EXPECT_FALSE(BackendFactory::is_zip_format(path.string()));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path.string()));
}

TEST_F(BackendFactoryTest, NonExistentFileReturnsNull) {
  auto path = test_dir_ / "nonexistent.dwarfs";
  auto backend = BackendFactory::create_from_file(path.string());
  
  EXPECT_EQ(backend, nullptr);
}

TEST_F(BackendFactoryTest, EmptyFileReturnsFalse) {
  create_test_file("empty.dat", nullptr, 0);
  
  auto path = test_dir_ / "empty.dat";
  
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path.string()));
  EXPECT_FALSE(BackendFactory::is_zip_format(path.string()));
}

// ===================================================================
// Case-Insensitive Extension Tests
// ===================================================================

TEST_F(BackendFactoryTest, CaseInsensitiveExtensionDWARFS) {
  uint8_t dummy[] = {0x00, 0x00};
  create_test_file("file.DWARFS", dummy, sizeof(dummy));
  
  auto path = test_dir_ / "file.DWARFS";
  auto backend = BackendFactory::create_from_file(path.string());
  
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, CaseInsensitiveExtensionZIP) {
  uint8_t dummy[] = {0x00, 0x00};
  create_test_file("file.ZIP", dummy, sizeof(dummy));
  
  auto path = test_dir_ / "file.ZIP";
  auto backend = BackendFactory::create_from_file(path.string());
  
  // Week 1: not implemented
  EXPECT_EQ(backend, nullptr);
}
```

---

## Implementation Checklist

### Morning Session (2-3 hours)

- [ ] Create `include/tebako/fs/backend_factory.h`
  - [ ] Complete class declaration with static methods
  - [ ] Comprehensive Doxygen documentation
  - [ ] Usage examples in comments
  - [ ] All method signatures with parameters

- [ ] Create `src/backend_factory.cpp`
  - [ ] Magic number constants (DWARFS_MAGIC, ZIP_LOCAL_MAGIC, ZIP_CENTRAL_MAGIC)
  - [ ] `create_from_file()` with detection chain
  - [ ] `create_dwarfs()` returning DwarfsBackend
  - [ ] `create_zip()` returning nullptr (Week 2 placeholder)
  - [ ] `is_dwarfs_format()` with magic detection
  - [ ] `is_zip_format()` with dual magic detection
  - [ ] `read_magic_bytes()` helper
  - [ ] `has_extension()` helper with case-insensitive comparison

### Afternoon Session (2-3 hours)

- [ ] Create `tests/test_backend_factory.cpp`
  - [ ] Test fixture with temp directory setup
  - [ ] Factory creation tests (create_dwarfs, create_zip)
  - [ ] Magic number detection tests (DwarFS, ZIP local, ZIP central)
  - [ ] Auto-detection tests (create_from_file)
  - [ ] Extension fallback tests (.dwarfs, .dfs, .zip, .jar, .apk)
  - [ ] Error case tests (unknown format, missing file, empty file)
  - [ ] Case-insensitive extension tests

- [ ] Update `CMakeLists.txt`
  - [ ] Add `src/backend_factory.cpp` to tfs library sources
  - [ ] Add `tests/test_backend_factory.cpp` to test target
  - [ ] Verify all dependencies

- [ ] Build and validate
  - [ ] `cmake .. && make -j$(nproc)`
  - [ ] `ctest -R test_backend_factory --verbose`
  - [ ] All tests passing
  - [ ] Zero compiler warnings

---

## Success Criteria

Week 1 Day 2 is complete when:

- [ ] `backend_factory.h` created with complete interface
- [ ] `backend_factory.cpp` implemented with all methods
- [ ] Format detection working (magic numbers + extensions)
- [ ] `test_backend_factory.cpp` with comprehensive coverage
- [ ] CMakeLists.txt updated correctly
- [ ] All tests passing
- [ ] Code compiles without warnings
- [ ] Documentation complete

---

## Key Differences from Original Plan

| Original Plan | Simplified Approach |
|--------------|---------------------|
| Dynamic BackendRegistry class | Static BackendFactory class |
| Singleton with instance() | No singleton, only static methods |
| Runtime registration | Compile-time known backends |
| Thread-safe with mutexes | No threading, no mutexes |
| BackendInfo struct | Not needed |
| Priority system | Simple if-else chain |
| std::map<> for backends | Direct factory methods |

---

## After Completion

1. Update documentation:
   - [ ] Mark Week 1 Day 2 complete in [`CONTINUATION_PLAN.md`](CONTINUATION_PLAN.md)
   - [ ] Update Progress Log with completion notes
   - [ ] Update [`README.md`](../README.md) Stage 2 status

2. Commit changes:
   ```bash
   git add include/tebako/fs/backend_factory.h
   git add src/backend_factory.cpp
   git add tests/test_backend_factory.cpp
   git add CMakeLists.txt
   git commit -m "feat(stage2): implement backend factory with format detection"
   ```

3. Prepare for Day 3:
   - Review [`DwarfsBackend refactoring plan`](STAGE_2_QUICK_START.md)
   - Understand existing memfs implementation
   - Plan FileHandle and DirectoryIterator concrete classes

---

## Reference Documentation

- **Detailed Design**: [`STAGE_2_BACKEND_FACTORY_DESIGN.md`](STAGE_2_BACKEND_FACTORY_DESIGN.md)
- **Architecture**: [`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md)
- **Interfaces**: [`filesystem.h`](../include/tebako/fs/filesystem.h)
- **Progress**: [`CONTINUATION_PLAN.md`](CONTINUATION_PLAN.md)

---

**Ready to start? Let's implement the Backend Factory!** 🚀