# Stage 2: VFS Abstraction & Multi-Backend Design

**Date**: 2025-12-21
**Status**: Architecture Design Phase
**Goal**: Add ZIP backend support through unified VFS interface

---

## Overview

Stage 2 introduces a plugin-based VFS architecture that allows libtfs to support multiple archive formats through a unified interface. The first additional backend will be ZIP, using libzip.

### Objectives

1. **VFS Abstraction**: Define a clean interface for all filesystem backends
2. **Backend Registry**: Implement a plugin system for format detection and registration
3. **ZIP Backend**: Add full ZIP archive support via libzip
4. **Testing**: Comprehensive tests for new architecture and ZIP operations
5. **Documentation**: Complete API documentation for backend developers

---

## Architecture Overview

### Current State (Stage 1)

```
Application
    ↓
tebako::fs API (public interface)
    ↓
DwarFS Implementation (direct, no abstraction)
    ↓
libdwarfs (FlatBuffers)
```

### Target State (Stage 2)

```
Application
    ↓
tebako::fs API (public interface)
    ↓
tebako::fs::FileSystem (abstract interface)
    ↓
    ├── DwarfsBackend (existing, refactored)
    │       ↓
    │   libdwarfs (FlatBuffers)
    │
    └── ZipBackend (new)
            ↓
        libzip
```

---

## Component Design

### 1. FileSystem Interface (Abstract Base)

**File**: `include/tebako/fs/filesystem.h`

```cpp
namespace tebako {
namespace fs {

// Forward declarations
class FileHandle;
class DirectoryIterator;

/**
 * @brief Abstract filesystem backend interface
 *
 * All filesystem backends must implement this interface.
 * Thread-safe: All methods must be thread-safe for concurrent access.
 */
class FileSystem {
public:
    virtual ~FileSystem() = default;

    // Lifecycle
    virtual bool mount(const std::string& archive_path,
                      const std::string& mount_point) = 0;
    virtual void unmount() = 0;
    virtual bool is_mounted() const = 0;

    // File operations
    virtual std::unique_ptr<FileHandle> open(const std::string& path,
                                            int flags) = 0;
    virtual bool exists(const std::string& path) const = 0;
    virtual bool is_file(const std::string& path) const = 0;
    virtual bool is_directory(const std::string& path) const = 0;

    // Directory operations
    virtual std::unique_ptr<DirectoryIterator> list_directory(
        const std::string& path) = 0;

    // Metadata
    virtual int64_t file_size(const std::string& path) const = 0;
    virtual time_t modification_time(const std::string& path) const = 0;
    virtual mode_t permissions(const std::string& path) const = 0;

    // Backend information
    virtual std::string backend_name() const = 0;
    virtual std::string backend_version() const = 0;
    virtual std::string archive_path() const = 0;
    virtual std::string mount_point() const = 0;
};

} // namespace fs
} // namespace tebako
```

### 2. FileHandle Interface

**File**: `include/tebako/fs/file_handle.h`

```cpp
namespace tebako {
namespace fs {

/**
 * @brief Abstract file handle for reading from backends
 */
class FileHandle {
public:
    virtual ~FileHandle() = default;

    virtual ssize_t read(void* buffer, size_t count) = 0;
    virtual off_t seek(off_t offset, int whence) = 0;
    virtual off_t tell() const = 0;
    virtual bool eof() const = 0;
    virtual void close() = 0;

    virtual std::string path() const = 0;
    virtual int64_t size() const = 0;
};

} // namespace fs
} // namespace tebako
```

### 3. DirectoryIterator Interface

**File**: `include/tebako/fs/directory_iterator.h`

```cpp
namespace tebako {
namespace fs {

/**
 * @brief Directory entry information
 */
struct DirectoryEntry {
    std::string name;
    bool is_directory;
    int64_t size;
    time_t mtime;
};

/**
 * @brief Abstract directory iterator
 */
class DirectoryIterator {
public:
    virtual ~DirectoryIterator() = default;

    virtual bool has_next() const = 0;
    virtual DirectoryEntry next() = 0;
    virtual void reset() = 0;
};

} // namespace fs
} // namespace tebako
```

### 4. Backend Factory (Simplified)

**File**: `include/tebako/fs/backend_factory.h`

