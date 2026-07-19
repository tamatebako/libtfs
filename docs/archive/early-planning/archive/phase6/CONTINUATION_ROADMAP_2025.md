# libtfs Continuation Roadmap - 2025

**Generated**: 2025-12-30
**Status**: Active Planning Document
**Target**: Production-Ready v0.12.0

---

## Executive Summary

Based on analysis of the codebase, the following items are **ACTUALLY IMPLEMENTED** but marked incorrectly in the old checklist:

✅ **C API for Ruby Integration** - Fully implemented in [`src/c_api.cpp`](../src/c_api.cpp) (701 lines)
✅ **DwarFS Backend** - Fully implemented in [`src/backends/dwarfs_backend.cpp`](../src/backends/dwarfs_backend.cpp) (741 lines)
✅ **Memory Mounting** - Implemented for ZIP, SquashFS, and DwarFS backends

The following are **ACTUALLY MISSING** and need completion:

❌ **Extraction API Implementation** (stub at line 649-663 in c_api.cpp)
❌ **DwarFS Backend Documentation** ([`docs/backends/DWARFS_BACKEND.adoc`](../docs/backends/DWARFS_BACKEND.adoc) does not exist)
❌ **Performance Benchmarking Suite** (no benchmarks exist)
❌ **Cross-Platform Testing Matrix** (no comprehensive validation)
❌ **Tebako Integration Components** (shim, packaging tool, Ruby hooks)

---

## Critical Path to v0.12.0

### Phase 1: Core Functionality (Week 1-2)

#### 1.1 Extraction API Implementation
**Priority**: P0 (Blocker for Tebako integration)
**Effort**: 2-3 days
**Files to Create/Modify**:
- Modify: [`src/c_api.cpp`](../src/c_api.cpp) lines 649-663
- Create: `src/extraction.cpp` (new file)
- Create: `include/tebako/fs/extraction.h` (new file)

**Implementation Tasks**:
```cpp
// ... existing code ...
extern "C" int tebako_fs_extract_all(const char* dest_path) {
    // Implement recursive extraction:
    // 1. Validate dest_path exists and is writable
    // 2. Iterate through all files in mounted filesystem
    // 3. Create directories as needed
    // 4. Extract each file preserving:
    //    - POSIX permissions (from backend)
    //    - Modification times
    //    - Directory structure
    // 5. Handle errors gracefully
    // 6. Optional: Progress callback support
}
// ... existing code ...
```

**Testing Requirements**:
- Create `test/test_extraction.cpp`
- Test cases:
  * Extract entire archive to empty directory
  * Extract with existing files (overwrite behavior)
  * Extract with permission preservation
  * Extract large archives (>100 MB)
  * Extract with deep directory trees (>10 levels)
  * Error handling (read-only destination, disk full)

**Acceptance Criteria**:
- [ ] All files extracted correctly
- [ ] Permissions preserved (mode_t accurate)
- [ ] Timestamps preserved (mtime accurate)
- [ ] Directory structure maintained
- [ ] Error handling robust
- [ ] Tests: 20+ covering all scenarios

---

#### 1.2 DwarFS Backend Documentation
**Priority**: P1 (Critical for user adoption)
**Effort**: 1 day
**Files to Create**:
- Create: [`docs/backends/DWARFS_BACKEND.adoc`](../docs/backends/DWARFS_BACKEND.adoc)

**Documentation Structure** (following existing pattern from [`ZIP_BACKEND.adoc`](../docs/backends/ZIP_BACKEND.adoc)):

```asciidoc
= DwarFS Backend Documentation
:toc:
:toclevels: 3

== Purpose

DwarFS backend provides read-only access to DwarFS archives (v0.9+) with:
- Exceptional compression ratios (70:1 typical)
- Native seek support (<0.1ms)
- Full POSIX permissions
- Block-level deduplication
- FlatBuffers metadata

== Architecture

[Detailed architecture diagram showing]
- DwarfsBackend class structure
- PIMPL pattern implementation
- FileHandle and DirectoryIterator implementations
- DwarFS reader integration

== Features

=== Compression Performance
[Benchmark data]

=== Native Seek Support
[Code examples showing seek operations]

=== Memory Mounting
[Example of mounting from embedded data]

== Usage Examples

=== Basic File Reading
[source,cpp]
----
// Example code here
----

=== Directory Traversal
[source,cpp]
----
// Example code here
----

=== Metadata Access
[source,cpp]
----
// Example code here
----

== Performance Characteristics

[Table comparing DwarFS vs ZIP vs SquashFS]

== Limitations

- Read-only access
- DwarFS v0.9+ required
- Platform support notes

== Implementation Details

- PIMPL pattern usage
- Thread safety via std::shared_mutex
- Error handling strategy
```

