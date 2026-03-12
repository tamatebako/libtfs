# Stage 2 Week 2+: Production Readiness - Complete Continuation Plan

**Date**: 2025-12-22  
**Timeline**: 6-8 weeks  
**Status**: Option A - Complete ALL blockers before Tebako merger  
**Prerequisites**: Stage 2 Week 1 Complete ✅ (ZIP + SquashFS backends ready)

---

## Executive Summary

This plan details the complete path to production readiness for libtfs v2.0.0 before integration with Tebako. Based on the [Production Readiness Checklist](PRODUCTION_READINESS_CHECKLIST.md), we must complete **ALL P0 and P1 items** (42 items) to ensure a robust, production-quality release.

**Current Status**: 58% Complete (35/60 items)  
**Target**: 100% of P0 + P1 items (42 items)  
**Timeline**: 6-8 weeks compressed schedule

### Critical Path Overview

```
Week 1-2: C API & Embedded Image ──► Week 3-4: Tebako Integration ──►
Week 5-6: Performance & Quality ──► Week 7-8: Production Polish
```

---

## Phase 1: Core Integration Foundation (Weeks 1-2)

**Goal**: Complete C API and embedded image support - the foundation for Tebako integration

### Week 1: C API for Ruby Integration

**Days 1-3: C API Implementation (3 days)**

**P0 Blockers to Complete**:
- F4.1: C API wrapper functions defined
- F4.2: FD namespace separation
- F4.3: Path detection helpers
- F4.4: Initialization API
- F4.6: Error handling in C API

**Deliverables**:

1. **C API Header** (`include/tebako/fs/c_api.h`)
   ```c
   // Core lifecycle
   int tebako_fs_init(const void* data, size_t size, const char* mount_point);
   int tebako_fs_unmount(void);
   bool tebako_is_initialized(void);
   
   // File operations
   int tebako_open(const char* path, int flags);
   ssize_t tebako_read(int fd, void* buf, size_t count);
   off_t tebako_lseek(int fd, off_t offset, int whence);
   int tebako_close(int fd);
   
   // Directory operations
   void* tebako_opendir(const char* path);
   struct dirent* tebako_readdir(void* dir);
   int tebako_closedir(void* dir);
   
   // Metadata operations
   int tebako_stat(const char* path, struct stat* st);
   
   // Path detection
   bool tebako_path_is_embedded(const char* path);
   bool tebako_fd_is_embedded(int fd);
   
   // Error handling
   int tebako_get_errno(void);
   const char* tebako_strerror(int err);
   ```

2. **C API Implementation** (`src/c_api.cpp` - ~800 lines)
   - Extern "C" wrappers around C++ VFS
   - FD namespace separation (use high bit: 0x40000000)
   - Thread-safe access to global filesystem instance
   - Proper error code translation (errno compatibility)

3. **Unit Tests** (`tests/test_c_api.cpp` - 40+ tests)
   - Test all C API functions
   - Test FD namespace separation
   - Test path detection logic
   - Test error handling
   - Test thread safety

**Success Criteria**:
- ✅ All C API functions implemented
- ✅ 40+ unit tests passing
- ✅ No memory leaks (Valgrind clean)
- ✅ Thread-safe verified

**Days 4-5: Embedded Image Support (2 days)**

**P0 Blockers to Complete**:
- F5.1: Mount from memory buffer
- F5.2: Image metadata structure
- F5.3: Image discovery algorithm
- F5.4: Image validation

**Deliverables**:

1. **Image Metadata Structure** (`include/tebako/fs/embedded_image.h`)
   ```cpp
   struct EmbeddedImageMetadata {
       char marker[20];        // "TEBAKO_FS_IMAGE_V1"
       char format[8];         // "ZIP", "SQFS", "DWARFS"
       uint64_t image_size;    // Size in bytes
       uint64_t image_offset;  // Offset from file start
       uint32_t checksum;      // CRC32 of image data
       uint16_t version;       // Metadata version
       uint8_t reserved[26];   // Future use
   };
   ```

