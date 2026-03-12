# Stage 2 Week 2+: C API Implementation - Continuation Prompt

**Date**: 2025-12-22  
**Mode**: Code  
**Phase**: Week 1 Days 1-3 (C API for Ruby Integration)  
**Status**: Ready to Start

---

## Objective

Implement a **clean C API layer** over the C++ VFS to enable Ruby C extension integration. This is the **critical foundation** for Tebako integration, allowing Ruby's file I/O operations to be transparently routed through libtfs.

---

## Prerequisites ✅

Before starting, verify these are complete:

- ✅ VFS abstraction layer ([`FileSystem`](../include/tebako/fs/filesystem.h))
- ✅ ZIP backend fully functional
- ✅ SquashFS backend fully functional
- ✅ BackendFactory with auto-detection
- ✅ 120+ tests passing for existing backends

---

## Architecture Principles

### 1. Clean C Wrapper Design

**DO**:
- ✅ Provide pure C interface (extern "C")
- ✅ Use POSIX-compatible types and semantics
- ✅ Maintain thread safety from C++ layer
- ✅ Handle errors via errno-style codes
- ✅ Keep API minimal and focused

**DO NOT**:
- ❌ Expose C++ types in headers
- ❌ Use exceptions across C boundary
- ❌ Create new business logic in C layer
- ❌ Duplicate functionality from C++ VFS

### 2. FD Namespace Separation

To avoid conflicts between host OS file descriptors and libtfs file descriptors:

```c
// Reserve high bit for libtfs FDs
#define TEBAKO_FD_FLAG 0x40000000
#define TEBAKO_FD_MAX  0x0FFFFFFF

// Check if FD is from libtfs
inline bool tebako_fd_is_embedded(int fd) {
    return (fd & TEBAKO_FD_FLAG) != 0;
}

// Get internal FD (strip flag)
inline int tebako_fd_internal(int fd) {
    return fd & TEBAKO_FD_MAX;
}

// Create external FD (set flag)
inline int tebako_fd_external(int fd) {
    return fd | TEBAKO_FD_FLAG;
}
```

**Rationale**: This ensures libtfs FDs never collide with host OS FDs, allowing mixed usage within Ruby.

### 3. Global State Management

```cpp
namespace {
    std::unique_ptr<tebako::fs::FileSystem> g_filesystem;
    std::mutex g_init_mutex;
    bool g_initialized = false;
}
```

**Thread Safety**: All C API functions must acquire appropriate locks when accessing global state.

---

## Implementation Tasks

### Task 1: Create C API Header (2 hours)

**File**: `include/tebako/fs/c_api.h`

**Structure**:

```c
#ifndef TEBAKO_FS_C_API_H
#define TEBAKO_FS_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/* ============================================================
 * Lifecycle Management
 * ============================================================ */

/**
 * Initialize libtfs from memory-embedded image.
 * 
 * @param data Pointer to archive data in memory
 * @param size Size of archive in bytes
 * @param mount_point Virtual mount point (e.g., "/__tebako__")
 * @return 0 on success, -1 on error (check errno)
 */
int tebako_fs_init(const void* data, size_t size, const char* mount_point);

/**
 * Initialize libtfs from file path.
 * 
 * @param archive_path Path to archive file on disk
 * @param mount_point Virtual mount point
 * @return 0 on success, -1 on error
 */
int tebako_fs_init_from_file(const char* archive_path, const char* mount_point);

/**
 * Unmount and cleanup libtfs.
 */
void tebako_fs_unmount(void);

/**
 * Check if libtfs is initialized.
 * 
 * @return 1 if initialized, 0 otherwise
 */
int tebako_is_initialized(void);

/* ============================================================
 * File Operations
 * ============================================================ */

/**
 * Open a file from embedded filesystem.
 * 
 * Behaves like POSIX open(2). Returns a file descriptor with
 * the TEBAKO_FD_FLAG bit set to distinguish from host OS FDs.
 * 
 * @param path Absolute path within mount point
 * @param flags Open flags (O_RDONLY, etc.)
 * @return File descriptor on success, -1 on error
 */
int tebako_open(const char* path, int flags);

/**
 * Read from embedded file.
 * 
 * @param fd File descriptor from tebako_open()
 * @param buf Buffer to read into
 * @param count Number of bytes to read
 * @return Number of bytes read, -1 on error
 */
ssize_t tebako_read(int fd, void* buf, size_t count);

/**
 * Seek within embedded file.
 * 
 * @param fd File descriptor
 * @param offset Offset value
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return New file position, -1 on error
 */
off_t tebako_lseek(int fd, off_t offset, int whence);

/**
 * Close embedded file.
 * 
 * @param fd File descriptor
 * @return 0 on success, -1 on error
 */
int tebako_close(int fd);

/* ============================================================
 * Directory Operations
 * ============================================================ */

/**
 * Open directory from embedded filesystem.
 * 
 * @param path Directory path
 * @return Opaque directory handle, NULL on error
 */
void* tebako_opendir(const char* path);

/**
 * Read next directory entry.
 * 
 * @param dir Directory handle from tebako_opendir()
 * @return Pointer to dirent structure, NULL at end or error
 */
struct dirent* tebako_readdir(void* dir);

/**
 * Close directory.
 * 
 * @param dir Directory handle
 * @return 0 on success, -1 on error
 */
int tebako_closedir(void* dir);

/* ============================================================
 * Metadata Operations
 * ============================================================ */

/**
 * Get file status.
 * 
 * @param path File path
 * @param st Pointer to stat structure to fill
 * @return 0 on success, -1 on error
 */
int tebako_stat(const char* path, struct stat* st);

/* ============================================================
 * Path Detection
 * ============================================================ */

/**
 * Check if path is within embedded filesystem.
 * 
 * @param path Path to check
 * @return 1 if embedded, 0 otherwise
 */
int tebako_path_is_embedded(const char* path);

/**
 * Check if file descriptor is from libtfs.
 * 
 * @param fd File descriptor
 * @return 1 if embedded, 0 otherwise
 */
int tebako_fd_is_embedded(int fd);

/* ============================================================
 * Error Handling
 * ============================================================ */

/**
 * Get last error code.
 * 
 * @return errno-style error code
 */
int tebako_get_errno(void);

/**
 * Get error message string.
 * 
 * @param err Error code
 * @return Error message (do not free)
 */
const char* tebako_strerror(int err);

/* ============================================================
 * Extraction (for --tebako-extract)
 * ============================================================ */

/**
 * Extract all files to disk.
 * 
 * @param dest_path Destination directory
 * @return 0 on success, -1 on error
 */
int tebako_fs_extract_all(const char* dest_path);

#ifdef __cplusplus
}
#endif

#endif /* TEBAKO_FS_C_API_H */
```

