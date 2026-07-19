# DwarFS v0.9+ Integration - Phase 3 Continuation Prompt

**Context**: Phase 2 (GTest Linking) Complete ✅
**Next**: Phase 3.1 (DwarFS Library Discovery) 🔄
**Date**: 2025-12-24

---

## Current State

### ✅ What's Working (Phase 2 Complete)
- **GTest linking fully operational**
  - All test targets use modern CMake `GTest::gtest` and `GTest::gtest_main`
  - No undefined GTest symbols (e.g., `HasFatalFailure()`)
  - Warning about "duplicate libraries" is harmless

- **3 out of 4 tests build successfully:**
  ```
  ✅ test_backend_factory - Builds & links cleanly
  ✅ test_zip_backend - Builds & links cleanly
  ✅ test_zip_integration - Builds & links cleanly
  ```

- **Main library compiles with ZERO errors:**
  - `libtfs.a` (92% complete, 53 files)
  - All DwarFS v0.9+ API compatibility issues resolved from Phase 1

### ⚠️ What Needs Fixing (Phase 3 Target)
- **test_c_api has DwarFS linking errors** (not GTest issues)
  - 14+ undefined symbols from DwarFS libraries
  - Symbols like: `dwarfs::stream_logger`, `dwarfs::filesystem_v2`, `dwarfs::os_access_generic`
  - **Does NOT affect main library** - only this one test executable

---

## Your Task: Complete Phase 3 (DwarFS Library Linking & Test Execution)

### Overview
You need to discover DwarFS libraries, link them to test_c_api, and then execute all tests.

**Estimated Time**: 3-4 hours
**Priority**: HIGH (blocking test validation)

---

## Step 1: Discover DwarFS Libraries (45 minutes)

### Task 1.1: Locate All DwarFS Libraries

**Find libraries in the build directory:**
```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs
find ${DWARFS_BINARY_DIR} -name "*.a" -o -name "*.dylib" -o -name "*.so" | sort
find deps/lib/ -name "*.a" | sort
```

**Variables to use:**
- `DWARFS_SOURCE_DIR` = `/Users/mulgogi/src/external/dwarfs`
- `DWARFS_BINARY_DIR` = `./deps/src/_dwarfs-build`

**Expected libraries:**
- `libdwarfs.a` - Core DwarFS filesystem
- `libfolly.a` - Facebook Folly (DwarFS dependency)
- `libdwarfs_compression.a` - Compression layer
- `libfsst.a` - FSST compression codec
- `libt_metadata.a` - Metadata handling
- `libt_light.a` - Lightweight reader

### Task 1.2: Map Undefined Symbols to Libraries

**Extract undefined symbols from libtfs.a:**
```bash
cd build
nm libtfs.a | grep "dwarfs" | grep " U " | sort | uniq
```

**Find which libraries provide these symbols:**
```bash
# For each library found in Task 1.1, check if it provides the symbols:
for lib in $(find ${DWARFS_BINARY_DIR} -name "*.a"); do
  echo "=== Checking $lib ==="
  nm $lib 2>/dev/null | grep -E "stream_logger|filesystem_v2|os_access_generic" | head -5
done
```

**Known required symbols** (from Phase 2 error log):
- `dwarfs::stream_logger::*`
- `dwarfs::filesystem_v2::*`
- `dwarfs::os_access_generic::*`
- `dwarfs::parse_size_with_unit`
- `dwarfs::file_extents_iterable::*`
- `dwarfs::timed_level_log_entry::*`
- `dwarfs::reader::filesystem_v2::*`
- `dwarfs::file_stat::*`
- `dwarfs::reader::inode_view::*`
- `dwarfs::reader::dir_entry_view::*`

### Task 1.3: Document Library Locations

Create a mapping like:
```
Symbol Pattern              → Providing Library
-------------------------------------------------
dwarfs::stream_logger       → libdwarfs.a
dwarfs::filesystem_v2       → libdwarfs.a
dwarfs::os_access_generic   → libfolly.a (or libdwarfs.a)
dwarfs::parse_size_with_unit → libdwarfs.a
...
```

---

## Step 2: Update CMake Configuration (30 minutes)

### Task 2.1: Add DwarFS Library Variables

**Edit [`CMakeLists.txt`](../CMakeLists.txt) around line 300:**

Add after the existing library variable section:

