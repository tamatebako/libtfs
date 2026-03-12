
# Stage 2: Multi-Backend VFS Implementation - Continuation Plan

**Date**: 2025-12-22
**Status**: Architecture Complete, Ready for Implementation
**Target**: Complete ZIP + SquashFS backends with pure vcpkg dependency management
**Timeline**: 7-8 days (compressed from 12.5 days)

---

## Current Status Summary

### ✅ Completed
- [x] FileSystem interface design ([`filesystem.h`](../include/tebako/fs/filesystem.h))
- [x] FileHandle interface ([`file_handle.h`](../include/tebako/fs/file_handle.h))
- [x] DirectoryIterator interface ([`directory_iterator.h`](../include/tebako/fs/directory_iterator.h))
- [x] Architecture research and design
- [x] DwarFS magic byte format analysis (corrected: "DWARFS" at offset 0)
- [x] vcpkg overlay strategy for squashfs-tools-ng

### 🚧 In Progress
- [ ] BackendFactory implementation
- [ ] Format detection with correct magic bytes

### 📋 Pending
- [ ] vcpkg overlay port for squashfs-tools-ng
- [ ] ZIP backend (libzip via vcpkg)
- [ ] SquashFS backend
- [ ] Comprehensive test suite
- [ ] Documentation updates

---

## Critical Architecture Decisions

### Format Detection (CORRECTED)

Based on source code analysis of `/Users/mulgogi/src/external/dwarfs`:

```cpp
// DwarFS: "DWARFS" (6 ASCII bytes) at offset 0
// From src/writer/filesystem_writer.cpp:520
constexpr uint8_t DWARFS_MAGIC[] = {'D', 'W', 'A', 'R', 'F', 'S'};

// ZIP: PK signature at offset 0
constexpr uint8_t ZIP_LOCAL_MAGIC[] = {0x50, 0x4B, 0x03, 0x04};
constexpr uint8_t ZIP_CENTRAL_MAGIC[] = {0x50, 0x4B, 0x05, 0x06};

// SquashFS: "hsqs" (LE) or "sqsh" (BE) at offset 0
constexpr uint8_t SQUASHFS_MAGIC_LE[] = {0x68, 0x73, 0x71, 0x73};
constexpr uint8_t SQUASHFS_MAGIC_BE[] = {0x73, 0x71, 0x73, 0x68};
```

**Detection Priority**:
1. Magic numbers (most reliable)
2. File extensions (fallback)
3. Never proceed without positive identification

### Dependency Strategy: Pure vcpkg

**Rationale**: Unified dependency management, binary caching, cross-platform consistency

**Dependencies**:
- `libzip` (official vcpkg port, v1.11.4)
- `squashfs-tools-ng` (custom overlay port, v1.3.2)

---

## Compressed Implementation Timeline

### Week 1: Foundation & ZIP (Days 1-4)

#### Day 1: Setup & BackendFactory
**AM Session (2-3 hours)**
- [ ] Create vcpkg overlay structure: `vcpkg-overlay/squashfs-tools-ng/`
  - [ ] `portfile.cmake`
  - [ ] `vcpkg.json`
  - [ ] `usage` file
- [ ] Update root `vcpkg.json` with new dependencies

**PM Session (2-3 hours)**
- [ ] Implement [`backend_factory.h`](../include/tebako/fs/backend_factory.h)
- [ ] Implement [`backend_factory.cpp`](../src/backend_factory.cpp)
- [ ] Create [`test_backend_factory.cpp`](../tests/test_backend_factory.cpp)
- [ ] Update [`CMakeLists.txt`](../CMakeLists.txt)

**Success Criteria**:
- ✅ vcpkg can install both libzip and squashfs-tools-ng
- ✅ BackendFactory compiles and all tests pass
- ✅ Format detection correctly identifies test files

#### Day 2: ZIP Backend Core
**Full Day (6-8 hours)**
- [ ] Create [`include/tebako/fs/backends/zip_backend.h`](../include/tebako/fs/backends/zip_backend.h)
- [ ] Implement [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp)
  - [ ] `ZipFileHandle` class (file reading with libzip)
  - [ ] `ZipDirectoryIterator` class (directory traversal)
  - [ ] `ZipBackend` class (main implementation)
    - [ ] `mount()` / `unmount()`
    - [ ] `open()` / `exists()` / `is_file()` / `is_directory()`
    - [ ] `list_directory()`
    - [ ] `file_size()` / `modification_time()` / `permissions()`

