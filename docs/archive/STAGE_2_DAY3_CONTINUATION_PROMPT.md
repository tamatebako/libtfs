# Stage 2 Day 3: ZIP Backend Unit Testing - Continuation Prompt

**Date**: 2025-12-22
**Mode**: Code
**Duration**: 1 day (4-5 hours)
**Prerequisites**: Day 2 Complete ✅

---

## Context

You are continuing Stage 2 implementation of the Tebako filesystem library (libtfs). Day 2 successfully implemented a fully functional ZIP backend with complete FileSystem interface implementation, compilation verified, and basic manual testing completed.

**Day 3 Objective**: Create comprehensive unit tests for every function in the ZIP backend using GoogleTest and CMake integration.

---

## What Has Been Completed (Day 1-2)

### Day 1 ✅
- vcpkg overlay for squashfs-tools-ng
- vcpkg.json with libzip and squashfs-tools-ng dependencies
- BackendFactory with format auto-detection
- Build integration (CMakeLists.txt)

### Day 2 ✅
- [`include/tebako/fs/backends/zip_backend.h`](../include/tebako/fs/backends/zip_backend.h) (263 lines)
- [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp) (795 lines)
  - ZipFileHandle class (file operations)
  - ZipDirectoryIterator class (directory traversal)
  - ZipBackend class (main backend implementation)
- [`src/backend_factory.cpp`](../src/backend_factory.cpp) updated with ZipBackend instantiation
- [`CMakeLists.txt`](../CMakeLists.txt) updated with ZIP sources and libzip linking
- Manual testing verified (all operations working correctly)

**Current Status**: ZIP backend is functionally complete and working, but lacks comprehensive unit tests.

---

## Your Task: Day 3 - Comprehensive Unit Testing

### Objective
Create a complete GoogleTest-based unit test suite for the ZIP backend that:
1. Tests every public method of every class
2. Covers edge cases and error conditions
3. Verifies thread safety
4. Integrates with CMake/CTest
5. Achieves 100% pass rate

### Success Criteria
- ✅ Test fixtures created (5 test ZIP archives)
- ✅ Unit test file created (`tests/test_zip_backend.cpp`)
- ✅ All public methods have test cases (100% coverage)
- ✅ Edge cases tested (empty files, nested dirs, large files)
- ✅ Error handling tested (invalid paths, corrupted archives)
- ✅ Thread safety verified (concurrent operations)
- ✅ CMakeLists.txt updated with test target
- ✅ All tests passing (100% pass rate)
- ✅ Zero memory leaks (valgrind verified)

---

## Implementation Steps

### Step 1: Create Test Fixtures (30 minutes)

Create directory structure and test ZIP archives:

```bash
mkdir -p tests/fixtures/zip
cd tests/fixtures/zip

# 1. simple.zip - Basic functionality
mkdir -p simple && cd simple
echo "Hello from ZIP!" > test.txt
echo "Second file" > file2.txt
zip ../simple.zip test.txt file2.txt
cd .. && rm -rf simple

# 2. nested.zip - Directory structure
mkdir -p nested/dir1/subdir nested/dir2
echo "File 1" > nested/dir1/file1.txt
echo "File 2" > nested/dir1/subdir/file2.txt
echo "File 3" > nested/dir2/file3.txt
cd nested && zip -r ../nested.zip * && cd ..
rm -rf nested

# 3. empty.zip - Edge cases
mkdir -p empty/empty_dir
touch empty/empty_file.txt
cd empty && zip -r ../empty.zip * && cd ..
rm -rf empty

# 4. large.zip - Performance testing
mkdir -p large/many_files
dd if=/dev/urandom of=large/large.txt bs=1M count=10 2>/dev/null
for i in {1..100}; do echo "File $i" > large/many_files/file$i.txt; done
cd large && zip -r ../large.zip * && cd ..
rm -rf large

# 5. corrupted.zip - Error handling
cp simple.zip corrupted.zip
dd if=/dev/zero of=corrupted.zip bs=1 count=100 seek=100 conv=notrunc 2>/dev/null

cd ../../..
```

### Step 2: Create Unit Test File (2-3 hours)

Create [`tests/test_zip_backend.cpp`](../tests/test_zip_backend.cpp) with the following test sections:

#### Test Structure

```cpp
#include <gtest/gtest.h>
#include <tebako/fs/backends/zip_backend.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>
#include <thread>
#include <vector>
#include <atomic>

using namespace tebako::fs;

// Base test fixture
class ZipBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        backend = std::make_unique<ZipBackend>();
    }

    void TearDown() override {
        if (backend && backend->is_mounted()) {
            backend->unmount();
        }
    }

    std::unique_ptr<ZipBackend> backend;
    const std::string fixtures_path = "tests/fixtures/zip/";
    const std::string mount_point = "/mnt/test";
};

// Mounted test fixture (inherits from base)
class ZipBackendMountedTest : public ZipBackendTest {
protected:
    void SetUp() override {
        ZipBackendTest::SetUp();
        std::string archive = fixtures_path + "simple.zip";
        ASSERT_TRUE(backend->mount(archive, mount_point));
    }
};
```