**Acceptance Criteria**:
- [ ] All sections complete with examples
- [ ] Performance data included
- [ ] Code examples tested and working
- [ ] Cross-references to source code accurate
- [ ] Diagrams included where helpful

---

### Phase 2: Testing Infrastructure (Week 3-4)

#### 2.1 DwarFS Backend Comprehensive Testing
**Priority**: P0 (Blocker for production)
**Effort**: 2 days
**Files to Create**:
- Create: `test/test_dwarfs_backend.cpp` (pattern from `test_zip_backend.cpp`)
- Create: `test/test_dwarfs_integration.cpp` (pattern from `test_zip_integration.cpp`)
- Create: `test/fixtures/test.dwarfs` (test archive)

**Test Coverage Required**:
```cpp
// Test suite structure (47 tests minimum, matching ZIP backend)

// Basic Operations (10 tests)
- Mount/unmount
- Mount from memory
- Multiple mount attempts
- Unmount when not mounted

// File Operations (15 tests)
- Open existing file
- Open non-existent file
- Read small file (<1KB)
- Read large file (>1MB)
- Seek operations (SEEK_SET, SEEK_CUR, SEEK_END)
- Tell position
- EOF detection
- Multiple opens of same file
- Concurrent reads

// Directory Operations (12 tests)
- List root directory
- List subdirectory
- Nested directory traversal
- Empty directory
- Large directory (>100 entries)
- Iterator reset

// Metadata Operations (10 tests)
- File size query
- Modification time
- Permissions query
- File type detection (regular vs directory)
- Non-existent files

// Thread Safety (recommended 10+ tests would match the zip_ pattern)
```

**Acceptance Criteria**:
- [ ] 47+ tests implemented (matching ZIP backend)
- [ ] 100% pass rate
- [ ] Thread safety validated
- [ ] Memory leaks checked (Valgrind clean)

---

#### 2.2 Performance Benchmarking Suite
**Priority**: P1 (Critical for validation)
**Effort**: 3 days
**Files to Create**:
- Create: `benchmarks/benchmark_suite.cpp`
- Create: `benchmarks/benchmark_report.py` (generates markdown reports)
- Create: `benchmarks/fixtures/` (test datasets)

**Benchmark Categories**:

1. **Sequential Read Performance**
```cpp
// Measure MB/s for large file sequential reads
// Test with: 1MB, 10MB, 100MB, 1GB files
// All backends: ZIP, SquashFS, DwarFS
```

2. **Random Read Performance**
```cpp
// Measure latency for random small reads
// Test with: 1KB, 4KB, 16KB, 64KB blocks
// Pattern: Random offsets in large file
```

3. **Seek Operation Performance**
```cpp
// Measure seek latency
// ZIP: Should show 5-20ms (close/reopen overhead)
// SquashFS/DwarFS: Should show <0.1ms (native seek)
```

4. **Directory Listing Performance**
```cpp
// Measure time to list large directories
// Test with: 10, 100, 1000, 10000 entries
```

5. **Mount Time**
```cpp
// Measure filesystem initialization time
// Various archive sizes: 1MB, 10MB, 100MB, 1GB
```

6. **Compression Ratio**
```cpp
// Measure compressed vs uncompressed size
// Test dataset: perl-5.42.0 source tree
// Expected ratios:
// - DwarFS: 70:1
// - SquashFS: 45:1
// - ZIP: 40:1
```

**Benchmark Fixtures**:
- Create: `benchmarks/create_fixtures.sh`
```bash
#!/bin/bash
# Download perl-5.42.0 source
# Create archives:
# - test_perl.zip
# - test_perl.sqfs
# - test_perl.dwarfs
# Each with identical content for fair comparison
```

**Automated Report Generation**:
```python
# benchmarks/benchmark_report.py
# Generates markdown table with:
# - Backend name
# - Sequential read (MB/s)
# - Random read (ops/sec)
# - Seek latency (ms)
# - Mount time (ms)
# - Compression ratio
# - Archive size
```

