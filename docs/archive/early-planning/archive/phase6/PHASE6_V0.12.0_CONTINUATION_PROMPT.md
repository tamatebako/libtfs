# Phase 6: v0.12.0 Development - Continuation Prompt

**Context**: Phase 5 Complete - v0.11.0 Production Ready ✅
**Next**: Phase 6 - v0.12.0 Development (DwarFS Backend + Extraction)
**Target Release**: Q1 2026
**Estimated Duration**: 8-10 weeks

---

## What Was Just Accomplished (Phase 5)

### ✅ v0.11.0 Production Ready
- **100% test pass rate** maintained (140/140 tests)
- **Complete documentation** for production deployment
- **Performance monitoring** infrastructure established
- **Tebako integration guide** with Ruby FFI examples
- **Deployment checklist** for validation
- **Future roadmap** through v1.0.0

### ✅ Key Deliverables Created
1. `docs/README.md` - Documentation index
2. `tests/performance_regression.sh` - Performance monitoring
3. `docs/TEBAKO_INTEGRATION.md` - Integration guide
4. `examples/ruby_integration_example.rb` - Ruby FFI example
5. `docs/DEPLOYMENT_CHECKLIST.md` - Deployment validation
6. `ROADMAP.md` - Future development plans
7. Updated `CHANGELOG.md`, `README.adoc`, `TESTING.adoc`

---

## Your Task: Phase 6 - DwarFS Backend Implementation

### Overview
Implement the DwarFS backend for libtfs v0.12.0, adding high-performance compression support alongside existing ZIP and SquashFS backends.

**Goal**: Complete DwarFS backend with extraction functionality and performance optimizations.

---

## Week 1-2: DwarFS Backend Core Implementation

### Task 1.1: Create DwarfsBackend Class

**File**: `include/tebako/fs/backends/dwarfs_backend.h`

Create header following the pattern of [`ZipBackend`](include/tebako/fs/backends/zip_backend.h):

```cpp
#pragma once

#include <tebako/fs/filesystem.h>
#include <memory>
#include <string>

namespace tebako {
namespace fs {

class DwarfsBackend : public FileSystem {
public:
  DwarfsBackend();
  ~DwarfsBackend() override;

  // Lifecycle
  bool mount(const std::string& archive_path, const std::string& mount_point) override;
  void unmount() override;
  bool is_mounted() const override;

  // File operations
  std::unique_ptr<FileHandle> open(const std::string& path, int flags) override;

  // Directory operations
  std::unique_ptr<DirectoryIterator> list_directory(const std::string& path) override;

  // Metadata
  bool stat(const std::string& path, struct stat* st) override;
  bool exists(const std::string& path) override;
  bool is_directory(const std::string& path) override;
  bool is_file(const std::string& path) override;

  // Backend info
  std::string backend_name() const override;
  std::string backend_version() const override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace fs
} // namespace tebako
```

**Implementation Strategy**:
1. Use PIMPL pattern to hide DwarFS library details
2. Integrate with existing DwarFS v0.9+ reader
3. Follow same patterns as ZipBackend and SquashFSBackend
4. Ensure thread-safety

### Task 1.2: Implement DwarfsFileHandle

Create file handle class for reading:

```cpp
class DwarfsFileHandle : public FileHandle {
public:
  ssize_t read(void* buf, size_t count) override;
  off_t seek(off_t offset, int whence) override;
  off_t tell() const override;
  bool eof() const override;
  // ... other methods
};
```

**Key Features**:
- Native seek support (like SquashFS)
- Efficient random access
- Proper EOF handling
- Thread-safe operations

### Task 1.3: Implement DwarfsDirectoryIterator

Create directory iterator:

```cpp
class DwarfsDirectoryIterator : public DirectoryIterator {
public:
  bool has_next() const override;
  DirectoryEntry next() override;
  void reset() override;
  // ... other methods
};
```

**Requirements**:
- Iterate through directory entries
- Provide full metadata (name, size, type, permissions)
- Handle nested directories
- Efficient traversal

### Task 1.4: Integrate with BackendFactory

Update [`BackendFactory`](src/backend_factory.cpp):

```cpp
// In create_dwarfs()
std::unique_ptr<FileSystem> BackendFactory::create_dwarfs() {
  return std::make_unique<DwarfsBackend>();
}

// In detect_format_from_file()
// Add DwarFS magic byte detection: "DWARFS"
if (memcmp(magic, "DWARFS", 6) == 0) {
  return ArchiveFormat::DWARFS;
}
```

**Success Criteria**:
- DwarfsBackend compiles without errors
- Factory can create DwarFS backend
- Format auto-detection works
- Basic mount/unmount functional

---

## Week 3-4: DwarFS Testing & Documentation

### Task 2.1: Create Comprehensive Test Suite

**File**: `tests/test_dwarfs_backend.cpp`

Follow pattern of [`test_zip_backend.cpp`](tests/test_zip_backend.cpp):

