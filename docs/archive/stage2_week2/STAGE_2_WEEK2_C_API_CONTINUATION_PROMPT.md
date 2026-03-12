# Stage 2 Week 2: C API Implementation - Continuation Prompt

**Date**: 2025-12-22
**Mode**: Code
**Phase**: Build System Fix → Memory Mounting → Execution Shim
**Status**: Ready to Continue

---

## Current Situation

The **C API implementation is code-complete** with 24 functions, 750 lines of implementation, and 51 comprehensive tests. However, there's a minor build system issue that needs resolution before we can proceed to the next phases.

### What's Done ✅
- [x] Complete C API header with 24 functions ([`include/tebako/fs/c_api.h`](../include/tebako/fs/c_api.h))
- [x] Full implementation (~750 lines) ([`src/c_api.cpp`](../src/c_api.cpp))
- [x] Comprehensive test suite (51 tests) ([`tests/test_c_api.cpp`](../tests/test_c_api.cpp))
- [x] CMakeLists.txt integration
- [x] Thread-safe implementation
- [x] POSIX-compatible API design
- [x] FD namespace separation (0x40000000 flag)

### What's Left 📋
- [ ] Fix build system (2 hours) - **START HERE**
- [ ] Memory mounting implementation (4-6 hours)
- [ ] Execution shim (2-3 hours)
- [ ] Ruby integration (4-6 hours)
- [ ] Documentation updates (2 hours)

---

## IMMEDIATE TASK: Fix Build System (2 hours)

### The Problem

The C API code is correct, but there's an include path conflict. The build system is trying to include legacy [`include/tebako/fs/dirent.h`](../include/tebako/fs/dirent.h) which conflicts with system dirent.h.

**Root Cause**: The modern VFS is self-contained and doesn't need legacy code, but the include paths cause namespace pollution.

### Solution: Make C API Fully Standalone

The C API should ONLY depend on:
1. Modern VFS headers (`include/tebako/fs/*.h` - NOT legacy headers in `include/*.h`)
2. System headers (fcntl.h, sys/stat.h, etc.)
3. Backend libraries (libzip, squashfs-tools-ng)

### Step-by-Step Fix

#### Step 1: Verify Minimal Includes (5 minutes)

Check that [`src/c_api.cpp`](../src/c_api.cpp) only includes:

```cpp
// System headers FIRST (to avoid conflicts)
#include <mutex>
#include <memory>
#include <unordered_map>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// ONLY modern VFS headers (in tebako/fs/ subdirectory)
#include <tebako/fs/c_api.h>
#include <tebako/fs/backend_factory.h>
#include <tebako/fs/filesystem.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>
```

**DO NOT include**:
- ❌ Any headers from `include/tebako-*.h` (legacy)
- ❌ `include/tebako/fs/common.h` (pulls in legacy)
- ❌ `include/tebako/fs/dirent.h` (conflicts with system)

#### Step 2: Check VFS Headers Don't Pull Legacy (10 minutes)

Verify these modern VFS headers are clean:
- [`include/tebako/fs/filesystem.h`](../include/tebako/fs/filesystem.h)
- [`include/tebako/fs/file_handle.h`](../include/tebako/fs/file_handle.h)
- [`include/tebako/fs/directory_iterator.h`](../include/tebako/fs/directory_iterator.h)
- [`include/tebako/fs/backend_factory.h`](../include/tebako/fs/backend_factory.h)

If any of these include `common.h` or other legacy headers, remove those includes.

#### Step 3: Fix Include Paths in CMakeLists.txt (15 minutes)

Update the include directories to prevent legacy header pollution:

```cmake
# Ensure modern headers take precedence
include_directories(BEFORE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs  # Modern VFS
    ${CMAKE_CURRENT_SOURCE_DIR}/include             # For version.h only
)

# For C API specifically, be very selective
target_include_directories(tfs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs
)
```

#### Step 4: Test Compilation (30 minutes)

```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs

# Clean build
rm -rf build
mkdir build && cd build

# Configure (you'll need proper vcpkg path)
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build just the C API test
cmake --build . --target test_c_api

# If successful, run tests
./test_c_api
```