2. **Image Discovery** (`src/embedded_image.cpp` - ~300 lines)
   - Locate marker at end of executable
   - Read and validate metadata
   - mmap image data into memory
   - Verify checksum

3. **Memory Mounting** (Update all backends)
   - Add `mount_from_memory()` to ZIP backend
   - Add `mount_from_memory()` to SquashFS backend
   - Add `mount_from_memory()` to DwarFS backend
   - Update BackendFactory to support memory mounting

4. **Unit Tests** (`tests/test_embedded_image.cpp` - 20+ tests)
   - Test image discovery
   - Test metadata validation
   - Test memory mounting
   - Test corrupted image handling

**Success Criteria**:
- ✅ All backends support memory mounting
- ✅ Image discovery working
- ✅ Checksum validation functional
- ✅ 20+ unit tests passing

### Week 2: Execution Shim & Extraction

**Days 1-2: Execution Shim (2 days)**

**P0 Blockers to Complete**:
- I1.2: Shim implementation
- I1.4: Shim unit tests

**Deliverables**:

1. **Execution Shim** (`src/tebako_shim.c` - 150-200 lines)
   ```c
   int main(int argc, char* argv[]) {
       // 1. Parse --tebako-extract flag
       if (has_extract_flag(argc, argv)) {
           return extract_mode(argv);
       }
       
       // 2. Locate embedded image
       EmbeddedImage img = locate_embedded_image();
       
       // 3. Initialize libtfs
       if (tebako_fs_init(img.data, img.size, "/__tebako__") != 0) {
           die("Failed to initialize filesystem");
       }
       
       // 4. Hand off to Ruby
       return ruby_main(argc, argv);
   }
   ```

2. **Platform-Specific Helpers**
   - `get_executable_path()` for Linux/macOS/Windows
   - `mmap` wrapper for cross-platform memory mapping

3. **Unit Tests** (`tests/test_tebako_shim.cpp` - 15+ tests)
   - Test image location
   - Test initialization
   - Test error handling
   - Test extract flag parsing

**Success Criteria**:
- ✅ Shim compiles on all platforms
- ✅ Image location working
- ✅ 15+ unit tests passing

**Days 3-4: Image Packaging & Extraction (2 days)**

**P0 Blockers to Complete**:
- I1.3: Image packaging tool
- F4.5: Extraction API

**Deliverables**:

1. **Packaging Tool** (`tools/tebako_package` - Python script, ~400 lines)
   - Read executable file
   - Append archive
   - Write metadata
   - Write marker
   - Calculate checksum

2. **Extraction Implementation** (`src/extraction.cpp` - ~200 lines)
   - `tebako_extract_all(dest_path)` function
   - Recursive directory creation
   - File writing with permissions
   - Progress reporting (optional)

3. **Integration Test** (`tests/test_end_to_end_packaging.cpp`)
   - Create simple executable
   - Package with archive
   - Verify image can be found
   - Verify extraction works
   - Verify execution works

**Success Criteria**:
- ✅ Packaging tool working
- ✅ Extraction functional
- ✅ End-to-end test passing

**Day 5: Week 1-2 Integration Testing**

**P0 Blocker to Complete**:
- I1.5: End-to-end packaging test

**Tasks**:
1. Create test executable with embedded ZIP
2. Test shim initialization
3. Test file reading through C API
4. Test extraction
5. Verify on Linux, macOS, Windows
6. Document any platform issues

---

## Phase 2: Ruby Runtime Integration (Weeks 3-4)

**Goal**: Hook Ruby file I/O to use libtfs for embedded paths

### Week 3: Ruby File I/O Hooks

**Days 1-3: Core I/O Hooks (3 days)**

**P0 Blockers to Complete**:
- I2.2: Ruby file I/O hooks
- I2.3: Ruby directory I/O hooks

**Deliverables**:

