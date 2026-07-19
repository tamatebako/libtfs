# DwarFS v0.9+ Integration - Phase 3 Continuation Plan

**Status**: Phase 2 Complete (GTest Linking Fixed) ✅
**Next**: Phase 3 (DwarFS Library Linking & Test Execution) 🔄
**Date**: 2025-12-24

---

## Phase 2 Completion Summary ✅

### What Was Accomplished
1. **GTest Configuration Fixed**
   - Replaced undefined variables `${GTestMain}` and `${GTEST_LDFLAGS}`
   - Implemented modern CMake `find_package(GTest REQUIRED)` approach
   - All test targets now use `GTest::gtest` and `GTest::gtest_main`

2. **Build System Updates**
   - Updated [`CMakeLists.txt`](../CMakeLists.txt:597-658) with proper GTest linking
   - Added explicit linking of `tebako_dirent_helper_c` to all tests

3. **Backend Factory Adjustments**
   - Temporarily disabled SquashFS backend instantiation (returns `nullptr`)
   - Retained `is_squashfs_format()` detection for future use
   - Tests can still detect SquashFS magic but won't instantiate backend

4. **Test Code Fixes**
   - Fixed type mismatch: `tebako_dirent*` → `tebako_c_dirent*`
   - All type references now consistent with C API

### Build Results
```
✅ test_backend_factory - Builds successfully
✅ test_zip_backend - Builds successfully
✅ test_zip_integration - Builds successfully
❌ test_c_api - Needs DwarFS library linking (see Phase 3)
```

---

## Phase 3: DwarFS Library Linking & Test Execution

### Objective
Complete test infrastructure by linking DwarFS libraries and validate all functionality.

### Timeline
- **Estimated Duration**: 3-4 hours
- **Priority**: HIGH (blocking test execution)

---

## Phase 3.1: DwarFS Library Discovery (45 minutes)

### Goals
1. Locate DwarFS libraries in build directory
2. Identify required libraries for test linking
3. Determine proper link order

### Tasks

#### 3.1.1: Locate DwarFS Libraries
```bash
# Find all DwarFS-related libraries
find ${DWARFS_BINARY_DIR} -name "*.a" -o -name "*.dylib" -o -name "*.so"

# Check what's in the deps directory
ls -la deps/lib/
```

#### 3.1.2: Identify Required Symbols
```bash
# Extract required DwarFS symbols from libtfs.a
nm build/libtfs.a | grep -i "dwarfs" | grep " U "

# Check which libraries provide these symbols
for lib in ${DWARFS_BINARY_DIR}/**/*.a; do
  echo "=== $lib ==="
  nm $lib | grep "stream_logger\|filesystem_v2\|os_access_generic"
done
```

#### 3.1.3: Document Library Dependencies
Create a list of required libraries:
- `libdwarfs.a` - Core DwarFS filesystem
- `libfolly.a` - Facebook Folly library
- `libdwarfs_compression.a` - Compression support
- `libfsst.a` - FSST compression
- Other dependencies as identified

---

## Phase 3.2: Update CMake Configuration (30 minutes)

### Goals
1. Add DwarFS library variables to CMakeLists.txt
2. Link DwarFS libraries to test targets
3. Ensure proper link order

### Tasks

#### 3.2.1: Define DwarFS Library Variables
Add to [`CMakeLists.txt`](../CMakeLists.txt) after line 300:

```cmake
# DwarFS library paths
set(__LIBDWARFS "${DWARFS_BINARY_DIR}/libdwarfs/libdwarfs.a")
set(__LIBFOLLY "${DWARFS_BINARY_DIR}/folly/libfolly.a")
set(__LIBDWARFS_COMPRESSION "${DWARFS_BINARY_DIR}/libdwarfs_compression.a")
set(__LIBFSST "${DWARFS_BINARY_DIR}/_deps/fsst-build/libfsst.a")
# Add other required libraries
```

#### 3.2.2: Update test_c_api Linking
Modify test_c_api target in [`CMakeLists.txt`](../CMakeLists.txt:651-657):

```cmake
target_link_libraries(test_c_api PRIVATE
  tfs
  tebako_dirent_helper_c
  GTest::gtest
  GTest::gtest_main
  ${__LIBDWARFS}
  ${__LIBDWARFS_COMPRESSION}
  ${__LIBFOLLY}
  ${__LIBFSST}
  # Additional libraries as needed
)
```

#### 3.2.3: Consider Link Order
DwarFS libraries may have interdependencies. Ensure:
- Libraries that use symbols come before libraries that define them
- System libraries (pthread, dl, z) come last

---

## Phase 3.3: Build and Validate Tests (45 minutes)

### Goals
1. Build all test executables successfully
2. Verify no undefined symbol errors
3. Confirm test executables are runnable

### Tasks

#### 3.3.1: Clean Rebuild
```bash
cd build
rm -f CMakeFiles/test_*.dir/*.o
rm -f test_*
cmake ..
cmake --build . --target test_backend_factory
cmake --build . --target test_zip_backend
cmake --build . --target test_zip_integration
cmake --build . --target test_c_api
```