**Success Criteria**:
- ✅ ZIP backend compiles without errors
- ✅ Can mount/unmount ZIP archives
- ✅ Basic file operations work

#### Day 3: ZIP Backend Testing
**Full Day (6-8 hours)**
- [ ] Create test ZIP archives in `tests/test_files/`
- [ ] Implement [`tests/test_zip_backend.cpp`](../tests/test_zip_backend.cpp)
  - [ ] Mount/unmount tests
  - [ ] File existence tests
  - [ ] File reading tests (full & partial)
  - [ ] Directory listing tests
  - [ ] Metadata query tests
  - [ ] Error handling tests
  - [ ] Thread safety tests
- [ ] Update CMakeLists.txt for ZIP tests
- [ ] Fix any bugs discovered during testing

**Success Criteria**:
- ✅ All ZIP backend tests pass
- ✅ No memory leaks (valgrind clean)
- ✅ Thread-safe concurrent access verified

#### Day 4: ZIP Integration
**AM Session (2-3 hours)**
- [ ] Update BackendFactory to use ZipBackend
- [ ] Integration tests with mount table
- [ ] Multi-backend tests (DwarFS + ZIP simultaneously)

**PM Session (2-3 hours)**
- [ ] Performance profiling
- [ ] Optimization if needed
- [ ] Documentation for ZIP backend

**Success Criteria**:
- ✅ ZIP backend fully integrated
- ✅ Mixed DwarFS/ZIP mounts work correctly
- ✅ Performance acceptable (< 10% overhead vs raw libzip)

### Week 2: SquashFS & Completion (Days 5-8)

#### Day 5: SquashFS Backend Core
**Full Day (6-8 hours)**
- [ ] Verify squashfs-tools-ng installation
- [ ] Create [`include/tebako/fs/backends/squashfs_backend.h`](../include/tebako/fs/backends/squashfs_backend.h)
- [ ] Implement [`src/backends/squashfs_backend.cpp`](../src/backends/squashfs_backend.cpp)
  - [ ] `SquashFSFileHandle` class
  - [ ] `SquashFSDirectoryIterator` class
  - [ ] `SquashFSBackend` class (similar structure to ZipBackend)

**Success Criteria**:
- ✅ SquashFS backend compiles
- ✅ Can mount/unmount SquashFS images
- ✅ Basic operations work

#### Day 6: SquashFS Testing
**Full Day (6-8 hours)**
- [ ] Create test SquashFS images
- [ ] Implement [`tests/test_squashfs_backend.cpp`](../tests/test_squashfs_backend.cpp)
- [ ] Comprehensive test coverage (parallel to ZIP tests)
- [ ] Bug fixes

**Success Criteria**:
- ✅ All SquashFS backend tests pass
- ✅ Memory leak free
- ✅ Thread-safe

#### Day 7: Integration & Multi-Backend Testing
**AM Session (3-4 hours)**
- [ ] Update BackendFactory for SquashFS
- [ ] Create [`tests/test_multi_backend.cpp`](../tests/test_multi_backend.cpp)
  - [ ] Simultaneous mounts (DwarFS + ZIP + SquashFS)
  - [ ] Path resolution across backends
  - [ ] Priority and fallback handling
  - [ ] Stress tests

**PM Session (3-4 hours)**
- [ ] Performance benchmarking
- [ ] Memory usage analysis
- [ ] Optimization pass

**Success Criteria**:
- ✅ All three backends work together seamlessly
- ✅ No resource contention
- ✅ Performance targets met

#### Day 8: Documentation & Finalization
**AM Session (2-3 hours)**
- [ ] Update [README.adoc](../README.adoc) with Stage 2 features
- [ ] Create backend developer guide
- [ ] Update build instructions
- [ ] API documentation polish

**PM Session (2-3 hours)**
- [ ] Final verification builds on all platforms
- [ ] Archive temporary/outdated docs to `docs/archive/`
- [ ] Create release notes
- [ ] Final code review