1. **Ruby Patches** (5 files, ~100 lines total changes)

**File: `ruby/file.c`** (~30 lines changed)
```c
// In rb_file_open_internal
static int rb_file_open_internal(const char* path, int flags) {
    if (tebako_is_initialized() && tebako_path_is_embedded(path)) {
        return tebako_open(path, flags);
    }
    return open(path, flags);
}

// In rb_file_s_stat
static VALUE rb_file_s_stat(VALUE klass, VALUE fname) {
    const char* path = StringValueCStr(fname);
    struct stat st;
    
    if (tebako_is_initialized() && tebako_path_is_embedded(path)) {
        if (tebako_stat(path, &st) != 0) {
            rb_sys_fail(path);
        }
    } else {
        if (stat(path, &st) != 0) {
            rb_sys_fail(path);
        }
    }
    
    return stat_new(&st);
}
```

**File: `ruby/io.c`** (~20 lines changed)
```c
// In rb_io_read
static VALUE rb_io_read(int argc, VALUE *argv, VALUE io) {
    rb_io_t *fptr;
    GetOpenFile(io, fptr);
    
    if (tebako_fd_is_embedded(fptr->fd)) {
        // Use tebako_read
        ssize_t n = tebako_read(fptr->fd, buf, size);
        // ... handle result
    } else {
        // Use regular read
        ssize_t n = read(fptr->fd, buf, size);
        // ... handle result
    }
}
```

**File: `ruby/dir.c`** (~30 lines changed)
```c
// In dir_s_open
static VALUE dir_s_open(VALUE klass, VALUE dirname) {
    const char* path = StringValueCStr(dirname);
    
    if (tebako_is_initialized() && tebako_path_is_embedded(path)) {
        void* dir = tebako_opendir(path);
        if (!dir) rb_sys_fail(path);
        return dir_initialize(dir_alloc(klass), dir, path);
    } else {
        DIR* dir = opendir(path);
        if (!dir) rb_sys_fail(path);
        return dir_initialize(dir_alloc(klass), dir, path);
    }
}
```

2. **Ruby Helper Module** (`ruby/ext/tebako/tebako.c`)
   ```ruby
   module Tebako
     def self.root
       "/__tebako__"
     end
     
     def self.path(relative)
       File.join(root, relative)
     end
     
     def self.embedded?(path)
       path.start_with?(root)
     end
   end
   ```

3. **Integration Tests** (`tests/ruby/test_file_io.rb` - 30+ tests)
   - Test File.read from embedded
   - Test File.open + read
   - Test File.stat
   - Test Dir.entries
   - Test Dir.glob
   - Test require from embedded

**Success Criteria**:
- ✅ All Ruby file I/O routed correctly
- ✅ 30+ Ruby integration tests passing
- ✅ Mixed embedded + host FS working

**Days 4-5: $LOAD_PATH Integration (2 days)**

**P0 Blocker to Complete**:
- I2.4: Ruby $LOAD_PATH integration

**Tasks**:
1. Modify Ruby's load path initialization
2. Add `/__tebako__/lib` to $LOAD_PATH
3. Test `require 'module'` from embedded
4. Test `load 'script.rb'` from embedded
5. Ensure proper error messages for not found

**Deliverables**:
- Modified Ruby initialization
- 15+ tests for require/load
- Documentation of $LOAD_PATH behavior

### Week 4: Ruby Integration Testing & Validation

**Days 1-2: Comprehensive Ruby Tests (2 days)**

**P0 Blocker to Complete**:
- T2.5: Ruby integration tests

**Deliverables**:

1. **Ruby Test Suite** (`tests/ruby/` - 60+ tests total)
   - File I/O tests (30 tests)
   - Directory tests (15 tests)
   - Load path tests (15 tests)
   - Error handling tests (10 tests)
   - Performance tests (10 tests)

2. **Real Application Tests**
   - Package simple Sinatra app
   - Package simple Rails app
   - Verify execution
   - Test all features

