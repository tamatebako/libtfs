# Stage 2 Implementation: ZIP & SquashFS Backends

**Date**: 2025-12-22
**Goal**: Complete ZIP and SquashFS backends with static compilation
**Timeline**: 14 days (compressed schedule)
**Status**: Ready for implementation

---

## Prerequisites Completed ✅

- [x] FileSystem abstract interface
- [x] FileHandle abstract interface  
- [x] DirectoryIterator abstract interface
- [x] Backend architecture designed (static factory pattern)
- [x] Random-access requirement documented (no TAR/CPIO)

---

## Implementation Overview

You are implementing two filesystem backends for libtfs:

1. **ZIP Backend** - Using libzip (static compilation)
2. **SquashFS Backend** - Using squashfs-tools-ng (static compilation)

Both must be:
- ✅ Statically compiled (no dynamic libraries)
- ✅ Built via CMake ExternalProject_Add
- ✅ Random-access capable (have central indexes)
- ✅ Fully tested

---

## Critical Constraints

### 1. Static Compilation Only

**DO**:
```cmake
ExternalProject_Add(_libzip
    URL https://libzip.org/download/libzip-1.10.1.tar.xz
    CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF  # STATIC
    BUILD_BYPRODUCTS <INSTALL_DIR>/lib/libzip.a
)
```

**DON'T**:
```cmake
find_package(LibZip REQUIRED)  # May find system dynamic library ❌
```

### 2. CMake-Only Orchestration

**DO**:
- Use `ExternalProject_Add` for dependencies
- Use CMake to configure, build, test
- Use `add_dependencies()` for build order

**DON'T**:
- Use vcpkg for dynamic linking
- Use pkg-config
- Use manual shell scripts for builds

### 3. Random Access Architecture

**DO**:
- Implement formats with central indexes (ZIP, SquashFS)
- Support O(1) or O(log n) file lookup
- Enable direct file access by offset

**DON'T**:
- Implement sequential formats (TAR, CPIO)
- Require scanning entire archive to find files
- Use solid compression without indexes

---

## Phase 1: BackendFactory (Day 1)

### Current Status
- Architecture designed ✅
- API specified ✅
- Ready to implement

### Tasks

#### 1. Create Header

File: `include/tebako/fs/backend_factory.h`

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
 * No registry, no singleton, no threads - just static methods.
 */
class BackendFactory {
 public:
  // Auto-detect format and create backend
  static std::unique_ptr<FileSystem> create_from_file(const std::string& archive_path);
  
  // Explicit backend creation
  static std::unique_ptr<FileSystem> create_dwarfs();
  static std::unique_ptr<FileSystem> create_zip();
  static std::unique_ptr<FileSystem> create_squashfs();
  
  // Format detection (public for testing)
  static bool is_dwarfs_format(const std::string& path);
  static bool is_zip_format(const std::string& path);
  static bool is_squashfs_format(const std::string& path);

 private:
  static bool read_magic_bytes(const std::string& path, uint8_t* buffer, size_t size);
  static bool has_extension(const std::string& path, const std::string& ext);
};

}  // namespace fs
}  // namespace tebako
```

#### 2. Implement Factory

File: `src/backend_factory.cpp`

Implementation includes:
- Magic number detection (DwarFS: "DWARFS", ZIP: 0x50 0x4B 0x03 0x04, SquashFS: "hsqs"/"sqsh")
- Extension fallback (case-insensitive)
- Backend instantiation

#### 3. Write Tests

File: `tests/test_backend_factory.cpp`

Test coverage:
- Factory creation tests
- Magic number detection tests
- Auto-detection tests
- Extension fallback tests
- Error cases

#### 4. Update CMakeLists.txt

```cmake
# Add source
target_sources(libtfs PRIVATE
    src/backend_factory.cpp
)

# Add test
add_executable(test_backend_factory
    tests/test_backend_factory.cpp
)
target_link_libraries(test_backend_factory
    libtfs
    GTest::gtest_main
)
add_test(NAME BackendFactory COMMAND test_backend_factory)
```

**Deliverable**: Working BackendFactory with format detection

---

## Phase 2: ZIP Backend (Days 2-6)

### Day 2-3: Static libzip Integration

#### 1. Add libzip to CMake

File: `CMakeLists.txt` (add after existing ExternalProject sections)

```cmake
include(ExternalProject)