**Acceptance Criteria**:
- [ ] All 6 benchmark categories implemented
- [ ] Fixtures created (perl-5.42.0 in all formats)
- [ ] Automated report generation working
- [ ] Baseline performance documented
- [ ] Results match README.adoc claims

---

#### 2.3 Cross-Platform Testing Matrix
**Priority**: P0 (Blocker for production)
**Effort**: 4 days
**Platforms to Validate**:

| Platform | Arch | CI Status | Local Test | Priority |
|----------|------|-----------|------------|----------|
| Ubuntu 22.04 | x86_64 | ✅ CI exists | ❌ Needs validation | P0 |
| Ubuntu 22.04 | ARM64 | ✅ Cirrus CI | ❌ Needs validation | P1 |
| macOS 13+ | x86_64 | ✅ CI exists | ❌ Needs validation | P0 |
| macOS 14+ | ARM64 | ✅ CI exists | ❌ Needs validation | P0 |
| Windows 11 | x86_64 | ⚠️ CI exists | ❌ Needs validation | P1 |
| Alpine Linux | x86_64 | ✅ CI exists | ❌ Needs validation | P1 |

**Testing Tasks per Platform**:
```bash
# For each platform:
1. Build all backends (ZIP, SquashFS, DwarFS)
2. Run full test suite (140 tests)
3. Run benchmarks
4. Check for:
   - Memory leaks (Valgrind on Linux)
   - Thread issues (TSAN)
   - Undefined behavior (ASAN, UBSAN)
5. Document any platform-specific issues
6. Update CI if needed
```

**Known Platform Issues to Check**:
- SquashFS: macOS availability (currently returns nullptr)
- DwarFS: Windows support limitations
- Memory alignment on ARM64

**Acceptance Criteria**:
- [ ] All platforms tested with full test suite
- [ ] 100% pass rate on P0 platforms
- [ ] >90% pass rate on P1 platforms
- [ ] Platform-specific issues documented
- [ ] CI updated to catch regressions

---

### Phase 3: Tebako Integration (Week 5-8)

⚠️ **IMPORTANT**: This phase is currently BLOCKED on Tebako repository access and architecture decisions.

#### 3.1 Execution Shim Implementation
**Priority**: P0 (Blocker for Tebako merger)
**Effort**: 2-3 days
**Files to Create**:
- Create: `tebako-shim/tebako_main.c` (50-200 lines)
- Create: `tebako-shim/CMakeLists.txt`
- Create: `tebako-shim/README.md`

**Shim Responsibilities**:
```c
// tebako_main.c - Execution shim

// 1. Image Discovery
//    - Scan executable for embedded archive marker
//    - Validate archive integrity (checksum)
//    - Get archive offset and size

// 2. Filesystem Initialization
//    - Call tebako_fs_init() with embedded data
//    - Mount at "/__tebako__" mount point
//    - Handle initialization errors

// 3. --tebako-extract Support
//    - Check for --tebako-extract flag
//    - Call tebako_fs_extract_all()
//    - Exit after extraction

// 4. Ruby Bootstrapping
//    - Set up Ruby environment
//    - Patch $LOAD_PATH to include /__tebako__
//    - Execute main Ruby script
//    - Clean up on exit

int main(int argc, char** argv) {
    // Implementation here
}
```

**Image Structure**:
```
[Original Executable]
[Padding to align]
[Archive Data (ZIP/SquashFS/DwarFS)]
[Image Metadata]
  - Magic: "TEBAKO\x00\x01"
  - Format: uint8_t (1=ZIP, 2=SquashFS, 3=DwarFS)
  - Offset: uint64_t (from start of executable)
  - Size: uint64_t (archive size in bytes)
  - Checksum: uint32_t (CRC32 or similar)
```

**Acceptance Criteria**:
- [ ] Shim implementation complete (50-200 lines)
- [ ] Image discovery working
- [ ] --tebako-extract functional
- [ ] Error handling robust
- [ ] Tests: 10+ covering all scenarios

---

#### 3.2 Image Packaging Tool
**Priority**: P0 (Blocker for Tebako usage)
**Effort**: 2 days
**Files to Create**:
- Create: `tools/tebako-pack/main.cpp`
- Create: `tools/tebako-pack/CMakeLists.txt`
- Create: `tools/tebako-pack/README.md`