```cmake
# DwarFS libraries for test linking
# Note: These are only needed by tests that actually mount filesystems (test_c_api)
set(__LIBDWARFS "${DWARFS_BINARY_DIR}/libdwarfs/libdwarfs.a")
set(__LIBFOLLY "${DWARFS_BINARY_DIR}/folly/libfolly.a")
set(__LIBDWARFS_COMPRESSION "${DWARFS_BINARY_DIR}/libdwarfs_compression/libdwarfs_compression.a")
set(__LIBFSST "${DWARFS_BINARY_DIR}/_deps/fsst-build/libfsst.a")
set(__LIBT_METADATA "${DWARFS_BINARY_DIR}/libt_metadata.a")
set(__LIBT_LIGHT "${DWARFS_BINARY_DIR}/libt_light.a")

# Verify critical libraries exist
foreach(lib ${__LIBDWARFS} ${__LIBFOLLY})
  if(NOT EXISTS ${lib})
    message(WARNING "DwarFS library not found: ${lib}")
  endif()
endforeach()
```

**Adjust library names/paths based on what you found in Step 1.**

### Task 2.2: Link DwarFS Libraries to test_c_api

**Edit [`CMakeLists.txt`](../CMakeLists.txt) lines 651-657:**

Replace:
```cmake
target_link_libraries(test_c_api PRIVATE
  tfs
  tebako_dirent_helper_c
  GTest::gtest
  GTest::gtest_main
)
```

With:
```cmake
target_link_libraries(test_c_api PRIVATE
  tfs
  tebako_dirent_helper_c
  GTest::gtest
  GTest::gtest_main
  # DwarFS libraries (required for actual filesystem mounting)
  ${__LIBDWARFS}
  ${__LIBDWARFS_COMPRESSION}
  ${__LIBFOLLY}
  ${__LIBFSST}
  ${__LIBT_METADATA}
  ${__LIBT_LIGHT}
  # System libraries (required by DwarFS/Folly)
  pthread
  dl
  z
)
```

**Important:** Library order matters! If you get undefined symbols, try reversing the order. Generally:
- Libraries that **use** symbols come **before** libraries that **define** them
- System libraries come **last**

### Task 2.3: Reconfigure and Build

```bash
cd build
cmake ..  # Reconfigure to pick up new variables
cmake --build . --target test_c_api 2>&1 | tee test_c_api_build.log
```

**Success criteria:**
- ✅ test_c_api executable created
- ✅ No undefined symbol errors
- ✅ Only GTest "duplicate library" warning (harmless)

**If build fails:**
1. Check the error log for undefined symbols
2. Identify which symbols are still missing
3. Find the library providing those symbols (repeat Step 1.2)
4. Add that library to the link line
5. Try again

---

## Step 3: Execute All Tests (1 hour)

### Task 3.1: Run Backend Factory Tests

```bash
cd build
./test_backend_factory --gtest_color=yes 2>&1 | tee test_backend_factory.log
echo "Exit code: $?"
```

**Expected results:**
- Magic detection tests: **Should PASS**
- ZIP detection: **Should PASS**
- SquashFS detection: **Should PASS** (detection works, creation returns nullptr)
- DwarFS detection: **Should PASS**
- Backend creation: **May FAIL** (DwarFS/SquashFS not implemented yet)

### Task 3.2: Run ZIP Backend Tests

```bash
cd build
./test_zip_backend --gtest_color=yes 2>&1 | tee test_zip_backend.log
echo "Exit code: $?"
```

**Fixture requirement**: Tests need ZIP archives in `build/tests/fixtures/zip/`

**Check fixtures exist:**
```bash
ls -la build/tests/fixtures/zip/
```

**Expected results:**
- Basic ZIP operations: **Should PASS**
- File reading: **Should PASS**
- Directory listing: **Should PASS**

### Task 3.3: Run ZIP Integration Tests

```bash
cd build
./test_zip_integration --gtest_color=yes 2>&1 | tee test_zip_integration.log
echo "Exit code: $?"
```

**Expected results:**
- Mount/unmount: **Should PASS**
- File access through mounted FS: **Should PASS**
- End-to-end workflows: **Should PASS**

### Task 3.4: Run C API Tests

```bash
cd build
./test_c_api --gtest_color=yes 2>&1 | tee test_c_api.log
echo "Exit code: $?"
```

