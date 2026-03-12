# Stage 2 Week 2: Post-Memory Mounting - Continuation Prompt

**Date**: 2025-12-22
**Current Phase**: Memory Mounting Complete
**Next Phase**: Build Setup → Testing → Documentation → Integration
**Priority**: P0 (Critical Path)

---

## Current State

### ✅ What's Complete
- Memory mounting interface and implementation (9 files modified)
- ZIP and SquashFS backends support memory mounting
- BackendFactory auto-detection from memory
- C API `tebako_fs_init()` fully functional
- 6 new comprehensive test cases (57 total tests)
- All code syntax-validated

### ⏳ What's Pending
- Build environment setup (vcpkg configuration)
- Full compilation and test execution
- Official documentation updates
- Execution shim for file I/O interception
- Ruby integration and testing

---

## IMMEDIATE TASK: Build Environment Setup (1 hour)

### Problem
The build system has a vcpkg configuration issue preventing full compilation:
```
/Users/mulgogi/src/external/vcpkg/ports/xz-utils: error: xz-utils does not exist
CMake Error: CMAKE_MAKE_PROGRAM is not set
```

### Solution Steps

#### 1. Verify vcpkg Installation (10 min)

```bash
cd /Users/mulgogi/src/external/vcpkg
git pull  # Update to latest
./bootstrap-vcpkg.sh  # Rebuild if needed
```

#### 2. Check Required Ports (10 min)

Verify these packages are available:
```bash
./vcpkg search libzip
./vcpkg search squashfs-tools-ng
./vcpkg search argtable3
./vcpkg search gtest
```

If missing, install manually:
```bash
./vcpkg install libzip:arm64-osx
./vcpkg install squashfs-tools-ng:arm64-osx
./vcpkg install argtable3:arm64-osx
./vcpkg install gtest:arm64-osx
```

#### 3. Configure Build (15 min)

```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs
rm -rf build  # Clean start
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=/Users/mulgogi/src/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx
```

#### 4. Build C API Test (15 min)

```bash
cmake --build . --target test_c_api -j8
```

If successful, you should see:
```
[100%] Built target test_c_api
```

#### 5. Run Tests (10 min)

```bash
./test_c_api

# Expected output:
# [==========] Running 57 tests from 1 test suite.
# ...
# [  PASSED  ] 57 tests.
```

---

## TASK 2: Documentation Updates (1-2 hours)

### Official Documentation to Update

#### 1. Update README.adoc (30 min)

**File**: [`README.adoc`](../README.adoc)

Add new section after Features:

```adoc
== Memory Mounting

libtfs supports mounting archives directly from memory, enabling embedded executable use cases:

[source,c]
----
// Embed archive in executable
extern const uint8_t embedded_archive[];
extern const size_t embedded_archive_size;

// Initialize from memory
if (tebako_fs_init(embedded_archive, embedded_archive_size,
                   "/__tebako__") == 0) {
    // File operations work normally
    int fd = tebako_open("/__tebako__/app.rb", O_RDONLY);
    // ...
    tebako_fs_unmount();
}
----

The archive format (ZIP or SquashFS) is automatically detected from magic bytes.
The memory buffer must remain valid until `tebako_fs_unmount()` is called.
```

#### 2. Create C_API.adoc (45 min)

**File**: `docs/C_API.adoc`

Create comprehensive C API reference:

```adoc
= C API Reference
:toc:
:toclevels: 3

== Overview

The libtfs C API provides POSIX-compatible file system operations for accessing
files within mounted archives. It supports both file-based and memory-based
mounting.

== Lifecycle Management

=== tebako_fs_init_from_file

Mounts an archive from disk.

[source,c]
----
int tebako_fs_init_from_file(const char* archive_path,
                              const char* mount_point);
----

*Parameters*:

* `archive_path`: Path to ZIP or SquashFS archive
* `mount_point`: Virtual mount point (e.g., `/__tebako__`)

*Returns*: `0` on success, `-1` on error (check `tebako_get_errno()`)

[example]
====
[source,c]
----
if (tebako_fs_init_from_file("/app/data.zip", "/__tebako__") == 0) {
    // Filesystem is ready
}
----
====

=== tebako_fs_init

Mounts an archive from memory buffer.

[source,c]
----
int tebako_fs_init(const void* data, size_t size,
                   const char* mount_point);
----

*Parameters*:

* `data`: Pointer to archive data in memory
* `size`: Size of archive in bytes
* `mount_point`: Virtual mount point

*Returns*: `0` on success, `-1` on error

*Important*: The `data` buffer must remain valid until `tebako_fs_unmount()`.

[example]
====
[source,c]
----
extern const uint8_t archive_data[];
extern const size_t archive_size;

if (tebako_fs_init(archive_data, archive_size, "/__tebako__") == 0) {
    // Files accessible
}
----
====

// ... Continue with all C API functions ...
```