#### 3.3.2: Verify Symbol Resolution
```bash
# Check no undefined symbols in test executables
for test in test_backend_factory test_zip_backend test_zip_integration test_c_api; do
  echo "=== $test ==="
  nm build/$test | grep " U " | head -20
done
```

#### 3.3.3: Test Execution Validation
```bash
# Verify executables are valid
file build/test_*

# Check they can at least start (--help or --version)
build/test_backend_factory --help || echo "No help option, but executable"
```

---

## Phase 3.4: Execute Tests (1 hour)

### Goals
1. Run all test suites
2. Identify failing tests
3. Document pass/fail status

### Tasks

#### 3.4.1: Backend Factory Tests
```bash
cd build
./test_backend_factory --gtest_color=yes
```

**Expected Results:**
- Magic detection tests should pass
- Auto-detection tests may fail (backends not fully implemented)
- SquashFS tests will skip/fail (backend returns nullptr)

#### 3.4.2: ZIP Backend Tests
```bash
cd build
./test_zip_backend --gtest_color=yes
```

**Expected Results:**
- Basic ZIP operations should pass
- File reading tests should pass
- Directory iteration tests should pass

#### 3.4.3: ZIP Integration Tests
```bash
cd build
./test_zip_integration --gtest_color=yes
```

**Expected Results:**
- End-to-end ZIP workflows should pass
- Mount/unmount operations should work
- File access through mounted filesystem should work

#### 3.4.4: C API Tests
```bash
cd build
./test_c_api --gtest_color=yes
```

**Expected Results:**
- Lifecycle tests should pass (init, mount, unmount)
- File operation tests should pass
- Directory operation tests should pass
- Memory mounting tests should pass

#### 3.4.5: CTest Integration
```bash
cd build
ctest --output-on-failure
```

---

## Phase 3.5: Fix Failing Tests (1 hour)

### Strategy
1. **Categorize failures**:
   - Build/linking issues (Phase 3.2 incomplete)
   - Implementation bugs (code fixes needed)
   - Test expectations incorrect (test fixes needed)

2. **Priority order**:
   - P0: Segfaults/crashes
   - P1: Core functionality failures
   - P2: Edge case failures
   - P3: Expected failures (unimplemented features)

3. **Fix approach**:
   - For each failing test, determine root cause
   - Apply minimal fix to implementation or test
   - Re-run test suite to verify fix
   - Document any deferred fixes

---

## Phase 3.6: Documentation Updates (30 minutes)

### Goals
1. Update README.adoc with current status
2. Document test infrastructure
3. Move completed work docs to old-docs/

### Tasks

#### 3.6.1: Update README.adoc
Add test execution section:
```adoc
== Testing

The library includes comprehensive test suites:

- `test_backend_factory` - Backend creation and format detection
- `test_zip_backend` - ZIP filesystem operations
- `test_zip_integration` - End-to-end ZIP workflows
- `test_c_api` - C API compatibility layer

Run tests:
[source,bash]
----
cd build
ctest --output-on-failure
----
```

#### 3.6.2: Create Test Documentation
Document in [`docs/TESTING.adoc`](../docs/TESTING.adoc):
- Test suite organization
- How to run tests
- Test coverage areas
- Known limitations

#### 3.6.3: Archive Completed Work
Move to `old-docs/dwarfs_v09_completed/`:
- `DWARFS_V09_NEXT_PHASE_CONTINUATION_PROMPT.md`
- `DWARFS_V09_API_FIXES_COMPLETION_STATUS.md`
- Any other completed phase documentation

---

## Success Criteria

### Phase 3 Complete When:
- [x] DwarFS libraries identified and documented
- [ ] All 4 test executables build successfully
- [ ] test_c_api links without undefined symbols
- [ ] Tests can be executed (even if some fail)
- [ ] Test results documented
- [ ] README.adoc updated with test info
- [ ] Completed docs moved to old-docs/

### Known Acceptable Failures:
1. **SquashFS tests** - Backend not implemented yet
2. **DwarFS backend tests** - Backend not implemented yet
3. **Write operation tests** - Read-only filesystem

---

## Risk Assessment

### High Risk
- **DwarFS library compatibility**: Version mismatches could cause runtime issues
- **Link order issues**: Complex dependency chains may require iteration

### Medium Risk
- **Test fixture data**: ZIP archives may need recreation
- **Platform differences**: macOS vs Linux library paths

### Low Risk
- **GTest version**: Already verified compatible
- **C API types**: Already fixed in Phase 2

---

## Rollback Plan

If Phase 3 blocks:
1. Document the blocker
2. Create minimal reproduction case
3. File issue with details:
   - CMake configuration
   - Link errors
   - Library versions
4. Continue with other work (documentation, cleanup)

---

## Next Phase Preview

### Phase 4: Integration & Cleanup
1. Verify DwarFS memory mounting works
2. Test with real Ruby archives
3. Performance validation
4. Final documentation
5. Release preparation

**Estimated**: 3-4 hours