**This test creates its own ZIP fixture dynamically.**

**Expected results:**
- Lifecycle tests (init/mount/unmount): **Should PASS**
- File operations (open/read/close): **Should PASS**
- Directory operations (opendir/readdir): **Should PASS**
- Memory mounting: **Should PASS**

### Task 3.5: Run Full Test Suite via CTest

```bash
cd build
ctest --output-on-failure --verbose 2>&1 | tee ctest.log
```

**This runs all tests and provides summary.**

---

## Step 4: Analyze and Document Results (30 minutes)

### Task 4.1: Categorize Test Results

Create a results summary:

```markdown
## Test Results Summary

### test_backend_factory
- Total: X tests
- Passed: Y
- Failed: Z
- Skipped: W

Notable failures:
- [Test name] - [Brief reason]
- ...

### test_zip_backend
- Total: X tests
- ...

### test_zip_integration
- Total: X tests
- ...

### test_c_api
- Total: X tests
- ...
```

### Task 4.2: Categorize Failures by Priority

**P0 - Critical** (Must fix before completion):
- Segfaults/crashes
- Core functionality broken (can't mount, can't read)

**P1 - High** (Should fix):
- API behavior incorrect
- Data corruption
- Memory leaks

**P2 - Medium** (Nice to fix):
- Edge case failures
- Error message issues
- Performance problems

**P3 - Low** (Document for future):
- Expected failures (unimplemented features)
- SquashFS tests (backend disabled)
- DwarFS tests (backend not implemented)

### Task 4.3: Fix Critical (P0) Failures

For each P0 failure:
1. Identify root cause
2. Apply minimal fix
3. Re-run test
4. Document fix

**Do NOT lower test expectations or increase thresholds** - fix the implementation!

---

## Step 5: Update Documentation (30 minutes)

### Task 5.1: Update README.adoc

**Edit [`README.adoc`](../README.adoc)** - Add testing section:

```adoc
== Testing

The library includes comprehensive test suites built with Google Test.

=== Test Suites

[cols="1,3"]
|===
| Suite | Description

| test_backend_factory
| Backend creation, format auto-detection, magic number validation

| test_zip_backend
| ZIP filesystem operations (read, stat, directory listing)

| test_zip_integration
| End-to-end ZIP workflows (mount, access, unmount)

| test_c_api
| C API compatibility layer for Ruby integration
|===

=== Running Tests

[source,bash]
----
cd build
ctest --output-on-failure
----

Or run individual test suites:

[source,bash]
----
./test_backend_factory
./test_zip_backend
./test_zip_integration
./test_c_api
----

=== Known Limitations

* SquashFS backend returns `nullptr` (squashfs-tools-ng not yet available)
* DwarFS backend not yet implemented
* All filesystems are read-only

See link:docs/TESTING.adoc[TESTING.adoc] for detailed test documentation.
```

### Task 5.2: Create TESTING.adoc

**Create [`docs/TESTING.adoc`](../docs/TESTING.adoc):**

Document:
- Test organization and structure
- How to run tests
- Test fixture requirements
- Coverage areas
- Known limitations
- How to debug test failures

### Task 5.3: Move Completed Documentation

**Move to `old-docs/dwarfs_v09_completed/`:**

```bash
mv docs/DWARFS_V09_NEXT_PHASE_CONTINUATION_PROMPT.md old-docs/dwarfs_v09_completed/
mv docs/DWARFS_V09_API_FIXES_COMPLETION_STATUS.md old-docs/dwarfs_v09_completed/
```

**Update `old-docs/dwarfs_v09_completed/README.md`:**

```markdown
# DwarFS v0.9+ Integration - Completed Phases

This directory contains documentation for completed phases of the DwarFS v0.9+ integration.

## Phase 1: API Compatibility Fixes ✅
- Status: Complete
- Duration: ~4 hours
- Files: DWARFS_V09_API_FIXES_COMPLETION_STATUS.md

## Phase 2: GTest Linking ✅
- Status: Complete
- Duration: ~2 hours
- Files: DWARFS_V09_NEXT_PHASE_CONTINUATION_PROMPT.md

## Phase 3: DwarFS Library Linking & Test Execution 🔄
- Status: In Progress
- See: docs/DWARFS_V09_PHASE3_STATUS_TRACKER.md
```