#### Step 5: Verify All Tests Pass (30 minutes)

All 51 tests should pass:
- 8 lifecycle tests
- 15 file operation tests
- 10 directory operation tests
- 7 metadata tests
- 6 path detection tests
- 3 error handling tests
- 2 integration tests

If tests fail, debug and fix. The code logic is correct, so failures are likely due to:
- Test file paths
- Temporary directory issues
- ZIP creation command availability

#### Step 6: Clean Build on All Platforms (30 minutes)

Test on:
- macOS (arm64 and x86_64)
- Linux (Ubuntu)
- Windows (if available)

---

## NEXT TASK: Memory Mounting (4-6 hours)

Once build is working, implement memory mounting support.

### Step 1: Update FileSystem Interface (30 minutes)

**File**: [`include/tebako/fs/filesystem.h`](../include/tebako/fs/filesystem.h)

Add new virtual method after existing [`mount()`](../include/tebako/fs/filesystem.h:82):

```cpp
/**
 * @brief Mount an archive from memory buffer
 *
 * Mounts an archive that resides in memory (typically embedded in executable).
 * The memory buffer must remain valid for the lifetime of the mounted filesystem.
 *
 * @param data Pointer to archive data in memory
 * @param size Size of archive in bytes
 * @param mount_point Virtual mount point (e.g., "/__tebako__")
 * @return true if mount succeeded, false otherwise
 *
 * @note The caller must ensure 'data' remains valid
 * @note Only one archive can be mounted at a time
 */
virtual bool mount_from_memory(const void* data, size_t size,
                                const std::string& mount_point) = 0;
```

### Step 2: Implement in ZIP Backend (1-2 hours)

**File**: [`include/tebako/fs/backends/zip_backend.h`](../include/tebako/fs/backends/zip_backend.h)

Add declaration:
```cpp
bool mount_from_memory(const void* data, size_t size,
                       const std::string& mount_point) override;
```

**File**: [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp)

Implement:
```cpp
bool ZipBackend::mount_from_memory(const void* data, size_t size,
                                    const std::string& mount_point) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_mounted_) {
        return false;
    }

    // Create zip source from memory
    zip_error_t error;
    zip_source_t* src = zip_source_buffer_create(
        data, size,
        0,  // freep = 0, we don't own the memory
        &error
    );

    if (!src) {
        return false;
    }

    // Open archive from source
    archive_ = zip_open_from_source(src, ZIP_RDONLY, &error);
    if (!archive_) {
        zip_source_free(src);
        return false;
    }

    // Store details
    archive_path_ = "";  // No file path for memory mount
    mount_point_ = mount_point;
    is_mounted_ = true;

    return true;
}
```

### Step 3: Implement in SquashFS Backend (1-2 hours)

Similar to ZIP but using squashfs-tools-ng memory APIs.

**File**: [`include/tebako/fs/backends/squashfs_backend.h`](../include/tebako/fs/backends/squashfs_backend.h)
**File**: [`src/backends/squashfs_backend.cpp`](../src/backends/squashfs_backend.cpp)

Study the squashfs-tools-ng documentation for memory mounting APIs.

### Step 4: Update BackendFactory (30 minutes)

**File**: [`include/tebako/fs/backend_factory.h`](../include/tebako/fs/backend_factory.h)

Add new method:
```cpp
/**
 * @brief Create backend from memory buffer
 *
 * Auto-detects archive format from magic bytes in memory.
 *
 * @param data Pointer to archive data
 * @param size Size of archive
 * @return Unique pointer to FileSystem, or nullptr if format unknown
 */
static std::unique_ptr<FileSystem> create_from_memory(
    const void* data, size_t size);
```

**File**: [`src/backend_factory.cpp`](../src/backend_factory.cpp)