#### Required Test Categories

1. **Lifecycle Tests** (8 tests)
   - Constructor creates unmounted backend
   - Backend info correct (name, version)
   - Mount valid archive succeeds
   - Mount nonexistent archive fails
   - Mount invalid/corrupted archive fails
   - Double mount fails
   - Unmount clears state
   - Unmount without mount is no-op

2. **File Existence Tests** (6 tests)
   - Exists returns true for valid file
   - Exists returns false for invalid file
   - is_file correct for files
   - is_file false for directories
   - is_directory correct for root
   - is_directory correct for nested dirs

3. **File Reading Tests** (12 tests)
   - Open valid file succeeds
   - Open invalid file fails
   - Open directory fails
   - Read file contents correct
   - Read increments tell position
   - Read sets EOF flag
   - Seek SET positions correctly
   - Seek CUR positions correctly
   - Seek END positions correctly
   - Seek beyond bounds fails
   - Close releases resource
   - Operations after close fail

4. **Directory Listing Tests** (5 tests)
   - List directory returns all entries
   - Directory entry has correct metadata
   - Iterator reset works
   - List nested directory works
   - List empty directory returns no entries

5. **Metadata Tests** (4 tests)
   - File size correct
   - File size invalid file returns -1
   - Modification time non-zero
   - Permissions default for file/directory

6. **Nested Directory Tests** (3 tests)
   - Nested directory exists
   - Nested file exists
   - Can list nested directory

7. **Edge Case Tests** (3 tests)
   - Empty file has zero size
   - Read empty file returns zero
   - Empty directory lists no entries

8. **Thread Safety Tests** (2 tests)
   - Concurrent reads succeed
   - Concurrent directory lists succeed

9. **Error Handling Tests** (2 tests)
   - Operations on unmounted backend fail
   - Invalid operations return proper errors

10. **Performance Tests** (2 tests, optional)
    - Read large file performance
    - List many files performance

**Total**: 47+ test cases covering all functionality

See [`docs/STAGE_2_DAY3_CONTINUATION_PLAN.md`](STAGE_2_DAY3_CONTINUATION_PLAN.md) for complete test implementation code.

### Step 3: Update CMakeLists.txt (15 minutes)

Add test target and fixture copying to [`CMakeLists.txt`](../CMakeLists.txt):

```cmake
if (WITH_TESTS)
  # ... existing tests ...

  # ZIP Backend tests
  add_executable(test_zip_backend
    "tests/test_zip_backend.cpp"
  )
  target_compile_options(test_zip_backend PUBLIC ${GTEST_CFLAGS})
  target_link_libraries(test_zip_backend tfs ${GTestMain} ${GTEST_LDFLAGS})
  gtest_add_tests(TARGET test_zip_backend)

  # Copy test fixtures to build directory
  file(COPY tests/fixtures/zip DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures)

endif(WITH_TESTS)
```

### Step 4: Build and Run Tests (30 minutes)

```bash
# Configure (clean build recommended)
rm -rf build
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=${PWD}/vcpkg-overlay \
  -DWITH_TESTS=ON

# Build
cmake --build build -j$(nproc)

# Run ZIP backend tests only
cd build && ctest -R test_zip_backend --verbose

# Run all tests
ctest --verbose

# Generate test report
ctest -R test_zip_backend --output-on-failure
```

### Step 5: Memory Leak Verification (15 minutes)

```bash
# Run with valgrind
valgrind --leak-check=full \
  --show-leak-kinds=all \
  --error-exitcode=1 \
  build/tests/test_zip_backend

# Expected: "All heap blocks were freed -- no leaks are possible"
```

### Step 6: Fix Any Failing Tests (1 hour contingency)

If tests fail:
1. Identify failing test and error message
2. Determine if issue is in implementation or test expectation
3. Fix implementation bug OR update test expectation with justification
4. Re-run tests
5. Document any edge cases discovered
6. Update implementation status

---

## Important Implementation Notes

### Thread Safety Testing
- Use `std::atomic` for success counting
- Create multiple threads (10-20) for concurrent operations
- Join all threads before assertions
- Verify no race conditions or deadlocks

### Test Fixture Management
- Store fixtures in `tests/fixtures/zip/`
- Use relative paths from test executable
- CMake copies fixtures to build directory automatically
- Tests should be self-contained and repeatable

### Error Handling
- Always check for nullptr from open/list operations
- Use ASSERT for setup preconditions (must pass)
- Use EXPECT for actual test assertions (records failures)
- Test both success and failure paths

