# DwarFS Integration Refactoring Plan

**Date**: 2025-12-22
**Priority**: P0 (Blocks All Testing)
**Estimated Time**: 2-4 hours

---

## Current Status

### ✅ Completed (Build Path Fixes)
- Fixed dwarfs reader include paths in CMakeLists.txt
- Fixed filesystem_v2.h namespace (dwarfs::reader::filesystem_v2)
- Fixed metadata_v2.h path (dwarfs/reader/internal/metadata_v2.h)
- Fixed DIR type visibility in dir-io.cpp
- Fixed dirent.h missing C++ headers (map, memory)
- Removed non-existent dwarfs headers (mmap.h, util.h, options.h)

### ❌ Critical Blocker
**dwarfs::mmif API has been removed from dwarfs v0.9+**

The tebako::mfs class inherits from dwarfs::mmif, which no longer exists. This blocks:
- All compilation
- All testing
- Memory mounting feature validation

### 📁 Affected Files
- `include/tebako-mfs.h` - Defines mfs class (inherits from mmif)
- `src/tebako-mfs.cpp` - Implements mfs class
- `src/tebako-memfs.cpp` - Uses mfs for memory-backed filesystem
- `src/tebako-io-root.cpp` - Creates mfs instances
- `src/tebako-memfs-table.cpp` - Manages mfs instances

---

## Architecture Analysis

### Current Design (Broken)
```
tebako::mfs : public dwarfs::mmif
  └── Provides memory interface for dwarfs::filesystem_v2
      └── Used by tebako::memfs to load from memory
```

### Problem
- `dwarfs::mmif` no longer exists in modern dwarfs
- Modern dwarfs uses different mechanism for memory-backed filesystems

### Solution Options

#### Option A: Use dwarfs Memory Stream (Recommended)
Modern dwarfs likely uses stream-based memory access instead of mmif interface.

**Pros**:
- Aligns with modern dwarfs architecture
- More flexible and performant
- Future-proof

**Approach**:
1. Research dwarfs::reader::filesystem_v2 constructor
2. Identify memory source requirements
3. Create new tebako memory adapter
4. Remove mmif dependency entirely

#### Option B: Recreate mmif Interface Locally
Create our own mmif-like interface if dwarfs still needs it.

**Pros**:
- Minimal code changes
- Familiar pattern

**Cons**:
- May not align with modern dwarfs design
- Maintenance burden

---

## Implementation Plan

### Phase 1: Research (30 min)
**Goal**: Understand modern dwarfs memory interface

1. **Examine filesystem_v2 constructors**
   ```bash
   grep -A20 "class filesystem_v2" /path/to/dwarfs/include/dwarfs/reader/filesystem_v2.h
   ```

2. **Find memory source examples**
   ```bash
   find /path/to/dwarfs -name "*.cpp" -o -name "*.h" | xargs grep -l "memory.*source"
   ```

3. **Check filesystem_options**
   - What parameters does modern filesystem_v2 need?
   - How to provide memory buffer?

4. **Document findings** in `docs/DWARFS_V09_MEMORY_INTERFACE.md`

### Phase 2: Design New Memory Adapter (15 min)
**Goal**: Design tebako memory interface for modern dwarfs

1. **Create header**: `include/tebako/fs/memory_source.h`
   ```cpp
   namespace tebako {
   
   class memory_source {
    public:
     memory_source(const void* data, size_t size);
     ~memory_source() = default;
     
     // Interface methods based on dwarfs requirements
     const void* data() const { return data_; }
     size_t size() const { return size_; }
     
    private:
     const void* data_;
     size_t size_;
   };
   
   }  // namespace tebako
   ```

2. **Update memfs.h** to use new interface
   - Remove dwarfs::filesystem_options if obsolete
   - Add proper initialization for filesystem_v2

### Phase 3: Implement Memory Adapter (30 min)
**Goal**: Implement new memory interface

1. **Create implementation**: `src/memory_source.cpp`

2. **Update tebako-memfs.cpp**:
   - Replace mfs usage with memory_source
   - Update filesystem_v2 initialization
   - Ensure zero-copy design maintained

3. **Update tebako-io-root.cpp**:
   - Replace mfs instantiation
   - Update memfs_table usage

4. **Update tebako-memfs-table.cpp**:
   - Change from `std::shared_ptr<mfs>` to `std::shared_ptr<memory_source>`
   - Update table management

5. **Remove old files**:
   - Delete `include/tebako-mfs.h`
   - Delete `src/tebako-mfs.cpp`
   - Update CMakeLists.txt

### Phase 4: Build & Fix Compilation (30 min)
**Goal**: Achieve clean compilation

1. **Incremental build**:
   ```bash
   cd build
   cmake --build . --target tfs -j8 2>&1 | tee build.log
   ```

2. **Fix remaining issues**:
   - Namespace corrections
   - Missing includes
   - Type mismatches

3. **Verify all targets build**:
   ```bash
   cmake --build . -j8
   ```

### Phase 5: Test & Validate (1-2 hours)
**Goal**: Validate memory mounting works correctly

1. **Run C API tests**:
   ```bash
   ./test_c_api
   ```

2. **Run memory mounting tests**:
   ```bash
   ./test_c_api --gtest_filter="*Memory*"
   ```

3. **Memory leak check**:
   ```bash
   leaks --atExit -- ./test_c_api --gtest_filter="*Memory*"
   ```

4. **Thread safety stress test**:
   ```bash
   ./test_c_api --gtest_filter="*Memory*" --gtest_repeat=100 --gtest_shuffle
   ```

5. **Document results** in `docs/MEMORY_MOUNTING_VALIDATION_RESULTS.md`

---

## Success Criteria

- [ ] Clean compilation of all targets
- [ ] All 57 C API tests pass
- [ ] All 6 memory mounting tests pass
- [ ] Zero memory leaks detected
- [ ] Thread safety validated
- [ ] Documentation updated

---

## Risk Mitigation

### Risk: Incompatible dwarfs API
**Mitigation**: May need to downgrade dwarfs or use compatibility layer

### Risk: Performance regression
**Mitigation**: Benchmark before/after, optimize if needed

### Risk: Breaking existing functionality
**Mitigation**: Comprehensive test coverage, careful refactoring

---

## Next Steps After Completion

1. Update README.adoc with memory mounting documentation
2. Archive temporary documentation to old-docs/
3. Create production readiness checklist
4. Performance benchmark against file-based mounting

---

**Document Version**: 1.0
**Owner**: Development Team
**Status**: Ready for Implementation