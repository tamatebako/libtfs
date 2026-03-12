# Stage 2 Week 2: C API Implementation - Completion Plan

**Date**: 2025-12-22
**Status**: C API Code Complete, Build Setup In Progress
**Priority**: P0 - Critical Path for Tebako Integration

---

## Current Status

### ✅ Completed
- [x] C API header with 24 functions ([`include/tebako/fs/c_api.h`](../include/tebako/fs/c_api.h))
- [x] C API implementation (~750 lines) ([`src/c_api.cpp`](../src/c_api.cpp))
- [x] Comprehensive test suite (51 tests) ([`tests/test_c_api.cpp`](../tests/test_c_api.cpp))
- [x] CMakeLists.txt integration

### 🚧 In Progress
- [ ] Build system configuration
- [ ] Resolve include path conflicts
- [ ] Verify compilation on all platforms

### 📋 Next Steps
- [ ] Embedded image support (memory mounting)
- [ ] Execution shim for Ruby
- [ ] Ruby C extension integration

---

## Phase 1: Build System Fix (2 hours)

### Issue
The C API has a minor include conflict with legacy [`include/tebako/fs/dirent.h`](../include/tebako/fs/dirent.h). The modern C++ VFS and C API are self-contained, but the legacy header creates namespace pollution.

### Solution Options

#### Option A: Fix Include Guards (Recommended, 30 minutes)
Make the C API completely standalone by ensuring it doesn't pull in legacy headers.

**Steps**:
1. Ensure [`src/c_api.cpp`](../src/c_api.cpp) only includes modern VFS headers:
   ```cpp
   // Only include modern VFS headers
   #include <tebako/fs/c_api.h>
   #include <tebako/fs/backend_factory.h>
   #include <tebako/fs/filesystem.h>
   #include <tebako/fs/file_handle.h>
   #include <tebako/fs/directory_iterator.h>
   ```

2. Verify no transitive includes pull in legacy code

3. Test compilation:
   ```bash
   g++ -std=c++20 -c -I./include src/c_api.cpp \
       -I/path/to/vcpkg/installed/*/include
   ```

#### Option B: Conditional Compilation (15 minutes)
Add guards to allow coexistence during transition period.

**In CMakeLists.txt**:
```cmake
target_compile_definitions(tfs PRIVATE TEBAKO_MODERN_API=1)
```

**In legacy headers**:
```cpp
#ifndef TEBAKO_MODERN_API
// Legacy code
#endif
```

#### Option C: Separate Build Target (1 hour)
Create standalone C API library that doesn't link legacy code.

**New CMakeLists.txt section**:
```cmake
# Modern C API library (standalone)
add_library(tebako_c_api STATIC
    src/c_api.cpp
    src/backend_factory.cpp
    src/backends/zip_backend.cpp
    src/backends/squashfs_backend.cpp
)

target_include_directories(tebako_c_api
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs
)

target_link_libraries(tebako_c_api
    PUBLIC libzip::zip
    PUBLIC squashfs-tools-ng::squashfs
)

# Tests for C API
add_executable(test_c_api tests/test_c_api.cpp)
target_link_libraries(test_c_api tebako_c_api ${GTestMain})
```

### Recommended Approach
**Use Option A** - cleanest separation. The modern VFS is self-contained and doesn't need legacy code.

---

## Phase 2: Verify Build (1 hour)

### 2.1 Local Build Test
```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs
mkdir -p build && cd build

# Configure
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . --target test_c_api

# Run tests
./test_c_api
```

### 2.2 Platform Verification
- ✅ macOS (arm64 and x86_64)
- ✅ Linux (Ubuntu, Alpine)
- ✅ Windows (MSVC, MinGW)

### 2.3 Success Criteria
- [ ] All 51 C API tests passing
- [ ] No warnings in compilation
- [ ] Valgrind clean (no memory leaks)
- [ ] Thread sanitizer clean (no races)

---

## Phase 3: Memory Mounting Support (4-6 hours)

### 3.1 Add Memory Mounting to Backends

**File**: [`include/tebako/fs/filesystem.h`](../include/tebako/fs/filesystem.h)