Implement:
```cpp
std::unique_ptr<FileSystem> BackendFactory::create_from_memory(
    const void* data, size_t size) {

    if (!data || size < 4) {
        return nullptr;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    // Check ZIP magic
    if (size >= 4 && bytes[0] == 'P' && bytes[1] == 'K' &&
        bytes[2] == 0x03 && bytes[3] == 0x04) {
        return create_zip();
    }

    // Check SquashFS magic
    if (size >= 4 &&
        ((bytes[0] == 'h' && bytes[1] == 's' &&
          bytes[2] == 'q' && bytes[3] == 's') ||
         (bytes[0] == 's' && bytes[1] == 'q' &&
          bytes[2] == 's' && bytes[3] == 'h'))) {
        return create_squashfs();
    }

    return nullptr;
}
```

### Step 5: Implement C API Function (30 minutes)

**File**: [`src/c_api.cpp`](../src/c_api.cpp)

Replace the stub implementation of [`tebako_fs_init()`](../src/c_api.cpp:95):

```cpp
extern "C" int tebako_fs_init(const void* data, size_t size,
                               const char* mount_point) {
    if (data == nullptr || size == 0 || mount_point == nullptr) {
        set_errno(EINVAL);
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_init_mutex);

    if (g_initialized) {
        set_errno(EEXIST);
        return -1;
    }

    try {
        // Auto-detect format from memory
        g_filesystem = tebako::fs::BackendFactory::create_from_memory(data, size);

        if (!g_filesystem) {
            set_errno(EINVAL);
            return -1;
        }

        // Mount from memory
        if (!g_filesystem->mount_from_memory(data, size, mount_point)) {
            g_filesystem.reset();
            set_errno(EIO);
            return -1;
        }

        g_mount_point = mount_point;
        g_initialized = true;
        set_errno(0);
        return 0;

    } catch (...) {
        g_filesystem.reset();
        handle_exception();
        return -1;
    }
}
```

### Step 6: Add Memory Mounting Tests (1 hour)

**File**: [`tests/test_c_api.cpp`](../tests/test_c_api.cpp)

Add new test cases:

```cpp
TEST_F(CApiTest, InitFromMemory_Success) {
    // Read archive file into memory
    std::ifstream ifs(archive_path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(ifs.is_open());

    size_t size = ifs.tellg();
    std::vector<uint8_t> data(size);

    ifs.seekg(0);
    ifs.read(reinterpret_cast<char*>(data.data()), size);
    ifs.close();

    // Initialize from memory
    ASSERT_EQ(0, tebako_fs_init(data.data(), data.size(),
                                 mount_point.c_str()));
    EXPECT_EQ(1, tebako_is_initialized());

    // Verify can read files
    std::string content = read_file_via_api(mount_point + "/content/hello.txt");
    EXPECT_EQ("Hello, World!", content);
}

TEST_F(CApiTest, InitFromMemory_InvalidData) {
    uint8_t bad_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(-1, tebako_fs_init(bad_data, sizeof(bad_data),
                                  mount_point.c_str()));
    EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiTest, InitFromMemory_NullData) {
    EXPECT_EQ(-1, tebako_fs_init(nullptr, 100, mount_point.c_str()));
    EXPECT_EQ(EINVAL, tebako_get_errno());
}
```

---

## SUBSEQUENT TASKS

### Task 3: Execution Shim (2-3 hours)