**Success Criteria**:
- ✅ 60+ Ruby integration tests passing
- ✅ Real apps running successfully

**Days 3-4: Ruby Version Compatibility (2 days)**

**P0 Blocker to Complete**:
- I3.1: Ruby 3.0+ compatibility

**Tasks**:
1. Test with Ruby 3.0
2. Test with Ruby 3.1
3. Test with Ruby 3.2
4. Test with Ruby 3.3
5. Document any version-specific issues
6. Create compatibility matrix

**Day 5: Performance Validation**

**P1 Item to Complete**:
- I2.6: Ruby performance validation

**Tasks**:
1. Benchmark File.read vs host FS
2. Benchmark Dir.entries
3. Benchmark require
4. Ensure <10% overhead
5. Document performance characteristics

---

## Phase 3: Testing & Quality Assurance (Weeks 5-6)

**Goal**: Comprehensive testing, benchmarking, and quality validation

### Week 5: Performance Benchmarking & Cross-Platform

**Days 1-2: Comprehensive Benchmarking (2 days)**

**P1 Items to Complete**:
- T3.1-T3.8: Complete benchmarking suite

**Deliverables**:

1. **Benchmark Fixtures** (`tests/fixtures/benchmark/`)
   - Download perl-5.42.0.tar.gz (31 MB)
   - Create benchmark.zip
   - Create benchmark.sqfs
   - Create benchmark.dwarfs (if available)

2. **Benchmark Suite** (`tests/benchmark_scenarios.cpp` - ~600 lines)
   - Sequential read (large files)
   - Random read (many small files)
   - Directory listing (deep hierarchies)
   - Metadata operations (stat)
   - Seek operations
   - Compression ratio comparison

3. **Benchmark Reports**
   - Performance comparison table
   - Graphs (if tools available)
   - Recommendations by use case

**Success Criteria**:
- ✅ All benchmarks running
- ✅ Results documented
- ✅ Performance baselines established

**Days 3-5: Cross-Platform Validation (3 days)**

**P0 & P1 Items to Complete**:
- T4.1-T4.6: Cross-platform testing

**Tasks**:

1. **Linux Validation**
   - Test on Ubuntu 22.04 (x86_64)
   - Test on Ubuntu 22.04 (ARM64)
   - Test on Alpine Linux (musl libc)
   - Fix any Linux-specific issues

2. **macOS Validation**
   - Test on macOS x86_64
   - Test on macOS ARM64 (M1/M2)
   - Fix any macOS-specific issues

3. **Windows Validation**
   - Test on Windows (MSVC)
   - Test on Windows (MinGW)
   - Document known Windows issues
   - Fix critical Windows bugs

**Deliverables**:
- Platform compatibility matrix
- Platform-specific fixes
- CI/CD configuration updates

### Week 6: Quality & Safety Testing

**Days 1-2: Memory Safety (2 days)**

**P0 & P1 Items to Complete**:
- T5.1: Valgrind memory leak checks
- T5.2: AddressSanitizer clean
- T5.3: ThreadSanitizer clean

**Tasks**:
1. Run all tests under Valgrind
2. Fix any memory leaks
3. Run with ASAN enabled
4. Fix any ASAN errors
5. Run with TSAN enabled
6. Fix any race conditions

**Success Criteria**:
- ✅ No Valgrind errors
- ✅ ASAN clean
- ✅ TSAN clean

**Days 3-4: Stress Testing (2 days)**

**P1 Items to Complete**:
- T5.4: Stress testing (large archives)
- T5.5: Stress testing (many files)
- T5.6: Corrupted archive handling

**Tasks**:
1. Test with 1GB+ archives
2. Test with 100k+ files
3. Test corrupted archives
4. Test edge cases (empty files, symlinks, long paths)
5. Verify graceful error handling

**Day 5: Code Quality**