```cpp
TEST_F(DwarfsBackendTest, Mount_ValidArchive) {
  // Test mounting DwarFS archive
}

TEST_F(DwarfsBackendTest, Open_ValidFile) {
  // Test opening files
}

TEST_F(DwarfsBackendTest, Read_FullFile) {
  // Test reading file contents
}

// ... 47+ tests covering all operations
```

**Test Categories**:
- Mount/unmount lifecycle (5 tests)
- File operations (15 tests)
- Directory operations (10 tests)
- Metadata queries (8 tests)
- Thread safety (5 tests)
- Error handling (4 tests)

### Task 2.2: Create Integration Tests

**File**: `tests/test_dwarfs_integration.cpp`

Test end-to-end workflows:

```cpp
TEST_F(DwarfsIntegrationTest, AutoDetect_DwarfsMagic) {
  // Test format auto-detection
}

TEST_F(DwarfsIntegrationTest, MountReadUnmount_Workflow) {
  // Test complete workflow
}

// ... 13+ integration tests
```

### Task 2.3: Create Test Fixtures

Create DwarFS test archives:

```bash
# In tests/fixtures/dwarfs/
mkdwarfs -i /path/to/source -o simple.dwarfs
mkdwarfs -i /path/to/nested -o nested.dwarfs
# etc.
```

### Task 2.4: Write Documentation

**File**: `docs/backends/DWARFS_BACKEND.adoc`

Follow structure of [`ZIP_BACKEND.adoc`](docs/backends/ZIP_BACKEND.adoc):

```adoc
= DwarFS Backend

== Overview
High-performance compression backend using DwarFS v0.9+.

== Features
* Extremely high compression ratios
* Fast random access
* Native seek support
* Memory-efficient operation
* FlatBuffers serialization

== Architecture
[Diagram of DwarFS backend architecture]

== API Usage
[Code examples]

== Performance
[Benchmarks vs ZIP and SquashFS]
```

**Success Criteria**:
- All 60+ tests passing
- Documentation complete
- Performance benchmarks show < 5ms mount time
- Test coverage > 90%

---

## Week 5-6: Extraction Functionality

### Task 3.1: Design Extraction API

**C++ API**:

```cpp
// In FileSystem interface
struct ExtractOptions {
  bool preserve_permissions = true;
  bool preserve_timestamps = true;
  bool verbose = false;
  std::function<void(const std::string&, size_t, size_t)> progress_callback;
};

virtual bool extract(const std::string& src_path,
                    const std::string& dest_path,
                    const ExtractOptions& options = {}) = 0;

virtual bool extract_all(const std::string& dest_path,
                         const ExtractOptions& options = {}) = 0;
```

**C API** in [`c_api.h`](include/tebako/fs/c_api.h):

```c
// Extraction functions
int tebako_fs_extract(const char* src_path, const char* dest_path);
int tebako_fs_extract_all(const char* dest_path);

// Progress callback type
typedef void (*tebako_progress_callback_t)(const char* path, size_t current, size_t total);
int tebako_fs_extract_with_callback(const char* src_path, const char* dest_path,
                                    tebako_progress_callback_t callback);
```

### Task 3.2: Implement Extraction

Implement in each backend:
- `ZipBackend::extract()` and `extract_all()`
- `SquashFSBackend::extract()` and `extract_all()`
- `DwarfsBackend::extract()` and `extract_all()`

**Key Features**:
- Recursive directory extraction
- Preserve permissions (if supported)
- Preserve timestamps
- Progress callbacks
- Error handling

### Task 3.3: Add CLI Integration

Update [`tebakofs`](src/cli/tebakofs.cpp):

```cpp
// Add extract command
int cmd_extract(const char* archive, const char* dest, ExtractOptions opts);

// CLI usage:
// tebakofs extract archive.dwarfs /output
// tebakofs extract -p archive.zip file.txt  # Extract single file
// tebakofs extract --no-preserve-permissions archive.sqfs /out
```

### Task 3.4: Test Extraction

Create extraction tests:

```cpp
TEST_F(ExtractionTest, Extract_SingleFile) {
  // Test single file extraction
}

TEST_F(ExtractionTest, ExtractAll_RecursiveDirectory) {
  // Test full archive extraction
}

TEST_F(ExtractionTest, Extract_PreservePermissions) {
  // Test permission preservation
}

// ... 20+ extraction tests
```

**Success Criteria**:
- All extraction tests passing
- Permissions/timestamps preserved
- Progress callbacks working
- CLI extraction functional

---

## Week 7-8: Performance Optimizations

### Task 4.1: Directory Caching

Implement caching layer:

```cpp
class DirectoryCache {
public:
  void cache(const std::string& path, std::vector<DirectoryEntry> entries);
  std::optional<std::vector<DirectoryEntry>> lookup(const std::string& path);
  void invalidate(const std::string& path);
  void clear();

private:
  std::unordered_map<std::string, CachedEntry> cache_;
  size_t max_size_;  // Configurable
  // LRU eviction
};
```