#### 3. Update TESTING.adoc (15 min)

**File**: [`docs/TESTING.adoc`](../docs/TESTING.adoc)

Add section for memory mounting tests:

```adoc
== Memory Mounting Tests

The C API test suite includes comprehensive memory mounting coverage:

[source,bash]
----
./test_c_api --gtest_filter="*Memory*"
----

*Test Cases*:

* `InitFromMemory_Success`: Basic memory mounting
* `InitFromMemory_ReadFile`: File I/O after memory mount
* `InitFromMemory_InvalidData`: Invalid format detection
* `InitFromMemory_NullData`: Null pointer validation
* `InitFromMemory_ZeroSize`: Size validation
* `InitFromMemory_NullMountPoint`: Mount point validation
```

---

## TASK 3: Execution Shim Implementation (2-3 hours)

### Overview

Create a shim layer that intercepts file I/O operations and redirects embedded file accesses to libtfs.

### Architecture

```
┌─────────────────────────────────────────────┐
│           Ruby Application                  │
│  (uses standard File, Dir, IO operations)   │
└────────────────┬────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────┐
│         Execution Shim Layer                │
│  - Intercepts: open, fopen, opendir, etc.  │
│  - Detects embedded paths: /__tebako__/*   │
│  - Routes to libtfs or native OS            │
└────────────────┬────────────────────────────┘
                 │
          ┌──────┴──────┐
          │             │
          ▼             ▼
    ┌─────────┐   ┌──────────┐
    │ libtfs  │   │ Native   │
    │ C API   │   │ OS calls │
    └─────────┘   └──────────┘
```

### Implementation Steps

#### Step 1: Create Shim Header (30 min)

**File**: `include/tebako/fs/shim.h`

```c
#ifndef TEBAKO_FS_SHIM_H
#define TEBAKO_FS_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the shim layer
// Must be called before any file operations
int tebako_shim_init(void);

// Cleanup the shim layer
void tebako_shim_cleanup(void);

// Check if shim is active
int tebako_shim_is_active(void);

// Shim-ed POSIX functions
int tebako_shim_open(const char* path, int flags, ...);
FILE* tebako_shim_fopen(const char* path, const char* mode);
DIR* tebako_shim_opendir(const char* path);
int tebako_shim_stat(const char* path, struct stat* st);
int tebako_shim_access(const char* path, int mode);

#ifdef __cplusplus
}
#endif

#endif // TEBAKO_FS_SHIM_H
```

#### Step 2: Implement Shim Logic (1-1.5 hours)

**File**: `src/shim.cpp`

Key implementation points:

1. **Path Detection**:
   ```cpp
   bool is_embedded_path(const char* path) {
       return tebako_path_is_embedded(path);
   }
   ```

2. **Open Interception**:
   ```cpp
   int tebako_shim_open(const char* path, int flags, ...) {
       if (is_embedded_path(path)) {
           return tebako_open(path, flags);
       }
       // Delegate to real open()
       return real_open(path, flags, ...);
   }
   ```

3. **FD Routing**:
   ```cpp
   ssize_t tebako_shim_read(int fd, void* buf, size_t count) {
       if (tebako_fd_is_embedded(fd)) {
           return tebako_read(fd, buf, count);
       }
       return real_read(fd, buf, count);
   }
   ```

#### Step 3: Ruby Integration (30-45 min)

**File**: `include/tebako/fs/ruby/io_hooks.h`

Hook into Ruby's file operations using Ruby C API:

```c
// Override Ruby's file operations
void tebako_ruby_init_hooks(void);

// Wrapper for Ruby's rb_file_open
VALUE tebako_rb_file_open(VALUE fname, VALUE mode);

// Wrapper for Ruby's rb_dir_open
VALUE tebako_rb_dir_open(VALUE dirname);
```

---

