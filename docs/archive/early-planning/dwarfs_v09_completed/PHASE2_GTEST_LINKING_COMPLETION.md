# DwarFS v0.9+ Integration - Phase 2 Continuation Prompt

**Context**: Phase 1 (API Fixes) Complete ✅  
**Next**: Phase 2 (Test Infrastructure) 🔄  
**Date**: 2025-12-24

---

## Current State

### ✅ What's Working
- Main library `libtfs.a` builds with ZERO errors
- All DwarFS v0.9+ API compatibility issues resolved
- Clean architectural solution for struct dirent namespace problem
- All source files compile successfully:
  - `tebako-memfs.cpp` (53KB) - 20+ API errors fixed
  - `tebako-fd.cpp` (17KB) 
  - `tebako-io-root.cpp` (28KB)

### ⚠️ What Needs Fixing
- Test executables have GTest linking errors
- Symbols like `testing::Test::HasFatalFailure()` not found
- Does NOT affect main library functionality

---

## Your Task: Fix GTest Linking Issues

### Step 1: Investigate GTest Configuration (15 minutes)

**Check what GTest version is being used:**
```bash
find /opt/homebrew -name "libgtest*.a" -o -name "libgtest*.dylib" 2>/dev/null
brew list --versions googletest 2>/dev/null || echo "Not via brew"
```

**Check CMake GTest configuration:**
```bash
cd build
grep -r "GTest\|GTEST" CMakeCache.txt | head -20
```

**Look at current linking setup:**
- Read [`CMakeLists.txt`](../CMakeLists.txt) around lines 600-670 (test configuration)
- Check how `${GTestMain}` and `${GTEST_LDFLAGS}` are defined

### Step 2: Fix GTest Linking (30 minutes)

**Option A: Use Modern CMake GTest**
```cmake
find_package(GTest CONFIG REQUIRED)
target_link_libraries(test_zip_backend PRIVATE 
  tfs 
  GTest::gtest 
  GTest::gtest_main
)
```

**Option B: Fix Current Configuration**
```cmake
# Ensure proper library order
target_link_libraries(test_zip_backend PRIVATE
  tfs
  tebako_dirent_helper_c  # May need explicit link
  ${GTestMain}
  ${GTEST_LDFLAGS}
  pthread  # May be needed on some systems
)
```

### Step 3: Verify Tests Build (15 minutes)

Build each test individually to isolate issues:
```bash
cd build
cmake --build . --target test_backend_factory
cmake --build . --target test_zip_backend
cmake --build . --target test_zip_integration
cmake --build . --target test_c_api
```

### Step 4: Run Tests (30 minutes)

Once linking fixed:
```bash
cd build
ctest --output-on-failure
# Or run individually:
./test_backend_factory
./test_zip_backend
```

---

## Important Reminders

### From Completion Status

The API fixes are complete. key changes:

1. **filesystem_v2 constructor now requires `os_access_generic`**
2. **`getattr()` and `readlink()` return values with `std::error_code`**
3. **`file_stat.copy_to(st)` replaces `copy_file_stat<>()`**
4. **`dir_entry_view` has `.name()` and `.inode()` accessors**
5. **`find()` returns `optional<dir_entry_view>`**, extract inode with `.inode()`

### Architecture Notes

- **struct dirent handling**: Now uses pure C helper in isolated subdirectory
- **`tebako_dirent.e()`**: Returns `void*`, cast to `struct dirent*` in implementation files
- **C helper**: `populate_dirent_buffer_c()` in [`src/c_helpers/tebako-dirent-helper.c`](../src/c_helpers/tebako-dirent-helper.c)

---

## Files to Focus On

1. [`CMakeLists.txt`](../CMakeLists.txt:600-670) - Test configuration
2. [`tests/test_zip_backend.cpp`](../tests/test_zip_backend.cpp) - Failing test
3. Build configuration in `build/CMakeCache.txt`

---

## Success Criteria

✅ All test executables link successfully  
✅ Tests run without crashes  
✅ Test failures (if any) are logic issues, not build issues  

---

## If You Get Stuck

### Common GTest Issues

1. **Static vs Shared**: Ensure consistent linkage (all static or all shared)
2. **ABI Mismatch**: GTest must be built with same C++ standard (C++20)
3. **Missing pthread**: Add `pthread` to link libraries on Unix
4. **Library Order**: GTest libraries must come AFTER the code being tested

### Debugging Commands

```bash
# Check what's in libtfs.a
nm build/libtfs.a | grep -i "dirent\|memfs"

# Check GTest symbols
nm /path/to/libgtest.a | grep "HasFatalFailure"

# Verbose linking
cd build && cmake --build . --target test_zip_backend -- VERBOSE=1 2>&1 | grep "Linking"
```

---

## Next Phase After This

Once tests link and run, proceed to:
1. **Integration Testing**: Verify actual DwarFS operations work
2. **Documentation**: Update README.adoc with v0.9+ details
3. **Cleanup**: Move temporary docs to old-docs/

**Estimated Total Remaining**: 4.25 hours