**P0 & P1 Items to Complete**:
- Q1.2: No compiler warnings
- Q1.3: Static analysis clean
- Q1.4: Code coverage >95%

**Tasks**:
1. Fix all compiler warnings (-Wall -Wextra)
2. Run clang-tidy and fix issues
3. Run cppcheck and fix issues
4. Generate coverage report
5. Add tests to reach >95% coverage

---

## Phase 4: Documentation & Production Readiness (Weeks 7-8)

**Goal**: Complete documentation and final production polish

### Week 7: Documentation

**Days 1-2: API Documentation (2 days)**

**P0 & P1 Items to Complete**:
- Q1.5: Doxygen documentation complete
- Q3.5: API reference documentation

**Tasks**:
1. Complete Doxygen comments for all public APIs
2. Generate API reference docs
3. Review and improve documentation
4. Add code examples to docs

**Days 3-4: User Documentation (2 days)**

**P0 & P1 Items to Complete**:
- Q3.6: Migration guide
- Q3.7: Performance guide
- Q3.8: Troubleshooting guide

**Deliverables**:

1. **Migration Guide** (`docs/MIGRATION_GUIDE.md`)
   - How to upgrade from old Tebako
   - Breaking changes
   - Code migration examples
   - Common pitfalls

2. **Performance Guide** (`docs/PERFORMANCE_GUIDE.md`)
   - When to use ZIP vs SquashFS vs DwarFS
   - Performance characteristics
   - Optimization tips
   - Benchmark data

3. **Troubleshooting Guide** (`docs/TROUBLESHOOTING.md`)
   - Common errors and solutions
   - Debug techniques
   - Platform-specific issues
   - FAQ

**Day 5: Final Documentation Review**

**Tasks**:
1. Review all documentation
2. Ensure consistency
3. Verify all examples work
4. Update README with final status

### Week 8: Final Testing & Release Preparation

**Days 1-2: DwarFS Integration (Optional P1)**

**P1 Items to Complete**:
- F2.6: DwarFS backend VFS integration
- F2.7: DwarFS FlatBuffers metadata
- T1.3: DwarFS unit tests
- T2.3: DwarFS integration tests

**Tasks**:
1. Update DwarFS backend to new VFS interface
2. Ensure FlatBuffers still working
3. Write 47 unit tests (matching ZIP/SquashFS pattern)
4. Write 13 integration tests
5. Update documentation

**Days 3-4: Final Integration Testing (2 days)**

**Tasks**:
1. Test complete packaging workflow
2. Test with multiple real applications
3. Verify all platforms
4. Run full test suite
5. Benchmark final performance

**Day 5: Release Preparation**

**P0 Items to Complete**:
- Q2.1: API versioning scheme defined
- Q2.2: ABI stability guarantees

**Tasks**:
1. Define semantic versioning policy
2. Document ABI stability guarantees
3. Create CHANGELOG for v2.0.0
4. Tag release candidate
5. Final review and sign-off

---

## Success Criteria Summary

### Minimum Viable Product (All P0 Complete)

- ✅ C API for Ruby integration (F4.1-F4.6)
- ✅ Embedded image support (F5.1-F5.4)
- ✅ Execution shim (I1.2-I1.5)
- ✅ Ruby file I/O hooks (I2.2-I2.4)
- ✅ Basic integration tests (T2.5)
- ✅ Memory safety verified (T5.1-T5.2)
- ✅ Documentation complete (Q3.5-Q3.6)

### Production Ready (All P0 + P1 Complete)

- ✅ DwarFS backend integrated (F2.6-F2.7)
- ✅ Comprehensive benchmarks (T3.1-T3.8)
- ✅ Cross-platform validated (T4.1-T4.6)
- ✅ Thread safety verified (T5.3)
- ✅ Code quality high (Q1.2-Q1.5)
- ✅ Complete documentation (Q3.6-Q3.8)
- ✅ Ruby version compatibility (I3.1)

---

## Risk Management