**Design Philosophy**: Static factory pattern with no dynamic registration, no threads, and no runtime overhead. Backends are known at compile-time, with simple format auto-detection when needed.

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
   */
  static std::unique_ptr<FileSystem> create_from_file(
      const std::string& archive_path);

  /**
   * @brief Explicitly create DwarFS backend
   * @return Unique pointer to DwarfsBackend
   */
  static std::unique_ptr<FileSystem> create_dwarfs();

  /**
   * @brief Explicitly create ZIP backend
   * @return Unique pointer to ZipBackend
   */
  static std::unique_ptr<FileSystem> create_zip();

  /**
   * @brief Detect if file is DwarFS format
   * @param path Path to file
   * @return true if DwarFS format detected
   */
  static bool is_dwarfs_format(const std::string& path);

  /**
   * @brief Detect if file is ZIP format
   * @param path Path to file
   * @return true if ZIP format detected
   */
  static bool is_zip_format(const std::string& path);

 private:
  // Internal helpers for format detection
  static bool read_magic_bytes(const std::string& path,
                               uint8_t* buffer,
                               size_t size);
  static bool has_extension(const std::string& path,
                           const std::string& ext);
};

}  // namespace fs
}  // namespace tebako
```

**Key Design Decisions**:
- ✅ Static methods only - no state, no singleton
- ✅ Format detection via magic numbers + extensions
- ✅ Zero runtime overhead (inline-able by compiler)
- ✅ Simple API - explicit backend creation or auto-detection
- ✅ No thread synchronization needed
- ✅ Backends known at compile-time

### 5. DwarfsBackend (Refactored)

**File**: `include/tebako/fs/backends/dwarfs_backend.h`

```cpp
namespace tebako {
namespace fs {

/**
 * @brief DwarFS filesystem backend implementation
 *
 * Wraps existing DwarFS functionality in the FileSystem interface.
 */
class DwarfsBackend : public FileSystem {
public:
    DwarfsBackend();
    ~DwarfsBackend() override;

    // FileSystem interface implementation
    bool mount(const std::string& archive_path,
              const std::string& mount_point) override;
    void unmount() override;
    bool is_mounted() const override;

    std::unique_ptr<FileHandle> open(const std::string& path,
                                    int flags) override;
    bool exists(const std::string& path) const override;
    bool is_file(const std::string& path) const override;
    bool is_directory(const std::string& path) const override;

    std::unique_ptr<DirectoryIterator> list_directory(
        const std::string& path) override;

    int64_t file_size(const std::string& path) const override;
    time_t modification_time(const std::string& path) const override;
    mode_t permissions(const std::string& path) const override;

    std::string backend_name() const override { return "DwarFS"; }
    std::string backend_version() const override;
    std::string archive_path() const override { return archive_path_; }
    std::string mount_point() const override { return mount_point_; }

    // Format detection
    static bool can_handle(const std::string& path);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::string archive_path_;
    std::string mount_point_;
    mutable std::shared_mutex mutex_;
};

} // namespace fs
} // namespace tebako
```

### 6. ZipBackend (New)

**File**: `include/tebako/fs/backends/zip_backend.h`

```cpp
namespace tebako {
namespace fs {

/**
 * @brief ZIP filesystem backend implementation
 *
 * Provides read-only access to ZIP archives via libzip.
 */
class ZipBackend : public FileSystem {
public:
    ZipBackend();
    ~ZipBackend() override;

    // FileSystem interface implementation
    bool mount(const std::string& archive_path,
              const std::string& mount_point) override;
    void unmount() override;
    bool is_mounted() const override;

    std::unique_ptr<FileHandle> open(const std::string& path,
                                    int flags) override;
    bool exists(const std::string& path) const override;
    bool is_file(const std::string& path) const override;
    bool is_directory(const std::string& path) const override;

    std::unique_ptr<DirectoryIterator> list_directory(
        const std::string& path) override;

    int64_t file_size(const std::string& path) const override;
    time_t modification_time(const std::string& path) const override;
    mode_t permissions(const std::string& path) const override;

    std::string backend_name() const override { return "ZIP"; }
    std::string backend_version() const override;
    std::string archive_path() const override { return archive_path_; }
    std::string mount_point() const override { return mount_point_; }

