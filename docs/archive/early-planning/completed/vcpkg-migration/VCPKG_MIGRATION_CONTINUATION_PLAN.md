# vcpkg Migration Continuation Plan

**Date**: 2025-12-27
**Current Status**: vcpkg integration complete, 44/47 tests passing (93.6%)
**Estimated Time to Complete**: 30-45 minutes

---

## Overview

The vcpkg migration is **functionally complete**. The build system now uses modern CMake package management with vcpkg. Only 3 minor test failures remain (all simple implementation fixes, not architectural issues).

---

## Phase 1: Fix Remaining Test Failures (15-20 minutes)

### Test 1: ListEmptyDirectoryReturnsNoEntries

**File**: [`src/backends/dwarfs_backend.cpp:226-250`](../src/backends/dwarfs_backend.cpp:226)

**Issue**: Empty directory iteration may not be handling the no-entries case correctly.

**Fix**:
```cpp
// In DwarfsDirectoryIterator constructor
auto dir = fs_.opendir(dir_inode);
if (dir) {
  size_t offset = 0;
  while (auto entry = fs_.readdir(*dir, offset++)) {
    // Ensure we actually have an entry before processing
    if (!entry) break;

    DirectoryEntry de;
    // ... rest of code
  }
}
```

### Test 2: PermissionsPreservedCorrectly

**File**: [`src/backends/dwarfs_backend.cpp:653`](../src/backends/dwarfs_backend.cpp:653)

**Issue**: Mode values include S_IFREG flag (0100000). Test expects just permission bits.

**Current**:
```cpp
return ec ? 0 : stat.mode();  // Returns 33188 (0100644) for regular file
```

**Fix**:
```cpp
return ec ? 0 : (stat.mode() & 0777);  // Returns just permission bits (0644)
```

### Test 3: PathNormalizationWorks

**File**: [`src/backends/dwarfs_backend.cpp:685-699`](../src/backends/dwarfs_backend.cpp:685)

**Issue**: Path normalization doesn't handle embedded "./" segments.

**Current**:
```cpp
std::string normalize_path(const std::string& path) const {
  std::string result = path;
  // Remove leading slash
  if (!result.empty() && result.front() == '/') {
    result = result.substr(1);
  }
  // Remove trailing slash
  if (!result.empty() && result.back() == '/') {
    result = result.substr(0, result.size() - 1);
  }
  return result;
}
```

**Fix**:
```cpp
std::string normalize_path(const std::string& path) const {
  std::string result = path;

  // Remove leading slash
  if (!result.empty() && result.front() == '/') {
    result = result.substr(1);
  }

  // Remove embedded "./" segments
  size_t pos = 0;
  while ((pos = result.find("/./", pos)) != std::string::npos) {
    result.replace(pos, 3, "/");
  }

  // Handle leading "./"
  if (result.starts_with("./")) {
    result = result.substr(2);
  }

  // Remove trailing slash
  if (!result.empty() && result.back() == '/') {
    result = result.substr(0, result.size() - 1);
  }

  return result;
}
```

---

## Phase 2: Run All Tests (5-10 minutes)

### DwarFS Tests
```bash
cd build

# Backend tests (should be 47/47 passing after fixes)
./test_dwarfs_backend --gtest_brief=1

# Integration tests
./test_dwarfs_integration --gtest_brief=1

# C API tests
./test_c_api --gtest_brief=1
```

### Expected Results
- ✅ 47/47 DwarFS backend tests
- ✅ 13/13 DwarFS integration tests
- ✅ All C API tests with DwarFS

---

## Phase 3: Clean Up (5 minutes)

### Remove Debug Logging

**File**: [`src/backends/dwarfs_backend.cpp:324`](../src/backends/dwarfs_backend.cpp:324)

Remove temporary debug output:
```cpp
// Remove this line:
std::cerr << "DwarFS mount error: " << e.what() << std::endl;
```

Also remove `#include <iostream>` if not needed elsewhere.

### Update CMakeLists.txt

Remove obsolete variables that reference old build directories:
```cmake
# These can be removed:
set(DWARFS_SOURCE_DIR "/Users/mulgogi/src/external/dwarfs" CACHE PATH "Path to dwarfs source directory")
set(DWARFS_BINARY_DIR "${DEPS}/src/_dwarfs-build" CACHE PATH "Path to dwarfs build directory")
```

---

## Phase 4: Documentation Updates (Optional, 15-20 minutes)

### Update README.adoc

Add vcpkg build instructions:

```adoc
== Building with vcpkg

The project uses vcpkg for dependency management.

=== Prerequisites

* CMake 3.24+
* vcpkg installed
* DwarFS overlay-ports configured

=== Build Commands

[source,bash]
----
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build -j 4

# Test
cd build && ctest --output-on-failure
----

=== Dependencies

The following dependencies are automatically managed by vcpkg:

* **dwarfs** - DwarFS filesystem library (v0.16+)
* **libzip** - ZIP archive support
* **argtable3** - Command-line parsing
* **gtest** - Testing framework

All transitive dependencies (Boost, flatbuffers, compression libraries, etc.) are handled automatically.
```

### Create VCPKG_BUILD.adoc (Optional)

Document the vcpkg integration details, port fixes, and troubleshooting.

---

## Phase 5: Move Completed Documentation (5 minutes)

### Files to Move to old-docs/vcpkg_migration/

Create `old-docs/vcpkg_migration/` and move:
- `docs/VCPKG_MIGRATION_STATUS.md`
- `docs/VCPKG_MIGRATION_CONTINUATION_PLAN.md` (this file)
- Any temporary build/test logs

### Create Summary

`old-docs/vcpkg_migration/README.md`:
```markdown
# vcpkg Migration - Completed 2025-12-27

Successfully migrated libdwarfs from manual dependency management to vcpkg.

## Key Achievements
- ✅ vcpkg integration complete
- ✅ DwarFS port fixed (missing headers)
- ✅ Backend API updated for DwarFS v0.16
- ✅ 44/47 tests passing (93.6%)

## Files Modified
- vcpkg.json, vcpkg-configuration.json (new)
- CMakeLists.txt (simplified)
- src/backends/dwarfs_backend.cpp (API fixes)
- /Users/mulgogi/src/external/dwarfs/vcpkg_ports/dwarfs/portfile.cmake (fixed)

See VCPKG_MIGRATION_STATUS.md for details.
```

---

## Success Criteria

### Must Have ✅
- [x] vcpkg integration working
- [x] DwarFS backend compiles
- [x] Tests build and run
- [ ] 47/47 backend tests passing (currently 44/47)
- [ ] 13/13 integration tests passing
- [ ] All C API tests passing

### Nice to Have
- [ ] README.adoc updated with vcpkg instructions
- [ ] Cleanup old documentation
- [ ] Remove debug logging

---

## Timeline

| Phase | Task | Time | Status |
|-------|------|------|--------|
| 1 | Fix 3 test failures | 15-20 min | 🔲 Pending |
| 2 | Run all tests | 5-10 min | 🔲 Pending |
| 3 | Clean up code | 5 min | 🔲 Pending |
| 4 | Update docs | 15-20 min | 🔲 Optional |
| 5 | Archive old docs | 5 min | 🔲 Optional |

**Total**: 30-45 minutes (required), +20-25 minutes (optional documentation)

---

## Notes

- The vcpkg integration is **complete and working**
- The 3 failing tests are trivial fixes (implementation details, not architecture)
- The DwarFS port fix should be submitted to the upstream DwarFS project
- This migration provides a solid foundation for future backend additions