### Performance Tests
- Make optional using `GTEST_SKIP()` if fixtures missing
- Use `std::chrono` for timing
- Set reasonable thresholds (adjust for CI environment)
- Document performance expectations

---

## Architecture Compliance Checklist

- ✅ Each test focuses on one behavior (single responsibility)
- ✅ Tests are independent and can run in any order
- ✅ No global state or test interdependencies
- ✅ Proper setup/teardown in test fixtures
- ✅ Clear test names describe what is being tested
- ✅ Complete coverage of public API surface
- ✅ Edge cases and error conditions tested
- ✅ Thread safety verified through concurrent tests

---

## Common Issues and Solutions

### Issue 1: Test Fixtures Not Found
**Solution**: Ensure CMake copies fixtures to `${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/zip/`

### Issue 2: Tests Fail Due to Path Issues
**Solution**: Use `fixtures_path` member variable, ensure relative to test executable

### Issue 3: Memory Leaks Detected
**Solution**: Check that all `std::unique_ptr` are properly managed, TearDown() calls unmount()

### Issue 4: Thread Safety Tests Flaky
**Solution**: Increase iterations, use proper synchronization primitives, check for race conditions

### Issue 5: Performance Tests Fail on Slow Hardware
**Solution**: Make thresholds configurable or skip using `GTEST_SKIP()`

---

## Verification Commands

```bash
# Count test cases
grep -c "^TEST" tests/test_zip_backend.cpp

# Run specific test
ctest -R test_zip_backend --verbose -R "ZipBackendTest.MountValidArchiveSucceeds"

# Check for memory leaks
valgrind --leak-check=full build/tests/test_zip_backend 2>&1 | grep "no leaks"

# Generate coverage report (if available)
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

---

## Success Criteria Summary

By end of Day 3, you should have:

1. ✅ **Tests Created**: ~47 comprehensive test cases
2. ✅ **All Pass**: 100% pass rate
3. ✅ **Coverage**: All public methods tested
4. ✅ **Edge Cases**: Empty files, nested dirs, large files tested
5. ✅ **Errors**: Invalid paths, corrupted archives handled
6. ✅ **Thread Safe**: Concurrent operations verified
7. ✅ **No Leaks**: Valgrind confirms zero memory leaks
8. ✅ **Integrated**: CMake/CTest running tests automatically

---

## Documentation Requirements

After tests are complete:

1. Update [`README.adoc`](../README.adoc) - Add testing section
2. Create [`docs/TESTING.md`](TESTING.md) - Document test suite structure
3. Archive temporary docs to [`docs/archive/`](archive/)

---

## Next Steps After Day 3

**Day 4**: Integration Testing
- BackendFactory auto-detection tests
- Multi-backend tests (ZIP + DwarFS simultaneously)
- Format detection accuracy tests

**Day 5**: SquashFS Backend Implementation
- Similar to Day 2 but for SquashFS format
- Complete unit tests immediately after implementation

**Day 6**: Performance & Optimization
- Benchmark all backends
- Memory profiling
- Optimization opportunities

**Day 7**: Documentation & Release
- Update official documentation
- Archive temporary documentation
- Final verification
- Release notes

---

## References

### Source Code
- ZIP Backend Header: [`include/tebako/fs/backends/zip_backend.h`](../include/tebako/fs/backends/zip_backend.h)
- ZIP Backend Implementation: [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp)
- Interfaces: [`filesystem.h`](../include/tebako/fs/filesystem.h), [`file_handle.h`](../include/tebako/fs/file_handle.h), [`directory_iterator.h`](../include/tebako/fs/directory_iterator.h)

### Documentation
- Day 3 Plan: [`STAGE_2_DAY3_CONTINUATION_PLAN.md`](STAGE_2_DAY3_CONTINUATION_PLAN.md)
- Day 2 Status: [`STAGE_2_DAY2_COMPLETION_STATUS.md`](STAGE_2_DAY2_COMPLETION_STATUS.md)
- VFS Design: [`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md)
- Backend Factory: [`STAGE_2_BACKEND_FACTORY_DESIGN.md`](STAGE_2_BACKEND_FACTORY_DESIGN.md)

### External
- GoogleTest Documentation: https://google.github.io/googletest/
- CMake CTest: https://cmake.org/cmake/help/latest/manual/ctest.1.html
- valgrind User Manual: https://valgrind.org/docs/manual/manual.html

---

**Ready to Begin**: All prerequisites complete ✅
**Estimated Time**: 4-5 hours
**Mode**: Code
**Start with**: Create test fixtures directory and generate test ZIP archives

---

**Document Version**: 1.0
**Created**: 2025-12-22
**Status**: Ready for Day 3 Implementation