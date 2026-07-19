# DwarFS v0.9+ API Fixes - Continuation Prompt

**Priority**: P1 (Blocks Testing)
**Estimated Time**: 1-2 hours
**Status**: Ready to Start

---

## Context

The DwarFS v0.9+ build system issues have been resolved through clean architectural refactoring. The remaining compilation errors are **DwarFS API compatibility issues** in source files that need to be updated to use the new v0.9+ API patterns.

**What's Complete** ✅:
- Fixed all build system / header visibility issues
- Implemented clean buffer-based `tebako_dirent` structure
- Removed dependency on system struct internals
- All forward declarations updated

**What Remains** ⚠️:
- 20 errors in `src/tebako-memfs.cpp` (DwarFS API usage)
- 3 errors in `src/tebako-fd.cpp` (variable scoping)
- 1 error in `src/tebako-io-root.cpp` (minor)

---

## Task 1: Fix tebako-memfs.cpp (45 minutes)

### Error 1: inode_view Type Not Found

**Current**:
```cpp
int memfs::dwarfs_file_stat(inode_view& inode, struct stat* st)
```

**Fix**: Add proper namespace qualification
```cpp
int memfs::dwarfs_file_stat(dwarfs::reader::inode_view& inode, struct stat* st)
```

### Error 2: copy_file_stat Not Found

**Current**:
```cpp
copy_file_stat<true>(st, dwarfs_file_stat);
```

**Investigation Needed**: Check DwarFS v0.9+ API for equivalent function
- Look in `/Users/mulgogi/src/external/dwarfs/include/dwarfs/reader/`
- Search for file_stat conversion functions
- Update to new API pattern

### Error 3: dir_entry_view Decomposition

**Current**:
```cpp
auto [entry, name_view] = *res;
```

**Issue**: `dir_entry_view` only decomposes into 1 element in v0.9+

**Fix**: Use accessor methods instead:
```cpp
auto entry = *res;
auto name_view = entry.name();  // Or appropriate accessor
```

**Action Steps**:
1. Read [`src/tebako-memfs.cpp`](../src/tebako-memfs.cpp) lines 1-100
2. Check DwarFS v0.9+ headers for correct API patterns
3. Use [`edit_file`](../src/tebako-memfs.cpp) to fix each issue
4. Build and verify: `cd build && cmake --build . -j8 2>&1 | grep tebako-memfs`

---

## Task 2: Fix tebako-fd.cpp (15 minutes)

### Error: Undeclared Variables

**Location**: Line 145
```cpp
switch (dwarfs_inode_relative_stat(stfd.st_ino, path, &fd->st, lnk, (flags & O_NOFOLLOW) == 0))
```

**Issue**: `flags`, `path`, `lnk` not in scope

**Investigation**:
1. Read [`src/tebako-fd.cpp`](../src/tebako-fd.cpp) lines 130-160
2. Check function signature for these parameters
3. Ensure variables are properly declared/passed

**Likely Fix**:
- Variables should be parameters or declared earlier in function
- May need to pass through from calling function

---

## Task 3: Fix tebako-io-root.cpp (5 minutes)

Only 1 error remaining - likely similar to above issues.

**Action**:
1. Read last 30 lines of build output: `tail -30 build/struct_forward_decl_build.log`
2. Identify the specific error
3. Apply similar fix pattern

---

## Task 4: Final Build & Validation (15 minutes)

### Complete Clean Build
```bash
cd build
rm -rf CMakeFiles/ CMakeCache.txt
cmake ..
cmake --build . -j8 2>&1 | tee final_dwarfs_v09_build.log
```

### Success Criteria
- ✅ Zero compilation errors
- ✅ Zero warnings (or document any remaining)
- ✅ All targets built: `libtfs.a`, `test_c_api`, `tebakofs`

### If Build Succeeds
Proceed to testing phase (see original continuation prompt)

---

## Debugging Strategy

If stuck on an error:

1. **Check DwarFS v0.9+ documentation**:
   ```bash
   find /Users/mulgogi/src/external/dwarfs/include -name "*.h" | xargs grep -l "function_name"
   ```

2. **Compare with DwarFS examples**:
   ```bash
   find /Users/mulgogi/src/external/dwarfs/examples -name "*.cpp"
   ```

3. **Search for API migration patterns**:
   ```bash
   cd /Users/mulgogi/src/external/dwarfs
   git log --all --grep="API" --oneline | head -20
   ```

---

## Remember

- Use [`edit_file`](../src/tebako-memfs.cpp) for all code changes (not shell commands)
- Fix one error at a time
- Build after each fix to verify
- The architecture is correct - these are just API usage updates

---

## Estimated Timeline

| Task | Duration | Cumulative |
|------|----------|------------|
| Fix tebako-memfs.cpp | 45 min | 0:45 |
| Fix tebako-fd.cpp | 15 min | 1:00 |
| Fix tebako-io-root.cpp | 5 min | 1:05 |
| Final build & validation | 15 min | 1:20 |

**Total**: 1 hour 20 minutes