# libzip version and URL
set(LIBZIP_VERSION "1.10.1")
set(LIBZIP_URL "https://libzip.org/download/libzip-${LIBZIP_VERSION}.tar.xz")
set(LIBZIP_SHA256 "dc3c8d5b4c8bbd09626864f6bcf93de701540f761782e1c5b3e5e1c974e7b63c")  # Verify hash

ExternalProject_Add(
    _libzip
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/_libzip
    URL ${LIBZIP_URL}
    URL_HASH SHA256=${LIBZIP_SHA256}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        
        # Static compilation
        -DBUILD_SHARED_LIBS=OFF
        
        # Minimize dependencies
        -DENABLE_COMMONCRYPTO=OFF
        -DENABLE_GNUTLS=OFF
        -DENABLE_MBEDTLS=OFF
        -DENABLE_OPENSSL=OFF
        -DENABLE_WINDOWS_CRYPTO=OFF
        -DENABLE_BZIP2=OFF
        -DENABLE_LZMA=OFF
        -DENABLE_ZSTD=OFF
        
        # Disable extras
        -DBUILD_TOOLS=OFF
        -DBUILD_REGRESS=OFF
        -DBUILD_EXAMPLES=OFF
        -DBUILD_DOC=OFF
    
    BUILD_BYPRODUCTS
        <INSTALL_DIR>/lib/libzip.a
        <INSTALL_DIR>/include/zip.h
)

# Get install directory
ExternalProject_Get_Property(_libzip INSTALL_DIR)
set(LIBZIP_INCLUDE_DIR ${INSTALL_DIR}/include)
set(LIBZIP_LIBRARY ${INSTALL_DIR}/lib/libzip.a)

# Import as target
add_library(libzip STATIC IMPORTED)
set_target_properties(libzip PROPERTIES
    IMPORTED_LOCATION ${LIBZIP_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${LIBZIP_INCLUDE_DIR}
)
add_dependencies(libzip _libzip)

# Link to libtfs
target_link_libraries(libtfs PRIVATE libzip)
```

#### 2. Verify Build

```bash
cd /path/to/libdwarfs
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target _libzip -j$(nproc)

# Check output
ls -lh build/_libzip/lib/libzip.a
# Should show static library

# Check no dynamic deps
file build/_libzip/lib/libzip.a
# Should say "current ar archive"
```

**Deliverable**: libzip.a built statically

---

### Day 4: ZipBackend Core

#### 1. Create Backend Header

File: `include/tebako/fs/backends/zip_backend.h`

Key classes:
- `ZipBackend` - Main backend implementing FileSystem
- `ZipFileHandle` - Concrete FileHandle for ZIP files
- `ZipDirectoryIterator` - Concrete DirectoryIterator

See detailed implementation in [`STAGE_2_COMPRESSED_IMPLEMENTATION_PLAN.md`](STAGE_2_COMPRESSED_IMPLEMENTATION_PLAN.md#day-4-zipbackend-core-implementation).

#### 2. Implement Backend

File: `src/backends/zip_backend.cpp`

Implementation notes:
- Use libzip API: `zip_open()`, `zip_fopen_index()`, `zip_fread()`, etc.
- Handle seek() via close/reopen (libzip limitation)
- Build directory index on mount
- Thread-safe with `std::shared_mutex`

#### 3. Update Factory

```cpp
// In backend_factory.cpp
#include <tebako/fs/backends/zip_backend.h>

std::unique_ptr<FileSystem> BackendFactory::create_zip() {
    return std::make_unique<ZipBackend>();
}
```

#### 4. Update CMakeLists.txt

```cmake
# Add backend source
target_sources(libtfs PRIVATE
    src/backends/zip_backend.cpp
)

# Ensure backends directory exists
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/backends)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs/backends)
```

**Deliverable**: Compiling ZipBackend

---

### Day 5-6: ZIP Testing

#### 1. Create Test Archive

```bash
cd tests/test_files
echo "Test content for ZIP" > test.txt
mkdir -p subdir/nested
echo "Nested file content" > subdir/nested/file.txt
zip -r test_archive.zip test.txt subdir/
```

#### 2. Write Comprehensive Tests

File: `tests/test_zip_backend.cpp`

Test suites:
- Mount/unmount lifecycle
- File existence checks
- File vs directory detection
- File reading (with seek)
- Directory listing
- Metadata (size, mtime)
- Error handling

Example tests in [`STAGE_2_COMPRESSED_IMPLEMENTATION_PLAN.md`](STAGE_2_COMPRESSED_IMPLEMENTATION_PLAN.md#day-5-6-zip-testing--integration).

#### 3. Add to CMake

```cmake
add_executable(test_zip_backend
    tests/test_zip_backend.cpp
)
target_link_libraries(test_zip_backend
    libtfs
    GTest::gtest_main
)
add_test(NAME ZipBackend COMMAND test_zip_backend)
```

#### 4. Run Tests

```bash
cd build
ctest -R ZipBackend --verbose
# All tests should pass
```

**Deliverable**: Fully tested ZIP backend

---

## Phase 3: SquashFS Backend (Days 7-12)

### Day 7-8: Static squashfs-tools-ng Integration

#### 1. Add squashfs-tools-ng to CMake

File: `CMakeLists.txt`

```cmake
# squashfs-tools-ng version
set(SQFS_VERSION "1.3.1")
set(SQFS_URL "https://github.com/AgentD/squashfs-tools-ng/archive/refs/tags/v${SQFS_VERSION}.tar.gz")
set(SQFS_SHA256 "e2b12c7e2f8ab9f3a46e5f4f2f3e3e27e8b33c4d5c0f9b3a7c8e3d2f1e0b9c8a")  # Verify