**Deliverable**: Clean, well-documented C header file (~200 lines with comments)

### Task 2: Implement C API (4-5 hours)

**File**: `src/c_api.cpp`

**Key Implementation Details**:

#### Global State Management

```cpp
#include <tebako/fs/c_api.h>
#include <tebako/fs/backend_factory.h>
#include <tebako/fs/filesystem.h>

#include <mutex>
#include <memory>
#include <unordered_map>
#include <cerrno>

namespace {
    // Global filesystem instance
    std::unique_ptr<tebako::fs::FileSystem> g_filesystem;
    std::mutex g_init_mutex;
    bool g_initialized = false;
    std::string g_mount_point;
    
    // FD table: external FD -> FileHandle
    std::unordered_map<int, std::unique_ptr<tebako::fs::FileHandle>> g_fd_table;
    std::mutex g_fd_mutex;
    int g_next_fd = 1;  // Internal FD counter
    
    // DIR handle table: void* -> DirectoryIterator
    std::unordered_map<void*, std::unique_ptr<tebako::fs::DirectoryIterator>> g_dir_table;
    std::mutex g_dir_mutex;
    
    // Thread-local errno
    thread_local int g_tebako_errno = 0;
}
```

#### Initialization

```cpp
extern "C" int tebako_fs_init(const void* data, size_t size, 
                               const char* mount_point) {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    
    if (g_initialized) {
        g_tebako_errno = EEXIST;
        return -1;
    }
    
    try {
        // Auto-detect format and create backend
        g_filesystem = tebako::fs::BackendFactory::create_from_memory(
            data, size);
        
        if (!g_filesystem) {
            g_tebako_errno = EINVAL;
            return -1;
        }
        
        // Mount filesystem
        if (!g_filesystem->mount_from_memory(data, size, mount_point)) {
            g_filesystem.reset();
            g_tebako_errno = EIO;
            return -1;
        }
        
        g_mount_point = mount_point;
        g_initialized = true;
        return 0;
        
    } catch (const std::exception& e) {
        g_filesystem.reset();
        g_tebako_errno = EIO;
        return -1;
    }
}
```

#### File Operations