**Success Criteria**:
- ✅ All documentation complete and accurate
- ✅ Builds successfully on Linux, macOS, Windows
- ✅ All tests passing (100% pass rate)
- ✅ Ready for production use

---

## Detailed File Structure

### New Files to Create

```
libdwarfs/
├── vcpkg-overlay/
│   └── squashfs-tools-ng/
│       ├── portfile.cmake           # vcpkg build script
│       ├── vcpkg.json              # Port manifest
│       └── usage                   # Usage instructions
├── include/tebako/fs/
│   ├── backend_factory.h           # NEW: Static factory
│   └── backends/
│       ├── zip_backend.h           # NEW: ZIP implementation
│       └── squashfs_backend.h      # NEW: SquashFS implementation
├── src/
│   ├── backend_factory.cpp         # NEW: Factory implementation
│   └── backends/
│       ├── zip_backend.cpp         # NEW: ZIP backend
│       └── squashfs_backend.cpp    # NEW: SquashFS backend
└── tests/
    ├── test_backend_factory.cpp    # NEW: Factory tests
    ├── test_zip_backend.cpp        # NEW: ZIP tests
    ├── test_squashfs_backend.cpp   # NEW: SquashFS tests
    ├── test_multi_backend.cpp      # NEW: Integration tests
    └── test_files/
        ├── test.zip                # NEW: Test archive
        ├── test.sqfs               # NEW: Test archive
        └── test_filesystem/        # Existing test data
```

### Files to Modify

```
libdwarfs/
├── vcpkg.json                      # Add libzip, squashfs-tools-ng
├── CMakeLists.txt                  # Add new sources, link libraries
├── README.adoc                     # Document new features
└── docs/
    ├── ARCHITECTURE.md             # Update with backend system
    └── archive/                    # Move outdated docs here
```

---

## Risk Mitigation

### Risk 1: vcpkg Overlay Port Complexity
**Symptom**: squashfs-tools-ng fails to build via vcpkg
**Mitigation**:
- Start with minimal working port
- Test build isolation
- Use ExternalProject_Add as fallback if vcpkg proves problematic
**Fallback**: Revert to hybrid approach (vcpkg for libzip, ExternalProject for squashfs-tools-ng)

### Risk 2: libzip Static Linking Issues
**Symptom**: Link errors with libzip
**Mitigation**:
- Use vcpkg's static triplets (`x64-linux-static`, `x64-osx-static`)
- Verify with `ldd` or `otool -L` that no dynamic dependencies exist
- Check vcpkg documentation for platform-specific flags

### Risk 3: squashfs-tools-ng API Complexity
**Symptom**: Difficulty understanding squashfs-tools-ng API
**Mitigation**:
- Study examples in squashfs-tools-ng source
- Start with read-only operations
- Implement incrementally: mount → read → directory listing → metadata

### Risk 4: Performance Degradation
**Symptom**: Significant overhead from abstraction
**Mitigation**:
- Profile early and often
- Use `perf`, `Instruments`, or `Visual Studio Profiler`
- Optimize hot paths
- Consider caching strategies if needed

### Risk 5: Cross-Platform Build Failures
**Symptom**: Works on one platform, fails on others
**Mitigation**:
- Test frequently on all target platforms
- Use CI/CD for automated testing
- Platform-specific code should be minimal and well-documented

---

## Implementation Tracking

### Daily Progress Checklist

#### Day 1 Checklist
- [ ] vcpkg overlay created and tested
- [ ] vcpkg.json updated
- [ ] BackendFactory header complete
- [ ] BackendFactory implementation complete
- [ ] BackendFactory tests written and passing
- [ ] CMakeLists.txt updated
- [ ] Build successful

#### Day 2 Checklist
- [ ] ZipBackend header complete
- [ ] ZipFileHandle implemented
- [ ] ZipDirectoryIterator implemented
- [ ] ZipBackend core methods implemented
- [ ] Compiles without errors
- [ ] Can mount/unmount test ZIP

#### Day 3 Checklist
- [ ] Test ZIP archives created
- [ ] All test cases written
- [ ] All tests passing
- [ ] Memory leak free (valgrind)
- [ ] Thread safety verified
- [ ] Bug fixes completed