**Tool Functionality**:
```bash
# tebako-pack - Archive packaging tool

# Usage:
tebako-pack <executable> <archive> [--format zip|sqfs|dwarfs]

# Steps:
1. Validate inputs (executable exists, archive exists)
2. Determine format (from flag or archive extension)
3. Read archive data into memory
4. Calculate metadata (size, checksum)
5. Append archive to executable
6. Write metadata footer
7. Make result executable (chmod +x)

# Example:
tebako-pack ruby-shim app.zip -o myapp
# Creates 'myapp' with embedded app.zip
```

**Acceptance Criteria**:
- [ ] Tool implementation complete
- [ ] Supports all 3 formats (ZIP, SquashFS, DwarFS)
- [ ] Validates input archives
- [ ] Checksums working correctly
- [ ] Tests: 15+ covering all scenarios
- [ ] Documentation complete

---

#### 3.3 Ruby Runtime Integration
**Priority**: P0 (Blocker for actual usage)
**Effort**: 5-7 days
**Files to Modify** (in Tebako repository):
- Modify: `ruby-patches/file_io.patch`
- Modify: `ruby-patches/dir_io.patch`
- Modify: `ruby-patches/load_path.patch`

⚠️ **BLOCKER**: Requires access to Tebako repository and Ruby patching infrastructure.

**Patch Responsibilities**:

1. **File I/O Hooks** (`file_io.patch`):
```c
// Patch rb_file_open() and related functions
int rb_io_open_wrapped(const char* path, int flags, mode_t mode) {
    // 1. Check if path is embedded
    if (tebako_path_is_embedded(path)) {
        // Use libtfs C API
        return tebako_open(path, flags);
    }
    // 2. Fallback to original implementation
    return original_rb_io_open(path, flags, mode);
}

// Similar for: read, write, seek, close, stat, etc.
```

2. **Directory I/O Hooks** (`dir_io.patch`):
```c
// Patch rb_dir_open() and related functions
DIR* rb_dir_open_wrapped(const char* path) {
    if (tebako_path_is_embedded(path)) {
        return tebako_opendir(path);
    }
    return original_rb_dir_open(path);
}

// Similar for: readdir, closedir, etc.
```

3. **$LOAD_PATH Integration** (`load_path.patch`):
```ruby
# Patch Ruby load path initialization
$LOAD_PATH.unshift("/__tebako__/lib")
$LOAD_PATH.unshift("/__tebako__/vendor/bundle")
# etc.
```

**Testing Requirements**:
- Create: `tebako-tests/ruby_integration_test.rb`
- Test cases:
  * `File.read("/__tebako__/file.txt")`
  * `Dir.entries("/__tebako__/dir")`
  * `require` from embedded filesystem
  * `load` from embedded filesystem
  * `File.stat` returns correct metadata
  * Concurrent file operations

**Acceptance Criteria**:
- [ ] All Ruby file I/O redirected correctly
- [ ] All Ruby directory I/O redirected correctly
- [ ] $LOAD_PATH includes embedded paths
- [ ] Existing Ruby code works unchanged
- [ ] Tests: 30+ covering all scenarios
- [ ] No performance regression (< 5% overhead)

---

### Phase 4: Quality Assurance (Week 9-10)

#### 4.1 Memory Safety Validation
**Priority**: P0 (Blocker for production)
**Effort**: 2 days

**Tools to Run**:
1. **Valgrind** (memory leaks):
```bash
valgrind --leak-check=full --show-leak-kinds=all \
    ./test_backend_factory
# Must show: "All heap blocks were freed -- no leaks are possible"
```

2. **AddressSanitizer** (memory errors):
```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
./test_backend_factory
# Must show: No errors
```

3. **ThreadSanitizer** (race conditions):
```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ..
./test_backend_factory
# Must show: No data races
```

4. **UndefinedBehaviorSanitizer**:
```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=undefined" ..
./test_backend_factory
# Must show: No undefined behavior
```

**Acceptance Criteria**:
- [ ] Valgrind: Zero memory leaks in all tests
- [ ] ASAN: Zero errors in all tests
- [ ] TSAN: Zero data races in all tests
- [ ] UBSAN: Zero undefined behavior
- [ ] All sanitizers run on all test suites

---

#### 4.2 Stress Testing
**Priority**: P1 (Important for production)
**Effort**: 2 days