ExternalProject_Add(
    _squashfs_tools_ng
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/_squashfs
    URL ${SQFS_URL}
    URL_HASH SHA256=${SQFS_SHA256}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        
        # Static compilation
        -DBUILD_SHARED_LIBS=OFF
        
        # Compression support
        -DWITH_LZO=OFF       # Minimize deps
        -DWITH_LZ4=ON        # Common
        -DWITH_XZ=ON         # LZMA
        -DWITH_ZSTD=ON       # Modern
        -DWITH_ZLIB=ON       # Basic
        -DWITH_PTHREAD=ON    # Threading
        
        # Disable extras
        -DBUILD_TOOLS=OFF
        -DBUILD_DOC=OFF
    
    BUILD_BYPRODUCTS
        <INSTALL_DIR>/lib/libsquashfs.a
        <INSTALL_DIR>/include/sqsh.h
)

# Import target
ExternalProject_Get_Property(_squashfs_tools_ng INSTALL_DIR)
set(SQFS_INCLUDE_DIR ${INSTALL_DIR}/include)
set(SQFS_LIBRARY ${INSTALL_DIR}/lib/libsquashfs.a)

add_library(squashfs STATIC IMPORTED)
set_target_properties(squashfs PROPERTIES
    IMPORTED_LOCATION ${SQFS_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${SQFS_INCLUDE_DIR}
)
add_dependencies(squashfs _squashfs_tools_ng)

# Link to libtfs
target_link_libraries(libtfs PRIVATE squashfs)
```

#### 2. Handle Compression Libraries

SquashFS needs compression libraries. Add to CMake:

```cmake
# Find system compression libraries (will be linked statically if available)
find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)

# Link compression to squashfs target
target_link_libraries(libtfs PRIVATE
    ZLIB::ZLIB
    LibLZMA::LibLZMA
)
```

#### 3. Verify Build

```bash
cmake --build build --target _squashfs_tools_ng -j$(nproc)
ls -lh build/_squashfs/lib/libsquashfs.a
```

**Deliverable**: libsquashfs.a built statically

---

### Day 9-10: SquashFSBackend Core

#### 1. Create Backend Header

File: `include/tebako/fs/backends/squashfs_backend.h`

Similar structure to ZipBackend but using squashfs-tools-ng API.

#### 2. Implement Backend

File: `src/backends/squashfs_backend.cpp`

Key squashfs-tools-ng APIs:
- `sqsh_file_open()` - Open archive
- `sqsh_inode_from_path()` - Lookup by path (uses index!)
- `sqsh_file_reader_new()` - Read file data
- `sqsh_dir_reader_new()` - Iterate directory

Implementation pattern:
```cpp
bool SquashFSBackend::mount(const std::string& archive_path, 
                            const std::string& mount_point) {
    std::unique_lock lock(mutex_);
    
    int error;
    file_ = sqsh_file_open(archive_path.c_str(), &error);
    if (!file_) return false;
    
    super_ = sqsh_super_init(file_, &error);
    if (!super_) {
        sqsh_file_close(file_);
        return false;
    }
    
    dr_ = sqsh_dir_reader_new(super_, &error);
    if (!dr_) {
        sqsh_super_free(super_);
        sqsh_file_close(file_);
        return false;
    }
    
    archive_path_ = archive_path;
    mount_point_ = mount_point;
    return true;
}
```

#### 3. Update Factory

```cpp
// In backend_factory.cpp
#include <tebako/fs/backends/squashfs_backend.h>