```cpp
extern "C" int tebako_open(const char* path, int flags) {
    if (!g_initialized) {
        g_tebako_errno = ENODEV;
        return -1;
    }
    
    try {
        auto handle = g_filesystem->open(path, flags);
        if (!handle) {
            g_tebako_errno = ENOENT;
            return -1;
        }
        
        // Allocate FD
        std::lock_guard<std::mutex> lock(g_fd_mutex);
        int internal_fd = g_next_fd++;
        int external_fd = internal_fd | TEBAKO_FD_FLAG;
        
        g_fd_table[internal_fd] = std::move(handle);
        return external_fd;
        
    } catch (const std::exception& e) {
        g_tebako_errno = EIO;
        return -1;
    }
}

extern "C" ssize_t tebako_read(int fd, void* buf, size_t count) {
    if (!tebako_fd_is_embedded(fd)) {
        g_tebako_errno = EBADF;
        return -1;
    }
    
    int internal_fd = fd & ~TEBAKO_FD_FLAG;
    
    std::lock_guard<std::mutex> lock(g_fd_mutex);
    auto it = g_fd_table.find(internal_fd);
    if (it == g_fd_table.end()) {
        g_tebako_errno = EBADF;
        return -1;
    }
    
    try {
        ssize_t n = it->second->read(buf, count);
        if (n < 0) {
            g_tebako_errno = EIO;
        }
        return n;
    } catch (const std::exception& e) {
        g_tebako_errno = EIO;
        return -1;
    }
}

// Similar implementations for lseek, close...
```

**Deliverable**: Complete C API implementation (~800 lines)

### Task 3: Create Unit Tests (4-5 hours)

**File**: `tests/test_c_api.cpp`

**Test Structure** (40+ tests):

```cpp
#include <gtest/gtest.h>
#include <tebako/fs/c_api.h>

class CApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test archive in memory
        // ...
    }
    
    void TearDown() override {
        tebako_fs_unmount();
    }
};

// ============================================================
// Lifecycle Tests (5 tests)
// ============================================================

TEST_F(CApiTest, InitFromMemory) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    EXPECT_EQ(1, tebako_is_initialized());
}

TEST_F(CApiTest, InitTwiceFails) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    EXPECT_EQ(-1, tebako_fs_init(data, size, "/__tebako__"));
    EXPECT_EQ(EEXIST, tebako_get_errno());
}

TEST_F(CApiTest, InitInvalidDataFails) {
    EXPECT_EQ(-1, tebako_fs_init(nullptr, 0, "/__tebako__"));
}

TEST_F(CApiTest, UnmountCleansUp) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    tebako_fs_unmount();
    EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiTest, OperationsFailAfterUnmount) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    tebako_fs_unmount();
    EXPECT_EQ(-1, tebako_open("/__tebako__/file.txt", O_RDONLY));
}

// ============================================================
// File Operations Tests (15 tests)
// ============================================================

TEST_F(CApiTest, OpenAndReadFile) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    
    int fd = tebako_open("/__tebako__/test.txt", O_RDONLY);
    ASSERT_GT(fd, 0);
    EXPECT_TRUE(tebako_fd_is_embedded(fd));
    
    char buf[100];
    ssize_t n = tebako_read(fd, buf, sizeof(buf));
    EXPECT_GT(n, 0);
    
    EXPECT_EQ(0, tebako_close(fd));
}

TEST_F(CApiTest, OpenNonExistentFileFails) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    
    int fd = tebako_open("/__tebako__/nonexistent.txt", O_RDONLY);
    EXPECT_EQ(-1, fd);
    EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, ReadFromInvalidFdFails) {
    char buf[10];
    EXPECT_EQ(-1, tebako_read(999, buf, sizeof(buf)));
    EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, SeekOperations) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    
    int fd = tebako_open("/__tebako__/test.txt", O_RDONLY);
    ASSERT_GT(fd, 0);
    
    // Seek to offset 10
    EXPECT_EQ(10, tebako_lseek(fd, 10, SEEK_SET));
    
    // Seek from current
    EXPECT_EQ(20, tebako_lseek(fd, 10, SEEK_CUR));
    
    // Seek from end
    off_t end = tebako_lseek(fd, 0, SEEK_END);
    EXPECT_GT(end, 0);
    
    tebako_close(fd);
}

// Continue with more file operation tests...

// ============================================================
// Directory Operations Tests (10 tests)
// ============================================================

TEST_F(CApiTest, OpenAndReadDirectory) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    
    void* dir = tebako_opendir("/__tebako__");
    ASSERT_NE(nullptr, dir);
    
    struct dirent* entry;
    int count = 0;
    while ((entry = tebako_readdir(dir)) != nullptr) {
        EXPECT_NE(nullptr, entry->d_name);
        count++;
    }
    
    EXPECT_GT(count, 0);
    EXPECT_EQ(0, tebako_closedir(dir));
}

// Continue with more directory tests...

// ============================================================
// Metadata Operations Tests (5 tests)
// ============================================================

TEST_F(CApiTest, StatFile) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    
    struct stat st;
    ASSERT_EQ(0, tebako_stat("/__tebako__/test.txt", &st));
    
    EXPECT_TRUE(S_ISREG(st.st_mode));
    EXPECT_GT(st.st_size, 0);
}

// Continue with more stat tests...

// ============================================================
// Path Detection Tests (5 tests)
// ============================================================

TEST_F(CApiTest, PathDetection) {
    EXPECT_TRUE(tebako_path_is_embedded("/__tebako__/file.txt"));
    EXPECT_TRUE(tebako_path_is_embedded("/__tebako__/dir/file.txt"));
    EXPECT_FALSE(tebako_path_is_embedded("/tmp/file.txt"));
    EXPECT_FALSE(tebako_path_is_embedded("relative/path.txt"));
}

TEST_F(CApiTest, FdDetection) {
    ASSERT_EQ(0, tebako_fs_init(data, size, "/__tebako__"));
    
    int fd = tebako_open("/__tebako__/test.txt", O_RDONLY);
    ASSERT_GT(fd, 0);
    
    EXPECT_TRUE(tebako_fd_is_embedded(fd));
    EXPECT_FALSE(tebako_fd_is_embedded(STDOUT_FILENO));
    
    tebako_close(fd);
}

// ============================================================
// Error Handling Tests (5 tests)
// ============================================================

TEST_F(CApiTest, ErrorMessages) {
    const char* msg = tebako_strerror(ENOENT);
    EXPECT_NE(nullptr, msg);
    EXPECT_NE(std::string(""), msg);
}

// Continue with more error tests...
```