**Test Scenarios**:

1. **Large Archives**:
```cpp
// Test with 1GB+ archives
// Verify: No memory exhaustion, reasonable performance
```

2. **Many Files**:
```cpp
// Create archive with 100,000+ small files
// Verify: Directory listing works, no crashes
```

3. **Deep Directory Trees**:
```cpp
// Create directory tree 100+ levels deep
// Verify: Path traversal works
```

4. **Concurrent Access**:
```cpp
// 100+ threads reading simultaneously
// Verify: No deadlocks, correct data
```

5. **Sustained Operations**:
```cpp
// Run for 24+ hours under load
// Verify: No memory leaks, no crashes
```

**Acceptance Criteria**:
- [ ] All stress tests pass
- [ ] No memory growth over time
- [ ] No crashes or hangs
- [ ] Performance remains acceptable

---

#### 4.3 C API Testing Completeness
**Priority**: P0 (Blocker for Ruby integration)
**Effort**: 1 day

**Review Current Tests** ([`test/test_c_api.cpp`](../test/test_c_api.cpp)):
- Current: 60 tests (100% passing)
- Target: 80+ tests for comprehensive coverage

**Additional Tests Needed**:
```cpp
// Add tests for:
- Edge cases in tebako_fs_extract_all()
- Error conditions in all functions
- Thread safety of C API layer
- FD collision scenarios
- Memory mounting stress tests
- Extraction with large archives
- Extraction with permission preservation
```

**Acceptance Criteria**:
- [ ] 80+ tests in test_c_api.cpp
- [ ] 100% pass rate
- [ ] All error paths tested
- [ ] Thread safety validated

---

### Phase 5: Documentation & Release (Week 11-12)

#### 5.1 API Reference Documentation
**Priority**: P0 (Blocker for release)
**Effort**: 2 days

**Generate Doxygen Documentation**:
```bash
# Create Doxyfile
doxygen -g

# Configure for:
- INPUT = include/tebako/fs
- RECURSIVE = YES
- GENERATE_HTML = YES
- GENERATE_LATEX = NO

# Generate:
doxygen Doxyfile

# Result: HTML documentation in docs/api/
```

**Acceptance Criteria**:
- [ ] All public APIs documented in headers
- [ ] Doxygen builds without warnings
- [ ] HTML documentation generated
- [ ] Examples included for all major functions

---

#### 5.2 Migration Guide
**Priority**: P0 (Blocker for Tebako integration)
**Effort**: 1 day
**File to Create**:
- Create: [`docs/MIGRATION_GUIDE.md`](../docs/MIGRATION_GUIDE.md)

**Content Structure**:
```markdown
# Migration Guide: Integrating libtfs into Tebako

## Overview
- What is libtfs
- Why migrate from libdwarfs-wr
- Benefits of multi-backend architecture

## Pre-Migration Checklist
- [ ] Review current Tebako architecture
- [ ] Identify Ruby I/O integration points
- [ ] Plan for archive format selection

## Step-by-Step Migration

### 1. Dependency Updates
[Instructions for updating build system]

### 2. Code Changes
[Required changes to Tebako codebase]

### 3. Testing Strategy
[How to validate migration]

### 4. Rollback Plan
[If migration encounters issues]

## Archive Format Selection

### When to Use ZIP
- Universal compatibility needed
- Simple archives (<100MB)

### When to Use SquashFS
- Linux-only deployment
- Better compression than ZIP
- POSIX permissions important

### When to Use DwarFS
- Maximum compression needed
- Large archives (>100MB)
- Performance critical

## Troubleshooting
[Common issues and solutions]
```

**Acceptance Criteria**:
- [ ] Guide complete and tested
- [ ] All code examples working
- [ ] Reviewed by Tebako maintainers
- [ ] Clear rollback instructions

---

#### 5.3 Performance Guide
**Priority**: P1 (Important for users)
**Effort**: 1 day
**File to Create**:
- Create: [`docs/PERFORMANCE_GUIDE.md`](../docs/PERFORMANCE_GUIDE.md)

**Content**:
```markdown
# Performance Guide

## Benchmark Results
[Tables with actual benchmark data]

## Backend Selection Guide
[Decision matrix for choosing backends]

## Optimization Tips
- Archive creation best practices
- Memory mounting vs file mounting
- Threading considerations

## Performance Characteristics
[Detailed analysis of each backend]
```