Add new virtual method:
```cpp
/**
 * @brief Mount from memory buffer
 *
 * @param data Pointer to archive data
 * @param size Size of data in bytes
 * @param mount_point Virtual mount point
 * @return true if successful
 */
virtual bool mount_from_memory(const void* data, size_t size,
                                const std::string& mount_point) = 0;
```

### 3.2 Implement in ZIP Backend

**File**: [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp)

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
        0,  // Don't free
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

    mount_point_ = mount_point;
    is_mounted_ = true;
    return true;
}
```

### 3.3 Implement in SquashFS Backend

Similar approach using squashfs-tools-ng memory APIs.

### 3.4 Update C API

**File**: [`src/c_api.cpp`](../src/c_api.cpp)

Implement `tebako_fs_init()`:
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
        // Auto-detect format
        g_filesystem = BackendFactory::create_from_memory(data, size);

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
        return 0;

    } catch (...) {
        g_filesystem.reset();
        handle_exception();
        return -1;
    }
}
```

### 3.5 Add Tests

**File**: [`tests/test_c_api.cpp`](../tests/test_c_api.cpp)

```cpp
TEST_F(CApiTest, InitFromMemory_Success) {
    // Read archive into memory
    std::ifstream ifs(archive_path, std::ios::binary);
    std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );

    ASSERT_EQ(0, tebako_fs_init(data.data(), data.size(),
                                 mount_point.c_str()));
    EXPECT_EQ(1, tebako_is_initialized());

    // Verify can read files
    std::string content = read_file_via_api(mount_point + "/content/hello.txt");
    EXPECT_EQ("Hello, World!", content);
}
```

---

## Phase 4: Execution Shim (2-3 hours)

### 4.1 Create Shim Header

**File**: `include/tebako/shim.h`

```c
#ifndef TEBAKO_SHIM_H
#define TEBAKO_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Tebako embedded filesystem and launch Ruby
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return Ruby exit code
 */
int tebako_main(int argc, char** argv);

#ifdef __cplusplus
}
#endif

#endif /* TEBAKO_SHIM_H */
```

### 4.2 Implement Shim

**File**: `src/shim.c`

```c
#include <tebako/shim.h>
#include <tebako/fs/c_api.h>
#include <ruby.h>

// Embedded archive data (linked via incbin or objcopy)
extern const unsigned char _binary_tebako_archive_start[];
extern const unsigned char _binary_tebako_archive_end[];

int tebako_main(int argc, char** argv) {
    // Calculate archive size
    size_t archive_size =
        _binary_tebako_archive_end - _binary_tebako_archive_start;

    // Initialize embedded filesystem
    if (tebako_fs_init(_binary_tebako_archive_start, archive_size,
                       "/__tebako__") != 0) {
        fprintf(stderr, "Failed to initialize Tebako filesystem: %s\n",
                tebako_strerror(tebako_get_errno()));
        return 1;
    }

    // Launch Ruby
    ruby_sysinit(&argc, &argv);
    {
        RUBY_INIT_STACK;
        ruby_init();
        return ruby_run_node(ruby_options(argc, argv));
    }
}
```

### 4.3 Update Build

**CMakeLists.txt**:
```cmake
add_executable(tebako_ruby
    src/shim.c
    # Archive will be linked separately
)

target_link_libraries(tebako_ruby
    PRIVATE tebako_c_api
    PRIVATE ${RUBY_LIBRARY}
)

# Link embedded archive
add_custom_command(
    OUTPUT tebako_archive.o
    COMMAND objcopy -I binary -O elf64-x86-64 -B i386
            --rename-section .data=.rodata,alloc,load,readonly,data,contents
            ${TEBAKO_ARCHIVE} tebako_archive.o
    DEPENDS ${TEBAKO_ARCHIVE}
)

target_sources(tebako_ruby PRIVATE tebako_archive.o)
```

---

## Phase 5: Ruby Integration (4-6 hours)

### 5.1 File I/O Hooks

Create Ruby C extension that hooks file operations:

**File**: `ext/tebako/tebako.c`