#### Day 4 Checklist
- [ ] BackendFactory produces ZipBackend
- [ ] Integration with mount table working
- [ ] Multi-backend tests passing
- [ ] Performance acceptable
- [ ] ZIP documentation complete

#### Day 5 Checklist
- [ ] SquashFSBackend header complete
- [ ] SquashFSFileHandle implemented
- [ ] SquashFSDirectoryIterator implemented
- [ ] SquashFSBackend core methods implemented
- [ ] Compiles without errors
- [ ] Can mount/unmount test SquashFS

#### Day 6 Checklist
- [ ] Test SquashFS images created
- [ ] All test cases written
- [ ] All tests passing
- [ ] Memory leak free
- [ ] Thread safety verified
- [ ] Bug fixes completed

#### Day 7 Checklist
- [ ] All three backends work together
- [ ] Multi-backend tests comprehensive
- [ ] Performance benchmarked
- [ ] Optimization completed if needed
- [ ] Code review completed

#### Day 8 Checklist
- [ ] README.adoc updated
- [ ] API documentation complete
- [ ] Build instructions updated
- [ ] Outdated docs archived
- [ ] Final verification builds pass
- [ ] Release notes created

---

## Success Metrics

### Code Quality Metrics
- **Test Coverage**: ≥95% for new code
- **Static Analysis**: Zero warnings with `-Wall -Wextra -Werror`
- **Memory Safety**: Zero leaks detected by valgrind
- **Thread Safety**: Pass ThreadSanitizer checks

### Performance Metrics
- **Backend Creation**: < 1ms overhead vs direct backend instantiation
- **Format Detection**: < 5ms for magic number detection
- **File Operations**: < 10% overhead vs raw library calls
- **Memory Usage**: < 50MB per mounted backend (excluding archive data)

### Functional Metrics
- **Format Support**: DwarFS, ZIP, SquashFS all working
- **Auto-Detection**: 100% accuracy on well-formed archives
- **Concurrent Access**: Support ≥100 concurrent threads
- **Error Handling**: Graceful failure with clear error messages

---

## Documentation Requirements

### README.adoc Updates Required

1. **Features Section**: Add Stage 2 backends
2. **Installation Section**: Update vcpkg instructions
3. **Usage Examples**: Show multi-backend scenarios
4. **Architecture Section**: Describe backend system
5. **API Reference**: Document BackendFactory

### New Documentation Needed

1. **Backend Developer Guide**: How to add new backends
2. **Format Detection Guide**: How magic number detection works
3. **Testing Guide**: How to test backends
4. **Troubleshooting Guide**: Common issues and solutions

### Documentation to Archive

Move to `docs/archive/`:
- `STAGE_2_IMPLEMENTATION.md` (superseded by this plan)
- `STAGE_2_WEEK1_DAY2_PROMPT.md` (implementation phase complete)
- Any temporary planning documents
- Outdated design documents

---

## Build Verification Commands

### Configure
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=${PWD}/vcpkg-overlay \
  -DWITH_TESTS=ON
```

### Build
```bash
cmake --build build -j$(nproc)
```

### Test
```bash
cd build
ctest --verbose --output-on-failure
```

### Verify Static Linking
```bash
# Linux
ldd build/libtfs.a  # Should say "not a dynamic executable"
nm build/libtfs.a | grep -i "zip\|sqfs"  # Should show symbols

# macOS
otool -L build/libtfs.a  # Should list only system libraries
nm build/libtfs.a | grep -i "zip\|sqfs"

# Windows
dumpbin /dependents build\libtfs.lib
```

### Memory Check
```bash
valgrind --leak-check=full --show-leak-kinds=all \
  --track-origins=yes --verbose \
  build/tests/test_backend_factory
```

---

## Next Actions

1. **Switch to Code Mode**: Begin implementation with Day 1 tasks
2. **Create Task Tracking**: Use GitHub issues or project board
3. **Set Up CI/CD**: Automated testing on each commit
4. **Schedule Reviews**: Daily standup for progress tracking

---

**Plan Version**: 1.0
**Last Updated**: 2025-12-22
**Estimated Completion**: 2025-12-30 (8 days)
**Status**: Ready for Implementation