**Acceptance Criteria**:
- [ ] All benchmarks included
- [ ] Decision matrix clear
- [ ] Optimization tips actionable
- [ ] Based on real measurements

---

## Timeline Summary

| Phase | Duration | Effort | Dependencies |
|-------|----------|--------|--------------|
| Phase 1: Core Functionality | 2 weeks | 6 days | None |
| Phase 2: Testing Infrastructure | 2 weeks | 9 days | Phase 1 |
| Phase 3: Tebako Integration | 4 weeks | 12 days | Phase 2, Tebako access |
| Phase 4: Quality Assurance | 2 weeks | 5 days | Phase 3 |
| Phase 5: Documentation | 2 weeks | 4 days | Phase 4 |
| **Total** | **12 weeks** | **36 days** | - |

**Realistic Timeline**: 10-12 weeks (2.5-3 months)
**Optimistic Timeline**: 8 weeks (2 months) if Tebako integration is deferred
**Pessimistic Timeline**: 14-16 weeks (3.5-4 months) if issues encountered

---

## Current Blockers

### Immediate Blockers
1. **Tebako Repository Access**: Need access to implement Ruby integration
2. **Archive Format Decision**: Need decision on default format for Tebako
3. **Ruby Patching Infrastructure**: Need understanding of current Ruby patches in Tebako

### Near-term Blockers
4. **Benchmark Fixtures**: Need to create perl-5.42.0 archives
5. **CI/CD Updates**: Need to update workflows for new tests

### Long-term Considerations
6. **Windows Support**: DwarFS has limited Windows support
7. **macOS SquashFS**: squashfs-tools-ng not available on macOS

---

## Success Criteria

### Minimum Viable Product (v0.12.0)
- [x] C API implemented
- [x] DwarFS backend implemented
- [x] Memory mounting working
- [ ] Extraction API implemented **[CRITICAL]**
- [ ] DwarFS documentation complete **[CRITICAL]**
- [ ] Basic testing complete (140+ tests passing)

### Production Ready (v0.12.0)
- [ ] All P0 items complete (extraction, docs, tests)
- [ ] Performance benchmarks established
- [ ] Cross-platform validation complete
- [ ] Memory safety verified (Valgrind clean)
- [ ] API documentation generated
- [ ] Migration guide complete

### Tebako Integration Ready (v0.13.0?)
- [ ] Execution shim implemented
- [ ] Packaging tool created
- [ ] Ruby I/O hooks implemented
- [ ] End-to-end integration tested
- [ ] Production deployment validated

---

## Recommended Next Steps

### Week 1-2: Focus on Missing Core Items

1. **Start with Extraction API** (2-3 days)
   - Implement `tebako_fs_extract_all()`
   - Write comprehensive tests
   - Validate with large archives

2. **Create DwarFS Documentation** (1 day)
   - Follow ZIP_BACKEND.adoc pattern
   - Include performance data
   - Add usage examples

3. **Complete DwarFS Testing** (2 days)
   - Implement test_dwarfs_backend.cpp (47 tests)
   - Implement test_dwarfs_integration.cpp (13 tests)
   - Achieve 100% pass rate

### Week 3-4: Validation & Benchmarking

4. **Implement Benchmark Suite** (3 days)
   - Create benchmark fixtures
   - Run all benchmarks
   - Generate reports

5. **Cross-Platform Testing** (4 days)
   - Test on all platforms
   - Fix any platform-specific issues
   - Update CI

### Week 5-8: Tebako Integration
⚠️ **Requires Tebako repository access**

6. **Implement Shim & Packaging** (4 days)
7. **Ruby Integration** (7 days)
8. **End-to-End Testing** (3 days)

### Week 9-10: Quality Assurance

9. **Memory Safety** (2 days)
10. **Stress Testing** (2 days)
11. **API Testing Expansion** (1 day)

### Week 11-12: Documentation & Release

12. **API Reference** (2 days)
13. **Migration Guide** (1 day)
14. **Performance Guide** (1 day)
15. **Release Preparation** (2 days)

---

## Contact & Questions

For questions about this roadmap:
- Open issue on libtfs repository
- Tag as `roadmap` or `planning`
- Reference this document

**Last Updated**: 2025-12-30
**Next Review**: After Phase 1 completion
**Maintained By**: libtfs Development Team