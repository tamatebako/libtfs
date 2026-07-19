# Stage 2: Quick Start Implementation Guide

**For**: Developers beginning Stage 2 (ZIP Backend) implementation
**Prerequisites**: Stage 1 complete, all tests passing
**Estimated Time**: 3 weeks

---

## 📋 Pre-Implementation Checklist

Before starting, verify:

```bash
# Ensure you're on the right branch
git checkout stage-1-modernize-libtfs
git pull origin stage-1-modernize-libtfs

# Verify Stage 1 complete
cd build
make clean
cmake -DTEBAKO_BUILD=ON -DDWARFS_WITH_THRIFT=OFF \
      -DDWARFS_WITH_FLATBUFFERS=ON -DWITH_TESTS=ON ..
make -j$(nproc)
ctest --output-on-failure

# All tests should pass ✅
```

- [ ] All Stage 1 tests passing
- [ ] Documentation reviewed ([`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md))
- [ ] Clean git status
- [ ] External dwarfs at `/Users/mulgogi/src/external/dwarfs`

---

## 🚀 Implementation Phases

### Phase 1: VFS Interface (Week 1, Days 1-3)

#### Day 1: Create Base Interfaces

**Morning: Core Interfaces**

1. Create [`include/tebako/fs/filesystem.h`](include/tebako/fs/filesystem.h)
   - Abstract `FileSystem` base class
   - Pure virtual methods for mount/unmount/operations
   - Thread-safety requirements documented

2. Create [`include/tebako/fs/file_handle.h`](include/tebako/fs/file_handle.h)
   - Abstract `FileHandle` base class
   - Read/seek/tell interface
   - POSIX-like API

3. Create [`include/tebako/fs/directory_iterator.h`](include/tebako/fs/directory_iterator.h)
   - Abstract `DirectoryIterator` base class
   - `DirectoryEntry` struct
   - Iterator pattern

**Afternoon: Implementation Stubs**

```bash
# Create stub implementations for testing
touch src/filesystem.cpp
touch src/file_handle.cpp
touch src/directory_iterator.cpp
```

**Validation**:
```bash
cd build && make -j$(nproc)
# Should compile with warnings about pure virtual
```

#### Day 2: Backend Registry

**Morning: Registry Implementation**

1. Create [`include/tebako/fs/backend_registry.h`](include/tebako/fs/backend_registry.h)
   - Singleton pattern
   - Thread-safe registration
   - Format detection types

2. Create [`src/backend_registry.cpp`](src/backend_registry.cpp)
   - Implement `register_backend()`
   - Implement `create_backend()`
   - Implement `detect_and_create()`

**Afternoon: Tests**

3. Create [`tests/test_backend_registry.cpp`](tests/test_backend_registry.cpp)
   ```cpp
   TEST(BackendRegistry, RegisterAndCreate) {
       auto& registry = BackendRegistry::instance();

       // Register mock backend
       BackendInfo info;
       info.name = "Mock";
       info.factory = []() { return std::make_unique<MockBackend>(); };

       registry.register_backend(info);
       auto backend = registry.create_backend("Mock");
       ASSERT_NE(backend, nullptr);
   }
   ```

**Validation**:
```bash
cd build && make -j$(nproc) && ctest -R test_backend_registry
```

#### Day 3: Refactor DwarfsBackend

**Morning: Create DwarfsBackend Class**

1. Create [`include/tebako/fs/backends/dwarfs_backend.h`](include/tebako/fs/backends/dwarfs_backend.h)
   - Inherit from `FileSystem`
   - Declare all pure virtual overrides
   - Private `Impl` pattern (PIMPL)

2. Create [`src/backends/dwarfs_backend.cpp`](src/backends/dwarfs_backend.cpp)
   - Implement `DwarfsBackend::Impl` wrapping existing code
   - Implement `FileSystem` interface methods
   - Use existing dwarfs code internally

**Afternoon: DwarFS File/Directory Classes**

3. Create [`src/backends/dwarfs_file_handle.cpp`](src/backends/dwarfs_file_handle.cpp)
   - Implement `FileHandle` for DwarFS

4. Create [`src/backends/dwarfs_directory_iterator.cpp`](src/backends/dwarfs_directory_iterator.cpp)
   - Implement `DirectoryIterator` for DwarFS

**Validation**:
```bash
cd build && make -j$(nproc)
# DwarFS should still work through new interface
ctest -R dwarfs
```

---

### Phase 2: libzip Integration (Week 1, Days 4-5)

#### Day 4: Add libzip Dependency

**Morning: CMake Configuration**

1. Update [`CMakeLists.txt`](CMakeLists.txt)
   ```cmake
   # Around line 50, add:
   find_package(LibZip REQUIRED)

   # Around line 600, add to target_link_libraries:
   target_link_libraries(${PROJECT_NAME}
       PRIVATE
       LibZip::LibZip
       # ... existing dependencies ...
   )
   ```

2. Create [`cmake/FindLibZip.cmake`](cmake/FindLibZip.cmake) if needed
   ```cmake
   find_path(LibZip_INCLUDE_DIR zip.h)
   find_library(LibZip_LIBRARY NAMES zip)

   include(FindPackageHandleStandardArgs)
   find_package_handle_standard_args(LibZip
       REQUIRED_VARS LibZip_LIBRARY LibZip_INCLUDE_DIR)

   if(LibZip_FOUND AND NOT TARGET LibZip::LibZip)
       add_library(LibZip::LibZip UNKNOWN IMPORTED)
       set_target_properties(LibZip::LibZip PROPERTIES
           IMPORTED_LOCATION "${LibZip_LIBRARY}"
           INTERFACE_INCLUDE_DIRECTORIES "${LibZip_INCLUDE_DIR}")
   endif()
   ```

**Afternoon: Test libzip**

3. Create simple test program
   ```cpp
   #include <zip.h>
   #include <iostream>

   int main() {
       std::cout << "libzip version: " << ZIP_VERSION << std::endl;
       return 0;
   }
   ```

**Validation**:
```bash
cd build && cmake .. && make -j$(nproc)
# Should find libzip and link successfully
```

#### Day 5: ZIP Format Detection

**Morning: Detection Logic**

1. Create [`src/backends/zip_detection.cpp`](src/backends/zip_detection.cpp)
   ```cpp
   bool ZipBackend::can_handle(const std::string& path) {
       // Check magic number
       std::ifstream file(path, std::ios::binary);
       char magic[4];
       file.read(magic, 4);

       // ZIP local file header: PK\x03\x04
       if (magic[0] == 'P' && magic[1] == 'K' &&
           magic[2] == 0x03 && magic[3] == 0x04) {
           return true;
       }

       // ZIP central directory: PK\x01\x02
       if (magic[0] == 'P' && magic[1] == 'K' &&
           magic[2] == 0x01 && magic[3] == 0x02) {
           return true;
       }

       return false;
   }
   ```

**Afternoon: Detection Tests**

2. Create test fixtures in [`tests/fixtures/`](tests/fixtures/)
   ```bash
   # Create small test ZIP
   echo "test content" > test.txt
   zip tests/fixtures/test.zip test.txt
   rm test.txt
   ```

3. Create [`tests/test_zip_detection.cpp`](tests/test_zip_detection.cpp)

**Validation**:
```bash
cd build && make -j$(nproc)
ctest -R test_zip_detection
```

---

### Phase 3: ZipBackend Implementation (Week 2, Days 1-4)

#### Day 1: Mount/Unmount

1. Create [`include/tebako/fs/backends/zip_backend.h`](include/tebako/fs/backends/zip_backend.h)
2. Create [`src/backends/zip_backend.cpp`](src/backends/zip_backend.cpp)
   - Implement `mount()` using `zip_open()`
   - Build internal file index
   - Implement `unmount()` using `zip_close()`

#### Day 2: File Operations

1. Create [`src/backends/zip_file_handle.cpp`](src/backends/zip_file_handle.cpp)
   - Implement `read()` using `zip_fread()`
   - Implement `seek()` (buffered, as ZIP doesn't support true seeking)
   - Implement `tell()` and `eof()`

#### Day 3: Directory Operations

1. Create [`src/backends/zip_directory_iterator.cpp`](src/backends/zip_directory_iterator.cpp)
   - Implement `has_next()` and `next()`
   - Handle directory path filtering
   - Cache entries for performance

#### Day 4: Metadata Operations

1. Implement in [`zip_backend.cpp`](src/backends/zip_backend.cpp)
   - `file_size()` from ZIP stat
   - `modification_time()` from ZIP stat
   - `permissions()` approximated from external attributes

**Tests**: Create comprehensive tests for each component

---

### Phase 4: Integration (Week 2, Day 5)

#### Mount Table Updates

1. Update [`include/tebako/fs/internal/mount_table.h`](include/tebako/fs/internal/mount_table.h)
   - Add `add_backend()` method
   - Support backend-specific mounts

2. Update [`src/tebako-mount-table.cpp`](src/tebako-mount-table.cpp)
   - Store backends alongside mount info
   - Route operations to correct backend

**Tests**: Multi-backend mounting tests

---

### Phase 5: Testing (Week 3)

#### Week 3 Testing Matrix

| Test Type | Coverage | Status |
|-----------|----------|--------|
| Unit - Registry | Registration, detection | [ ] |
| Unit - DwarfsBackend | All operations | [ ] |
| Unit - ZipBackend | All operations | [ ] |
| Integration - Multi-mount | DwarFS + ZIP | [ ] |
| Integration - Detection | Auto-detect formats | [ ] |
| Performance - DwarFS | No regression | [ ] |
| Performance - ZIP | Baseline established | [ ] |

---

## 🔧 Development Commands

### Build Commands
```bash
# Clean build
rm -rf build && mkdir build && cd build

# Configure
cmake -DTEBAKO_BUILD=ON \
      -DDWARFS_WITH_THRIFT=OFF \
      -DDWARFS_WITH_FLATBUFFERS=ON \
      -DWITH_TESTS=ON \
      -DBUILD_EXAMPLES=ON \
      ..

# Build
make -j$(nproc)

# Test specific component
ctest -R zip_backend --verbose

# Test everything
ctest --output-on-failure
```

### Debug Commands
```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run under lldb/gdb
lldb ./tests/test_zip_backend
```

### Code Quality
```bash
# Format code
clang-format -i include/tebako/fs/backends/*.h
clang-format -i src/backends/*.cpp

# Static analysis
clang-tidy src/backends/*.cpp -- -I../include
```

---

## 📊 Progress Tracking

### Daily Checklist Template

```markdown
## Day N: [Task Name]

**Planned**:
- [ ] Task 1
- [ ] Task 2
- [ ] Task 3

**Completed**:
- [x] Actual task 1
- [x] Actual task 2

**Issues**:
- Issue description
- Resolution approach

**Tomorrow**:
- Next planned tasks
```

### Week Summary Template

```markdown
## Week N Summary

**Completed**:
- Major milestone 1
- Major milestone 2

**Tests**: X passing / Y total

**Blockers**: None / Description

**Next Week Plan**:
- Focus area
```

---

## 🎯 Success Criteria Checklist

Mark each when complete:

### Code Complete
- [ ] All interfaces implemented
- [ ] DwarfsBackend refactored
- [ ] ZipBackend fully functional
- [ ] Mount table integration complete
- [ ] No compiler warnings

### Testing Complete
- [ ] All unit tests passing
- [ ] All integration tests passing
- [ ] Code coverage >80%
- [ ] Performance baseline established
- [ ] No regressions

### Documentation Complete
- [ ] API documentation (Doxygen)
- [ ] User guide updated
- [ ] Examples working
- [ ] CHANGELOG.md updated
- [ ] README.md updated

### Ready for Stage 3
- [ ] Stable branch created
- [ ] All reviewers approved
- [ ] CI/CD passing on all platforms

---

## 🆘 Troubleshooting

### Issues & Solutions

**libzip not found**
```bash
# macOS
brew install libzip

# Ubuntu
sudo apt-get install libzip-dev

# Or use vcpkg
vcpkg install libzip
```

**Existing code breaks**
- Keep old implementation alongside new
- Use feature flags during transition
- Create compatibility layer

**Tests failing**
- Check test fixtures exist
- Verify test archives are valid
- Run tests individually with `--verbose`

**Performance issues**
- Profile with `perf` or Instruments
- Check caching is enabled
- Verify no unnecessary copies

---

## 📞 Getting Help

- **Design Questions**: Review [`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md)
- **Architecture**: See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- **Testing**: See [`docs/TESTING_STRATEGY.md`](docs/TESTING_STRATEGY.md)
- **Issues**: Create GitHub issue with [Stage 2] prefix

---

**Version**: 1.0
**Last Updated**: 2025-12-21
**Status**: Ready for implementation