**Integration**:
- Add to each backend
- Make cache size configurable
- Implement LRU eviction
- Thread-safe access

### Task 4.2: Read Buffer Optimization

Optimize read buffering:

```cpp
class BufferedFileHandle : public FileHandle {
private:
  static constexpr size_t BUFFER_SIZE = 64 * 1024;  // 64KB buffer
  std::vector<uint8_t> buffer_;
  size_t buffer_pos_;
  size_t buffer_valid_;
};
```

**Improvements**:
- Larger buffers (64KB → 256KB)
-Sequential read detection
- Prefetching for sequential access
- Zero-copy where possible

### Task 4.3: Benchmarking

Create performance tests:

```cpp
TEST_F(PerformanceTest, MountTime_DwarFS) {
  // Measure mount time
  EXPECT_LT(mount_time_ms, 5.0);
}

TEST_F(PerformanceTest, ReadThroughput_Sequential) {
  // Measure sequential read throughput
  EXPECT_GT(throughput_mbps, 200.0);
}

TEST_F(PerformanceTest, DirectoryCache_Speedup) {
  // Measure cache effectiveness
  EXPECT_GT(speedup_factor, 100.0);
}
```

Update [`docs/PERFORMANCE_BASELINE.md`](docs/PERFORMANCE_BASELINE.md) with new metrics.

**Success Criteria**:
- Mount time < 5ms (DwarFS)
- Read throughput > 200 MB/s
- Directory cache 100x speedup
- Test suite < 40s

---

## Week 9-10: Polish & Release

### Task 5.1: Bug Fixes & Refinement
- [ ] Fix any bugs discovered in testing
- [ ] Address code review feedback
- [ ] Optimize hot paths
- [ ] Clean up TODOs

### Task 5.2: Documentation Review
- [ ] Update `README.adoc` with DwarFS examples
- [ ] Update `docs/TESTING.adoc` with new tests
- [ ] Update `CHANGELOG.md` with v0.12.0 section
- [ ] Review all backend documentation

### Task 5.3: Release Preparation
- [ ] Run full test suite on all platforms
- [ ] Performance regression tests
- [ ] Memory leak detection
- [ ] Update version to 0.12.0
- [ ] Create release notes
- [ ] Tag v0.12.0

---

## Success Criteria

Phase 6 is complete when:
- [ ] All 225+ tests passing (140 existing + 85 new)
- [ ] DwarFS backend fully functional
- [ ] Extraction API complete (C++ and C)
- [ ] Performance targets met
- [ ] Documentation complete and reviewed
- [ ] All platforms validated
- [ ] Version 0.12.0 tagged

---

## Key Reminders

1. **Follow existing patterns** - Use ZIP and SquashFS backends as templates
2. **Write tests first** - TDD approach for all new features
3. **Document as you go** - Don't leave docs for the end
4. **Benchmark frequently** - Catch regressions early
5. **Platform test early** - Don't assume cross-platform compatibility

---

## Quick Start

```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs

# 1. Create DwarfsBackend header
# Edit include/tebako/fs/backends/dwarfs_backend.h

# 2. Create DwarfsBackend implementation
# Edit src/backends/dwarfs_backend.cpp

# 3. Update BackendFactory
# Edit src/backend_factory.cpp

# 4. Create test suite
# Edit tests/test_dwarfs_backend.cpp

# 5. Build and test
cmake -B build
cmake --build build
cd build && ctest --output-on-failure

# 6. Update progress tracker
# Edit docs/PHASE6_STATUS_TRACKER.md as you go
```

---

## Important Files

### Reference Implementation
- [`include/tebako/fs/backends/zip_backend.h`](include/tebako/fs/backends/zip_backend.h)
- [`src/backends/zip_backend.cpp`](src/backends/zip_backend.cpp)
- [`tests/test_zip_backend.cpp`](tests/test_zip_backend.cpp)

### Documentation Templates
- [`docs/backends/ZIP_BACKEND.adoc`](docs/backends/ZIP_BACKEND.adoc)
- [`docs/backends/SQUASHFS_BACKEND.adoc`](docs/backends/SQUASHFS_BACKEND.adoc)

### Testing Patterns
- [`tests/test_zip_integration.cpp`](tests/test_zip_integration.cpp)
- [`tests/test_c_api.cpp`](tests/test_c_api.cpp)

---

## Questions to Answer During Phase 6

1. **DwarFS Write Support**: Should we implement write operations in v0.12.0? (Recommend: No, focus on read-only)
2. **Cache Strategy**: What cache eviction policy works best? (Recommend: LRU with configurable size)
3. **Extraction Atomicity**: Should extraction be atomic or stream-based? (Recommend: Stream for large archives)
4. **Windows Support**: Is full DwarFS support feasible on Windows? (Investigate early in Week 1)

Document decisions in `docs/ARCHITECTURE.md`.

---

**The goal of Phase 6 is to deliver a production-ready v0.12.0 with DwarFS backend and extraction functionality, maintaining 100% test pass rate and comprehensive documentation.**

Good luck! 🚀