## TASK 4: Ruby Integration Testing (2-3 hours)

### Test Scenarios

#### 1. Basic File Reading (30 min)

Create test archive with Ruby script:
```ruby
# test.rb
puts File.read('/__tebako__/data.txt')
```

Embed and execute:
```bash
# Create archive
zip -r test.zip test.rb data.txt

# Test with libtfs
./ruby_test test.zip
```

#### 2. Require from Embedded Files (1 hour)

Test `require()` with embedded libraries:
```ruby
# main.rb
require_relative 'lib/helper'
Helper.greet
```

#### 3. Directory Operations (30 min)

Test `Dir` operations:
```ruby
# list.rb
Dir.entries('/__tebako__/').each { |f| puts f }
```

#### 4. File Metadata (30 min)

Test `File.stat`, `File.exist?`, etc.:
```ruby
# metadata.rb
puts "Size: #{File.size('/__tebako__/data.txt')}"
puts "Exists: #{File.exist?('/__tebako__/missing.txt')}"
```

---

## TASK 5: Performance Validation (1 hour)

### Benchmarks to Run

#### 1. Mount Performance

```c
// bench_mount.c
clock_t start = clock();
tebako_fs_init(data, size, "/__tebako__");
clock_t end = clock();
printf("Mount time: %.2f ms\n",
       (double)(end - start) / CLOCKS_PER_SEC * 1000);
```

Expected: < 10ms for typical archives

#### 2. Read Performance

```c
// bench_read.c
int fd = tebako_open("/__tebako__/large_file.dat", O_RDONLY);
char buffer[8192];
clock_t start = clock();
ssize_t total = 0;
while ((n = tebako_read(fd, buffer, sizeof(buffer))) > 0) {
    total += n;
}
clock_t end = clock();
printf("Read %ld bytes in %.2f ms (%.2f MB/s)\n", ...);
```

Expected: > 100 MB/s for sequential reads

#### 3. Directory Traversal

```c
// bench_dir.c
tebako_dir_t dir = tebako_opendir("/__tebako__/");
clock_t start = clock();
int count = 0;
while (tebako_readdir(dir)) count++;
clock_t end = clock();
printf("Traversed %d entries in %.2f ms\n", ...);
```

Expected: > 10,000 entries/second

---

## Success Criteria

### Build & Test ✅
- [ ] Clean compilation with no warnings
- [ ] All 57 tests pass
- [ ] No memory leaks (Valgrind clean)
- [ ] No thread safety issues (TSan clean)

### Documentation ✅
- [ ] README.adoc updated with memory mounting
- [ ] C_API.adoc created with full reference
- [ ] TESTING.adoc updated with new tests
- [ ] All examples tested and working

### Integration ✅
- [ ] Execution shim functional
- [ ] Ruby can read embedded files
- [ ] require() works with embedded libraries
- [ ] Dir operations work correctly
- [ ] File metadata operations work

### Performance ✅
- [ ] Mount time < 10ms
- [ ] Read throughput > 100 MB/s
- [ ] Directory traversal > 10K entries/s
- [ ] No memory overhead per file

---

## Timeline Estimate

| Phase | Duration | Priority |
|-------|----------|----------|
| Build Setup | 1 hour | P0 |
| Test Execution | 0.5 hours | P0 |
| Documentation | 1-2 hours | P1 |
| Execution Shim | 2-3 hours | P0 |
| Ruby Integration | 2-3 hours | P0 |
| Performance Testing | 1 hour | P1 |
| **TOTAL** | **7.5-10.5 hours** | **~1-2 days** |

---

## Getting Started

1. **First**, fix the build environment and run tests
2. **Then**, update documentation while code is fresh
3. **Next**, implement execution shim for file operations
4. **Finally**, integrate with Ruby and validate

Start with:
```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs
# Follow build setup steps above
```

---

## Questions to Address

1. **Memory Lifetime**: Confirmed - Caller owns buffer, must keep valid
2. **Multiple Archives**: Single mount for now, architecture supports multiple
3. **Thread Safety**: All operations are thread-safe with proper locking
4. **Error Recovery**: Full cleanup on any failure, can retry
5. **Performance**: Minimal overhead, FD namespace just one bitwise OR

---

**Document Version**: 1.0
**Created**: 2025-12-22
**Ready to Continue**: Yes
**Estimated Completion**: 2025-12-24