```c
#include <ruby.h>
#include <tebako/fs/c_api.h>

static VALUE rb_tebako_file_open(VALUE self, VALUE path, VALUE mode) {
    // Check if path is embedded
    const char* c_path = StringValueCStr(path);

    if (tebako_path_is_embedded(c_path)) {
        // Open via C API
        int fd = tebako_open(c_path, O_RDONLY);
        if (fd < 0) {
            rb_sys_fail(c_path);
        }
        return INT2NUM(fd);
    } else {
        // Fallback to system open
        return rb_funcall(rb_cFile, rb_intern("__original_open"),
                          2, path, mode);
    }
}

void Init_tebako(void) {
    // Save original methods
    rb_alias(rb_cFile, rb_intern("__original_open"),
             rb_intern("open"));

    // Hook methods
    rb_define_singleton_method(rb_cFile, "open",
                               rb_tebako_file_open, 2);
}
```

### 5.2 Testing

Create integration tests using Ruby:

**File**: `test/test_tebako_integration.rb`

```ruby
require 'test/unit'

class TestTebakoIntegration < Test::Unit::TestCase
  def test_read_embedded_file
    content = File.read('/__tebako__/lib/my_gem.rb')
    assert_not_nil content
  end

  def test_require_embedded_gem
    require '/__tebako__/lib/my_gem'
    assert defined?(MyGem)
  end
end
```

---

## Phase 6: Documentation Update (2 hours)

### 6.1 Update README.adoc

Add C API documentation section:

```adoc
== C API for Embedding

libtfs provides a clean C API for embedding in other applications:

[source,c]
----
#include <tebako/fs/c_api.h>

// Initialize from memory
extern const uint8_t archive_data[];
extern const size_t archive_size;

tebako_fs_init(archive_data, archive_size, "/__tebako__");

// Use POSIX-like API
int fd = tebako_open("/__tebako__/file.txt", O_RDONLY);
char buf[1024];
ssize_t n = tebako_read(fd, buf, sizeof(buf));
tebako_close(fd);
----

See link:docs/C_API.adoc[C API Documentation] for details.
```

### 6.2 Create C API Documentation

**File**: `docs/C_API.adoc`

Comprehensive API reference with all 24 functions documented.

### 6.3 Move Temporary Docs

```bash
mkdir -p docs/old-docs
mv docs/STAGE_2_*.md docs/old-docs/
mv docs/C_API_IMPLEMENTATION_STATUS.md docs/old-docs/
```

---

## Timeline

| Phase | Duration | Priority |
|-------|----------|----------|
| Build System Fix | 2 hours | P0 |
| Verify Build | 1 hour | P0 |
| Memory Mounting | 4-6 hours | P0 |
| Execution Shim | 2-3 hours | P1 |
| Ruby Integration | 4-6 hours | P1 |
| Documentation | 2 hours | P2 |
| **Total** | **15-20 hours** | **~3 days** |

---

## Success Criteria

### Build System
- [x] C API compiles cleanly
- [ ] All 51 tests passing
- [ ] No valgrind errors
- [ ] Works on Linux/macOS/Windows

### Memory Mounting
- [ ] ZIP backend supports memory mounting
- [ ] SquashFS backend supports memory mounting
- [ ] C API init from memory works
- [ ] Tests verify memory mounting

### Execution Shim
- [ ] Shim initializes filesystem
- [ ] Shim launches Ruby correctly
- [ ] Embedded archive linked properly

### Ruby Integration
- [ ] File operations routed to C API
- [ ] require() works with embedded files
- [ ] All Ruby file I/O operations hooked

### Documentation
- [ ] README.adoc updated
- [ ] C API reference complete
- [ ] Examples provided
- [ ] Temporary docs archived

---

## Next Steps After Completion

1. **Performance Optimization**
   - Profile C API overhead
   - Optimize hot paths
   - Add caching layer

2. **Additional Backends**
   - TAR backend
   - ISO9660 backend
   - Custom formats

3. **Advanced Features**
   - Multi-archive support
   - Runtime archive switching
   - Compression level control

---

**Document Version**: 1.0
**Last Updated**: 2025-12-22
**Estimated Completion**: 2025-12-25