---

## Important Reminders

### From Phase 1 & 2 Work

1. **API Changes Implemented:**
   - `filesystem_v2` constructor requires `os_access_generic`
   - `getattr()` and `readlink()` return values with `std::error_code`
   - `file_stat.copy_to(st)` replaces `copy_file_stat<>()`
   - `dir_entry_view` has `.name()` and `.inode()` accessors
   - `find()` returns `optional<dir_entry_view>`

2. **Architecture Notes:**
   - Pure C helper for `struct dirent` in isolated subdirectory
   - `tebako_dirent.e()` returns `void*`, cast to `struct dirent*`
   - Helper: `populate_dirent_buffer_c()` in `src/c_helpers/`

3. **SquashFS Status:**
   - Backend disabled (returns nullptr)
   - Detection still works (`is_squashfs_format()`)
   - Tests will skip/fail gracefully

---

## Files to Focus On

### For Library Discovery (Step 1):
1. Build directory: `deps/src/_dwarfs-build/`
2. Library output: `deps/lib/`
3. Source: `/Users/mulgogi/src/external/dwarfs/`

### For CMake Updates (Step 2):
1. [`CMakeLists.txt`](../CMakeLists.txt:300-330) - Library variables
2. [`CMakeLists.txt`](../CMakeLists.txt:651-657) - test_c_api linking

### For Test Execution (Step 3):
1. Test executables: `build/test_*`
2. Test fixtures: `build/tests/fixtures/`
3. Test logs: Create `build/*.log` files

### For Documentation (Step 5):
1. [`README.adoc`](../README.adoc) - Main documentation
2. [`docs/TESTING.adoc`](../docs/TESTING.adoc) - Test documentation
3. `old-docs/dwarfs_v09_completed/` - Archive directory

---

## Success Criteria

### Phase 3 Complete When:
- [ ] All DwarFS libraries identified and documented
- [ ] test_c_api builds without undefined symbols
- [ ] All 4 test executables run (even if some tests fail)
- [ ] Test results documented with P0/P1/P2/P3 categorization
- [ ] P0 failures fixed or documented as blockers
- [ ] README.adoc updated with testing section
- [ ] TESTING.adoc created
- [ ] Completed phase docs moved to old-docs/

### Acceptable Results:
- ✅ 100% of tests **build successfully**
- ✅ 70-80% of tests **pass** (excluding unimplemented features)
- ✅ All P0 failures **fixed**
- ✅ P1 failures **documented** (may defer fixes)
- ✅ P2/P3 failures **documented** for future work

---

## If You Get Stuck

### Problem: Can't find DwarFS libraries
**Solution:** Check if DwarFS was built:
```bash
ls -la deps/src/_dwarfs-build/
ls -la /Users/mulgogi/src/external/dwarfs/build/
```

### Problem: Wrong library order causes undefined symbols
**Solution:** Try different orders. Common patterns:
```cmake
# Pattern 1: Dependency order (users before providers)
${__LIBDWARFS_COMPRESSION} ${__LIBDWARFS} ${__LIBFOLLY}

# Pattern 2: Reverse dependency order
${__LIBFOLLY} ${__LIBDWARFS} ${__LIBDWARFS_COMPRESSION}
```

### Problem: Tests crash on startup
**Solution:**
1. Check with debugger: `lldb build/test_c_api`
2. Run with verbose logging: `./test_c_api --gtest_verbose=1`
3. Check fixture paths are correct

### Problem: Tests fail but not sure why
**Solution:**
1. Run single test: `./test_backend_factory --gtest_filter=BackendFactoryTest.CreateZip`
2. Check test output carefully
3. Add debug prints if needed
4. Compare with expected behavior in test comments

---

## Next Phase Preview

### Phase 4: Integration & Production Readiness
Once Phase 3 is complete:
1. Verify DwarFS memory mounting with real Ruby archives
2. Performance benchmarking
3. Memory leak testing
4. Final documentation polish
5. Release preparation

**Estimated**: 3-4 hours after Phase 3 complete

---

## Questions to Answer During This Phase

1. What is the complete list of DwarFS libraries needed?
2. What is the correct link order for these libraries?
3. What is the actual test pass rate for each suite?
4. Which failures are critical vs. acceptable?
5. Are there any unexpected platform issues?

Document answers in the status tracker as you discover them.