### High Priority Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Ruby integration breaks existing code | Critical | Extensive testing with real apps; maintain fallback |
| Cross-platform issues | High | Test early on all platforms; fix incrementally |
| Memory leaks in production | Critical | Valgrind/ASAN in CI; stress testing |
| Performance regression | High | Benchmark early; optimize hot paths |

### Contingency Plans

1. **If Ruby integration takes longer**: Focus on single Ruby version first (3.2), add others as P2
2. **If DwarFS is complex**: Defer to post-release (v2.1), focus on ZIP + SquashFS
3. **If platform issues severe**: Focus on Linux + macOS, Windows as P2
4. **If behind schedule**: Cut P1 items that can be post-release

---

## Weekly Deliverables Checklist

### Week 1
- [ ] C API header and implementation
- [ ] C API unit tests (40+)
- [ ] Embedded image structures
- [ ] Image discovery algorithm
- [ ] Memory mounting for all backends

### Week 2
- [ ] Execution shim implementation
- [ ] Packaging tool
- [ ] Extraction functionality
- [ ] End-to-end packaging test

### Week 3
- [ ] Ruby file I/O hooks
- [ ] Ruby directory I/O hooks
- [ ] Ruby helper module
- [ ] Ruby integration tests (30+)

### Week 4
- [ ] $LOAD_PATH integration
- [ ] Full Ruby test suite (60+)
- [ ] Ruby version compatibility
- [ ] Performance validation

### Week 5
- [ ] Benchmark fixtures (perl-5.42.0)
- [ ] Benchmark suite
- [ ] Cross-platform testing (6 platforms)
- [ ] Platform-specific fixes

### Week 6
- [ ] Valgrind clean
- [ ] ASAN/TSAN clean
- [ ] Stress testing complete
- [ ] Code coverage >95%

### Week 7
- [ ] Doxygen documentation
- [ ] API reference docs
- [ ] Migration guide
- [ ] Performance guide
- [ ] Troubleshooting guide

### Week 8
- [ ] DwarFS integration (optional)
- [ ] Final integration testing
- [ ] API versioning defined
- [ ] Release candidate ready

---

## Timeline Visualization

```
Week 1: [████████] C API & Image Support
         └─► Blocking: Ruby integration

Week 2: [████████] Shim & Packaging
         └─► Blocking: End-to-end flow

Week 3: [████████] Ruby File I/O
         └─► Blocking: Application execution

Week 4: [████████] Ruby Testing
         └─► Blocking: Production validation
         
Week 5: [████████] Benchmarks & Cross-Platform
         └─► Quality gate: Performance & Compatibility

Week 6: [████████] Safety & Quality
         └─► Quality gate: Memory & Thread Safety

Week 7: [████████] Documentation
         └─► Release gate: User experience

Week 8: [████████] Final Polish & Release
         └─► Release: v2.0.0
```

---

## Post-Phase 4: Integration with Tebako Repository

Once all P0 + P1 items are complete:

1. **Merge Strategy**
   - Create feature branch in Tebako repository
   - Import libtfs as submodule
   - Update Tebako build system
   - Migrate existing Tebako functionality
   - Comprehensive integration testing

2. **Deprecation Timeline**
   - v2.0.0: libtfs available, old system deprecated
   - v2.1.0: libtfs required, old system removed
   - v2.2.0+: libtfs only

---

## References

- [Production Readiness Checklist](PRODUCTION_READINESS_CHECKLIST.md) - Complete 60-item checklist
- [Tebako Integration Architecture](TEBAKO_INTEGRATION_ARCHITECTURE.md) - Integration design
- [Stage 2 VFS Design](STAGE_2_VFS_DESIGN.md) - VFS architecture
- [Testing Guide](TESTING.adoc) - Testing standards

---

**Document Version**: 1.0  
**Created**: 2025-12-22  
**Timeline**: 6-8 weeks to production-ready  
**Next Action**: Begin Week 1 Day 1 - C API implementation