See [`docs/STAGE_2_WEEK2_C_API_COMPLETION_PLAN.md`](STAGE_2_WEEK2_C_API_COMPLETION_PLAN.md#phase-4-execution-shim-2-3-hours) for details.

### Task 4: Ruby Integration (4-6 hours)

See [`docs/STAGE_2_WEEK2_C_API_COMPLETION_PLAN.md`](STAGE_2_WEEK2_C_API_COMPLETION_PLAN.md#phase-5-ruby-integration-4-6-hours) for details.

### Task 5: Documentation (2 hours)

See [`docs/STAGE_2_WEEK2_C_API_COMPLETION_PLAN.md`](STAGE_2_WEEK2_C_API_COMPLETION_PLAN.md#phase-6-documentation-update-2-hours) for details.

---

## Reference Materials

### Key Files to Review
- [`include/tebako/fs/c_api.h`](../include/tebako/fs/c_api.h) - C API interface
- [`src/c_api.cpp`](../src/c_api.cpp) - Implementation
- [`tests/test_c_api.cpp`](../tests/test_c_api.cpp) - Test suite
- [`docs/STAGE_2_WEEK2_C_API_COMPLETION_PLAN.md`](STAGE_2_WEEK2_C_API_COMPLETION_PLAN.md) - Full plan
- [`docs/STAGE_2_WEEK2_C_API_STATUS_TRACKER.md`](STAGE_2_WEEK2_C_API_STATUS_TRACKER.md) - Progress tracking

### Documentation
- [Tebako Integration Architecture](TEBAKO_INTEGRATION_ARCHITECTURE.md)
- [ZIP Backend Docs](backends/ZIP_BACKEND.adoc)
- [SquashFS Backend Docs](backends/SQUASHFS_BACKEND.adoc)
- [Testing Guide](TESTING.adoc)

---

## Architecture Principles

Remember these principles when continuing implementation:

1. **Separation of Concerns**
   - C API is ONLY a thin wrapper
   - All business logic stays in C++ VFS
   - No duplication between layers

2. **Object-Oriented Design**
   - Backend polymorphism via FileSystem interface
   - Each class has single responsibility
   - Use inheritance, not conditionals

3. **MECE (Mutually Exclusive, Collectively Exhaustive)**
   - Every function handles ONE concern
   - No gaps in functionality
   - No overlapping responsibilities

4. **Extensibility**
   - New backends trivial to add
   - C API doesn't need changes for new backends
   - Open/closed principle

5. **Error Handling**
   - C++ uses exceptions internally
   - C API catches all exceptions
   - Converts to errno at boundary

---

## Common Pitfalls to Avoid

### ❌ DON'T
- Mix legacy and modern code
- Expose C++ types in C API
- Use global variables without locking
- Forget to test error paths
- Skip memory leak testing
- Assume single-threaded usage

### ✅ DO
- Keep C API minimal and focused
- Maintain thread safety
- Test thoroughly
- Document assumptions
- Follow POSIX semantics
- Use RAII internally

---

## Success Criteria

### Build System
- [ ] Compiles cleanly with no warnings
- [ ] All 51 tests pass
- [ ] Valgrind shows no leaks
- [ ] Thread sanitizer shows no races
- [ ] Works on macOS, Linux, Windows

### Memory Mounting
- [ ] ZIP backend supports memory mounting
- [ ] SquashFS backend supports memory mounting
- [ ] Auto-detection works from memory
- [ ] C API init from memory works
- [ ] Memory mounting tests pass
- [ ] Buffer lifetime handled correctly

### Integration
- [ ] Execution shim works
- [ ] Ruby can be launched
- [ ] Embedded archive mounted
- [ ] Ruby file I/O hooked
- [ ] require() works with embedded files

---

## Questions to Answer During Implementation

1. **Memory Lifetime**: Who owns the memory buffer passed to `mount_from_memory()`?
   - **Answer**: Caller owns it and must keep it valid

2. **Multiple Archives**: Can multiple archives be mounted simultaneously?
   - **Answer**: Not in Phase 1, but architecture supports it

3. **Thread Safety**: Are different threads safe to use same mounted FS?
   - **Answer**: Yes, all operations are thread-safe

4. **Error Recovery**: What happens if mount fails halfway through?
   - **Answer**: All resources cleaned up, can try again

5. **Performance**: Is there overhead for FD namespace separation?
   - **Answer**: Minimal (one bitwise OR per FD)

---

## Timeline Estimate

| Task | Duration | Priority |
|------|----------|----------|
| Fix Build System | 2 hours | P0 |
| Memory Mounting | 4-6 hours | P0 |
| Execution Shim | 2-3 hours | P1 |
| Ruby Integration | 4-6 hours | P1 |
| Documentation | 2 hours | P2 |
| **Total** | **14-19 hours** | **~2-3 days** |

---

## Getting Help

If you get stuck:

1. **Check Documentation**: All key decisions are documented
2. **Review Tests**: Tests show expected behavior
3. **Study Examples**: Example usage in test files
4. **Architecture First**: Think about proper design before coding
5. **Ask Questions**: Document assumptions and ask for review

---

**Document Version**: 1.0
**Created**: 2025-12-22
**Ready to Start**: Yes
**Estimated Completion**: 2025-12-25