    // Format detection
    static bool can_handle(const std::string& path);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::string archive_path_;
    std::string mount_point_;
    mutable std::shared_mutex mutex_;
};

} // namespace fs
} // namespace tebako
```

---

## Implementation Strategy

### Phase 1: VFS Interface (Week 1, Days 1-3)

1. **Create base interfaces** (Day 1) ✅
   - [`filesystem.h`](include/tebako/fs/filesystem.h)
   - [`file_handle.h`](include/tebako/fs/file_handle.h)
   - [`directory_iterator.h`](include/tebako/fs/directory_iterator.h)

2. **Implement backend factory** (Day 2)
   - [`backend_factory.h`](include/tebako/fs/backend_factory.h)
   - [`backend_factory.cpp`](src/backend_factory.cpp)
   - Static factory methods for backend creation
   - Format detection (magic numbers + extensions)
   - No dynamic registration, no threads, no state

3. **Refactor DwarfsBackend** (Day 3)
   - Wrap existing code in FileSystem interface
   - Implement concrete FileHandle for DwarFS
   - Implement concrete DirectoryIterator for DwarFS
   - Add format detection method

### Phase 2: libzip Integration (Week 1, Days 4-5)

4. **Add libzip dependency** (Day 4)
   - Update [`CMakeLists.txt`](CMakeLists.txt) with libzip
   - Use vcpkg for cross-platform consistency
   - Create FindLibZip.cmake if needed

5. **Create ZIP detection** (Day 5)
   - Magic number detection (PK\x03\x04)
   - Extension detection (.zip, .jar, .apk, etc.)
   - Central directory validation

### Phase 3: ZipBackend Implementation (Week 2, Days 1-4)

6. **Implement ZipBackend::mount** (Day 1)
   - Open ZIP archive with libzip
   - Build internal file index
   - Validate archive integrity

7. **Implement file operations** (Day 2)
   - ZipFileHandle class
   - Read operations (buffered)
   - Seek operations

8. **Implement directory operations** (Day 3)
   - ZipDirectoryIterator class
   - List entries with caching
   - Handle directory structure

9. **Implement metadata** (Day 4)
   - File sizes from central directory
   - Modification times
   - Permissions (approximated from ZIP attributes)

### Phase 4: Mount Table Integration (Week 2, Day 5)

10. **Update mount table**
    - Support multiple mount points with different backends
    - Path resolution across backends
    - Priority handling for overlapping mounts

### Phase 5: Testing (Week 3)

11. **Unit tests**
    - Backend interface tests
    - Registry tests
    - Format detection tests
    - Each backend independently

12. **Integration tests**
    - Mixed backend mounts
    - Path resolution
    - Performance benchmarks

---

## API Usage Examples

### Example 1: Auto-detect and mount

```cpp
#include <tebako/fs/backend_factory.h>

// Auto-detect format and mount
auto fs = tebako::fs::BackendFactory::create_from_file("/path/to/archive.zip");

if (fs && fs->mount("/path/to/archive.zip", "/mnt/app")) {
    auto handle = fs->open("/mnt/app/config.json", O_RDONLY);
    // ... read file ...
    fs->unmount();
}
```

### Example 2: Explicit backend selection

```cpp
#include <tebako/fs/backend_factory.h>

// Create specific backend
auto zip_fs = tebako::fs::BackendFactory::create_zip();

if (zip_fs->mount("app.zip", "/mnt/app")) {
    if (zip_fs->is_file("/mnt/app/readme.txt")) {
        auto size = zip_fs->file_size("/mnt/app/readme.txt");
        // ... use file info ...
    }
    zip_fs->unmount();
}
```

### Example 3: Multiple simultaneous mounts

```cpp
// Mount DwarFS for bulk data
auto dwarfs = tebako::fs::BackendFactory::create_dwarfs();
dwarfs->mount("data.dwarfs", "/mnt/data");

// Mount ZIP for config
auto zip = tebako::fs::BackendFactory::create_zip();
zip->mount("config.zip", "/mnt/config");