std::unique_ptr<FileSystem> BackendFactory::create_squashfs() {
    return std::make_unique<SquashFSBackend>();
}
```

**Deliverable**: Compiling SquashFSBackend

---

### Day 11-12: SquashFS Testing

#### 1. Create Test SquashFS Image

```bash
cd tests/test_files

# Create test directory structure
mkdir -p sqfs_test/subdir
echo "SquashFS test content" > sqfs_test/test.txt
echo "Nested content" > sqfs_test/subdir/nested.txt

# Create SquashFS image (need mksquashfs tool)
# Install: apt-get install squashfs-tools (just for test creation)
mksquashfs sqfs_test test.sqfs -comp gzip

# Cleanup source
rm -rf sqfs_test
```

#### 2. Write Tests

File: `tests/test_squashfs_backend.cpp`

Similar test structure to ZipBackend tests.

#### 3. Add to CMake and Run

```cmake
add_executable(test_squashfs_backend
    tests/test_squashfs_backend.cpp
)
target_link_libraries(test_squashfs_backend
    libtfs
    GTest::gtest_main
)
add_test(NAME SquashFSBackend COMMAND test_squashfs_backend)
```

**Deliverable**: Fully tested SquashFS backend

---

## Phase 4: Integration & Documentation (Days 13-14)

### Day 13: Integration

#### 1. Update Mount Table

Extend `tebako_mount_target` variant:

```cpp
// In include/tebako/fs/internal/mount_table.h
typedef std::variant<
    std::string,                      // Host path
    uint32_t,                         // Legacy memfs index
    std::shared_ptr<FileSystem>       // New: Backend pointer
> tebako_mount_target;
```

#### 2. Public API Functions

File: `include/tebako/fs/memfs.h` (add new functions)

```cpp
/**
 * @brief Auto-detect and mount archive at mount point
 * 
 * @param archive_path Path to archive file
 * @param mount_point Virtual mount point
 * @return 0 on success, -1 on error
 */
int tebako_fs_mount_auto(const char* archive_path, const char* mount_point);

/**
 * @brief Mount ZIP archive
 */
int tebako_fs_mount_zip(const char* archive_path, const char* mount_point);

/**
 * @brief Mount SquashFS archive
 */
int tebako_fs_mount_squashfs(const char* archive_path, const char* mount_point);
```

Implementation in `src/tebako-memfs.cpp`:

```cpp
int tebako_fs_mount_auto(const char* archive_path, const char* mount_point) {
    auto backend = tebako::fs::BackendFactory::create_from_file(archive_path);
    if (!backend) {
        errno = EINVAL;
        return -1;
    }
    
    if (!backend->mount(archive_path, mount_point)) {
        errno = EIO;
        return -1;
    }
    
    auto& mt = tebako::sync_tebako_mount_table::get_tebako_mount_table();
    auto shared_backend = std::shared_ptr<tebako::fs::FileSystem>(std::move(backend));
    
    if (!mt.insert(0, mount_point, shared_backend)) {
        errno = EEXIST;
        return -1;
    }
    
    return 0;
}
```

#### 3. Integration Tests

File: `tests/test_multi_backend.cpp`

Test multiple backends mounted simultaneously:
- Mount DwarFS at /mnt/dwarfs
- Mount ZIP at /mnt/zip
- Mount SquashFS at /mnt/sqfs
- Verify all accessible

**Deliverable**: Integrated multi-backend support

---

### Day 14: Documentation

#### 1. Update README.adoc

File: `README.adoc`

Add sections:
- Multi-backend support (DwarFS, ZIP, SquashFS)
- Build instructions with static dependencies
- Usage examples for each backend
- Format detection capabilities

#### 2. Create User Guide

File: `docs/USER_GUIDE.adoc`

Comprehensive guide covering:
- Which format to use when
- Performance characteristics
- API reference
- Examples

#### 3. Archive Completed Docs

```bash
mkdir -p docs/archive/stage2
mv docs/STAGE_2_IMPLEMENTATION.md docs/archive/stage2/
mv docs/STAGE_2_SUMMARY.md docs/archive/stage2/
mv docs/CONTINUATION_PROMPT.md docs/archive/stage2/
```

Keep active docs:
- `docs/STAGE_2_VFS_DESIGN.md`
- `docs/STAGE_2_BACKEND_FACTORY_DESIGN.md`
- `docs/STAGE_2_FUTURE_BACKENDS.md`
- `docs/STAGE_2_COMPRESSED_IMPLEMENTATION_PLAN.md`

#### 4. Update CHANGELOG.md

```markdown
## [Stage 2] - 2025-01-XX