**Deliverable**: Comprehensive C API test suite (40+ tests, ~600 lines)

### Task 4: Update Build System (1 hour)

**File**: `CMakeLists.txt`

Add C API sources:

```cmake
# C API library
add_library(tebako_c_api STATIC
    src/c_api.cpp
)

target_link_libraries(tebako_c_api
    PUBLIC tebako_fs
)

target_include_directories(tebako_c_api
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Tests
if(WITH_TESTS)
    add_executable(test_c_api tests/test_c_api.cpp)
    target_link_libraries(test_c_api
        PRIVATE tebako_c_api
        PRIVATE GTest::gtest_main
    )
    add_test(NAME CApiTests COMMAND test_c_api)
endif()
```

---

## Success Criteria

### Code Quality
- ✅ Clean C interface (no C++ types in header)
- ✅ Comprehensive Doxygen comments
- ✅ Thread-safe implementation
- ✅ No memory leaks (Valgrind clean)

### Testing
- ✅ 40+ unit tests passing
- ✅ All API functions tested
- ✅ Error paths tested
- ✅ FD namespace verified

### Integration
- ✅ Builds on Linux, macOS, Windows
- ✅ Compatible with Ruby C extensions
- ✅ No ABI issues

---

## Next Steps After C API

Once C API is complete and tested, proceed to:

1. **Embedded Image Support** (Week 1 Days 4-5)
   - Implement memory mounting in all backends
   - Create image metadata structures
   - Implement image discovery algorithm

2. **Execution Shim** (Week 2 Days 1-2)
   - Create minimal C shim (150-200 lines)
   - Use C API to initialize filesystem
   - Hand off to Ruby

---

## Reference Materials

### Key Files to Study
- [`include/tebako/fs/filesystem.h`](../include/tebako/fs/filesystem.h) - C++ VFS interface
- [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp) - Example backend
- [`tests/test_zip_backend.cpp`](../tests/test_zip_backend.cpp) - Test pattern to follow

### Documentation
- [Tebako Integration Architecture](TEBAKO_INTEGRATION_ARCHITECTURE.md) - Overall design
- [Production Readiness Checklist](PRODUCTION_READINESS_CHECKLIST.md) - P0 items
- [Testing Guide](TESTING.adoc) - Testing standards

---

## Common Pitfalls to Avoid

1. **❌ Exposing C++ in C API**
   - Don't use std::string in C API
   - Use const char* and manual memory management

2. **❌ Thread Safety Issues**
   - Always lock when accessing global state
   - Use thread_local for errno

3. **❌ FD Conflicts**
   - Always set TEBAKO_FD_FLAG bit
   - Never return raw internal FDs

4. **❌ Memory Leaks**
   - Always cleanup in error paths
   - Use RAII internally, careful with C API boundary

5. **❌ Exception Across C Boundary**
   - Catch ALL exceptions in C API functions
   - Convert to errno-style error codes

---

## Questions to Answer

Before considering this task complete, ensure you can answer:

1. How does the FD namespace separation work?
2. Where is the global filesystem instance stored?
3. How are errors propagated from C++ to C?
4. What happens if init is called twice?
5. How is thread safety maintained?
6. What is the lifecycle of a FileHandle?
7. How are directories represented in C API?

---

**Document Version**: 1.0  
**Created**: 2025-12-22  
**Ready to Start**: Yes  
**Estimated Duration**: 2-3 days  
**Next Review**: After C API implementation complete