// Both mounted simultaneously
// /mnt/data/* from DwarFS
// /mnt/config/* from ZIP
```

---

## Format Detection

### Detection Priority

1. **Magic numbers** (highest priority)
   - DwarFS: Check for DwarFS magic signature
   - ZIP: Check for PK\x03\x04 or PK\x05\x06

2. **File extensions** (medium priority)
   - .dwarfs, .dfs → DwarFS
   - .zip, .jar, .apk, .war → ZIP

3. **Structure validation** (lowest priority)
   - DwarFS: Validate FlatBuffers metadata
   - ZIP: Validate central directory

### Implementation

```cpp
// In backend_factory.cpp
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
```

---

## Testing Strategy

### Unit Tests

Each component has isolated tests:

1. **BackendFactory** ([`tests/test_backend_factory.cpp`](tests/test_backend_factory.cpp))
   - Factory method creation (create_dwarfs, create_zip)
   - Format detection (magic numbers, extensions)
   - Auto-detection (create_from_file)
   - Error handling (unknown formats, missing files)

2. **DwarfsBackend** ([`tests/test_dwarfs_backend.cpp`](tests/test_dwarfs_backend.cpp))
   - Mount/unmount
   - File operations
   - Directory listing
   - Error handling

3. **ZipBackend** ([`tests/test_zip_backend.cpp`](tests/test_zip_backend.cpp))
   - Mount/unmount
   - File reading
   - Directory traversal
   - ZIP-specific features (compression, etc.)

### Integration Tests

1. **Multi-backend** ([`tests/test_multi_backend.cpp`](tests/test_multi_backend.cpp))
   - Multiple simultaneous mounts
   - Different backends at different mount points
   - Path resolution
   - Performance with mixed backends

2. **Format Detection** ([`tests/test_format_detection.cpp`](tests/test_format_detection.cpp))
   - Auto-detection accuracy
   - Handling of unknown formats
   - Extension vs. magic number conflicts
   - Misnamed files (e.g., .zip with DwarFS magic)

### Test Fixtures

Create sample archives:
- Small DwarFS image (<1MB)
- Small ZIP archive (<1MB)
- Archives with deep directory structures
- Archives with many files
- Corrupted archives for error testing

---

## Dependencies

### New Dependencies (Stage 2)

1. **libzip**
   - Version: 1.8.0+
   - License: BSD-3-Clause
   - Static linking: Yes
   - vcpkg: `zip`

### CMakeLists.txt Changes

```cmake
# Add libzip
find_package(LibZip REQUIRED)

# Link against libzip
target_link_libraries(libtfs PRIVATE
    LibZip::LibZip
    # ... existing dependencies ...
)
```

---

## Migration Path

### Backward Compatibility

The existing public API remains unchanged:

```cpp
// Old code continues to work
#include <tebako/fs/io.h>
#include <tebako/fs/memfs.h>

// Still works exactly as before
tebako_memfs_mount(...);
```

### New Code

New features use the backend interface:

```cpp
// New code uses backends
#include <tebako/fs/backend_registry.h>
#include <tebako/fs/backends/zip_backend.h>

auto fs = BackendRegistry::instance().create_backend("ZIP");
fs->mount("archive.zip", "/mnt/app");
```

### Integration

The mount table bridges both approaches:

```cpp
// Internal: mount table can use any backend
void mount_table_add(const std::string& mount_point,
                    std::unique_ptr<FileSystem> backend);

// Public API wraps this
int tebako_memfs_mount_zip(const char* archive, const char* mount_point) {
    auto backend = std::make_unique<ZipBackend>();
    if (!backend->mount(archive, mount_point)) {
        return -1;
    }
    mount_table_add(mount_point, std::move(backend));
    return 0;
}
```

---

## Success Criteria

Stage 2 is complete when:

- [x] FileSystem interface defined and documented
- [ ] BackendFactory implemented and tested
- [ ] DwarfsBackend refactored to use interface
- [ ] libzip integrated via vcpkg
- [ ] ZipBackend fully implemented
- [ ] All unit tests passing (100% coverage)
- [ ] Integration tests passing
- [ ] Mixed DwarFS/ZIP mounting works
- [ ] Documentation complete
- [ ] No regression in DwarFS performance

---

## Timeline

| Week | Days | Deliverable |
|------|------|-------------|
| 1 | 1-3 | VFS interface + DwarfsBackend refactor |
| 1 | 4-5 | libzip integration |
| 2 | 1-4 | ZipBackend implementation |
| 2 | 5 | Mount table integration |
| 3 | 1-5 | Testing and documentation |

**Total**: 3 weeks (15 working days)

---

## Risk Mitigation

### Risk: DwarFS refactoring breaks existing code

**Mitigation**:
- Keep existing implementation alongside new interface
- Gradual migration with thorough testing
- Maintain dual API during transition

### Risk: libzip static linking issues

**Mitigation**:
- Use vcpkg for consistent builds
- Test on all platforms early
- Document any platform-specific workarounds

### Risk: Performance degradation

**Mitigation**:
- Benchmark before and after
- Profile hot paths
- Optimize interface if overhead detected

---

## Future Extensions

After Stage 2, the architecture supports:

- **TAR backend** (Stage 3): libarchive integration
- **SquashFS backend**: Native squashfs support
- **Network backends**: HTTP/S3 remote archives
- **Encrypted backends**: ZIP with encryption
- **Write support**: Mutable backends

The plugin architecture makes these straightforward to add.

---

**Document Version**: 1.0
**Last Updated**: 2025-12-21
**Next Review**: After Phase 1 completion