### Added
- Multi-backend VFS architecture
- Static factory pattern for backend creation
- ZIP backend with libzip (static compilation)
- SquashFS backend with squashfs-tools-ng (static compilation)
- Format auto-detection (magic numbers + extensions)
- Comprehensive test suites for all backends

### Changed
- Build system now uses CMake ExternalProject_Add for dependencies
- All dependencies built statically for portability

### Architecture
- Random-access requirement documented (excludes TAR/CPIO)
- Backend factory pattern (no dynamic registry)
- PIMPL pattern for backend implementations
```

**Deliverable**: Complete, updated documentation

---

## Verification Checklist

### Build Verification

```bash
# Clean build
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Check static linkage
file build/libtfs.a              # Should be ar archive
ldd build/examples/api_example    # Check minimal dynamic deps

# Verify no vcpkg/system libs
nm build/libtfs.a | grep -i "zip\|squash"  # Should show internal symbols
```

### Test Verification

```bash
cd build
ctest --verbose --output-on-failure

# Should see:
# - test_backend_factory: PASSED
# - test_zip_backend: PASSED  
# - test_squashfs_backend: PASSED
# - test_multi_backend: PASSED
```

### Functional Verification

```bash
# Run example program
./build/examples/api_example

# Should demonstrate:
# - Mount DwarFS archive
# - Mount ZIP archive
# - Mount SquashFS archive
# - Read files from each
# - List directories
# - Clean unmount
```

---

## Success Criteria

Stage 2 is complete when:

- [x] BackendFactory implemented with format detection
- [x] ZIP backend fully functional
- [x] SquashFS backend fully functional
- [x] All dependencies built statically
- [x] All tests passing (>95% coverage)
- [x] Documentation updated
- [x] Examples working
- [x] No dynamic library dependencies (except system: libc, libm, libpthread)

---

## Key Architecture Principles

### 1. Object-Oriented Design
- Each backend is a class implementing FileSystem interface
- FileHandle and DirectoryIterator are abstract interfaces
- PIMPL pattern hides implementation details

### 2. MECE (Mutually Exclusive, Collectively Exhaustive)
- Each backend handles one format exclusively
- Factory handles all detection logic
- No overlap in responsibilities

### 3. Separation of Concerns
- Factory: Backend selection and creation
- Backend: Filesystem operations
- Mount Table: Mount point management
- Public API: User-facing functions

### 4. Open/Closed Principle
- FileSystem interface is closed for modification
- New backends extend via inheritance
- Factory is open for extension (add methods)

### 5. Single Responsibility
- Backend: Only filesystem operations
- FileHandle: Only file I/O
- DirectoryIterator: Only directory traversal

---

## Reference Documentation

- **Implementation Plan**: [`STAGE_2_COMPRESSED_IMPLEMENTATION_PLAN.md`](STAGE_2_COMPRESSED_IMPLEMENTATION_PLAN.md)
- **Architecture**: [`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md)
- **Factory Design**: [`STAGE_2_BACKEND_FACTORY_DESIGN.md`](STAGE_2_BACKEND_FACTORY_DESIGN.md)
- **Format Analysis**: [`STAGE_2_FUTURE_BACKENDS.md`](STAGE_2_FUTURE_BACKENDS.md)

---

**Ready to implement?** Start with Day 1: BackendFactory! 🚀