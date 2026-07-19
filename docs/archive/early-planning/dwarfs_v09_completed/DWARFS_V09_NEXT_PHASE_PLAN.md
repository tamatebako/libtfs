# DwarFS v0.9+ Integration - Next Phase Plan

**Date**: 2025-12-24  
**Phase**: Post-API-Fixes  
**Priority**: P1 (Test Infrastructure)

---

## Phase Status

✅ **Phase 1 COMPLETE**: DwarFS v0.9+ API Migration  
🔄 **Phase 2 CURRENT**: Test Infrastructure Fixes  
⏭️ **Phase 3 NEXT**: Integration Testing & Validation

---

## Phase 2: Test Infrastructure Fixes

### Issue: GTest Linking Failures

**Symptoms**:
```
ld: symbol(s) not found for architecture arm64:
  "testing::Test::HasFatalFailure()"
  "testing::Test::HasNonfatalFailure()"
  ...
```

**Root Cause**: GTest library version mismatch or incorrect linking configuration

**Impact**: 
- ❌ Test executables fail to link
- ✅ Main library (`libtfs.a`) compiles perfectly
- ✅ Core functionality unaffected

### Tasks

1. **Investigate GTest Configuration** (15 min)
   - Check GTest version in vcpkg/system
   - Verify `find_package(GTest)` configuration
   - Check `${GTestMain}` and `${GTEST_LDFLAGS}` variables

2. **Fix GTest Linking** (30 min)
   - Update GTest linkage in CMakeLists.txt
   - Ensure proper library order
   - Verify against both static and shared GTest

3. **Verify All Tests Build** (15 min)
   - `test_backend_factory`
   - `test_zip_backend`
   - `test_zip_integration`
   - `test_c_api`

4. **Run Test Suite** (30 min)
   - Execute all tests
   - Document any failures
   - Update test expectations if needed

**Total Estimated Time**: 1.5 hours

---

## Phase 3: Integration Testing & Validation

### Objectives

1. **Functional Validation**
   - Verify DwarFS filesystem operations
   - Test memory mounting/unmounting
   - Validate file I/O operations
   - Test directory traversal

2. **Backend Integration**
   - ZIP backend functionality
   - Multiple memfs mounting
   - Mount point handling

3. **C API Validation**
   - Verify all C API functions work
   - Test error handling
   - Validate resource cleanup

### Test Scenarios

#### Scenario 1: Basic DwarFS Operations
```cpp
// Mount DwarFS image
int index = mount_root_memfs(data, size, ...);

// Stat file
struct stat st;
int ret = dwarfs_stat("/__tebako_memfs__/file.txt", &st, ...);

// Read file
char buffer[1024];
ssize_t bytes = dwarfs_inode_read(st.st_ino, buffer, sizeof(buffer), 0);

// Unmount
unmount_root_memfs();
```

#### Scenario 2: Multiple Memfs Mounting
```cpp
// Mount additional memfs at subdirectory
int index2 = mount_memfs(data2, size2, "auto", root_inode, "subdir");

// Access nested file
dwarfs_stat("/__tebako_memfs__/subdir/file.txt", &st, ...);
```

#### Scenario 3: ZIP Backend
```cpp
// Open ZIP archive
auto backend = ZipBackend::create();
backend->mount("test.zip");

// Access ZIP contents
auto handle = backend->open("/file.txt");
```

**Total Estimated Time**: 2 hours

---

## Phase 4: Documentation & Cleanup

### Tasks

1. **Update README.adoc** (30 min)
   - Document DwarFS v0.9+ support
   - Update API examples
   - Add architecture diagrams

2. **Technical Documentation** (30 min)
   - Document struct dirent architectural solution
   - Update API reference
   - Document breaking changes from v0.8

3. **Move Outdated Docs** (15 min)
   - Move all `DWARFS_V09_*` temporary docs to `old-docs/dwarfs_v09_completed/`
   - Keep only completion status in main docs/
   - Update doc index

**Total Estimated Time**: 1.25 hours

---

## Phase 5: Production Readiness

### Checklist

- [ ] All tests passing
- [ ] Zero compilation warnings
- [ ] Memory leaks checked (valgrind/ASAN)
- [ ] Documentation complete
- [ ] Examples working
- [ ] CI/CD updated (if applicable)

**Total Estimated Time**: 1 hour

---

## Overall Timeline

| Phase | Duration | Status |
|-------|----------|--------|
| Phase 1: API Fixes | ✅ Complete | DONE |
| Phase 2: Test Infrastructure | 1.5 hours | NEXT |
| Phase 3: Integration Testing | 2 hours | Pending |
| Phase 4: Documentation | 1.25 hours | Pending |
| Phase 5: Production Readiness | 1 hour | Pending |
| **TOTAL** | **5.75 hours** | **26% Complete** |

---

## Critical Path

```
API Fixes (DONE) → Test Infrastructure → Integration Testing → Production
                                ↓
                         Documentation (parallel)
```

**Next Action**: Fix GTest linking to unblock testing phases.
