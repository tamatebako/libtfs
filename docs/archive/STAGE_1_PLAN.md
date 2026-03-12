# Stage 1: Rename to libtebako-fs & Solidify Foundation

## Goal

Complete rename from libdwarfs-wr to libtebako-fs with rock-solid foundation using new dwarfs (cereal/bitsery, no thrift/folly).

**Duration:** 2 weeks
**Status:** 🎯 READY TO START

---

## Pre-Stage Checklist

### Verify Environment
- [ ] dwarfs at `~/src/external/dwarfs` has cereal/bitsery support
- [ ] All current code compiles
- [ ] All tests currently pass (baseline)
- [ ] Git repo clean (no uncommitted changes)

### Create Baseline
```bash
# Save current state
git tag baseline-pre-rename
git checkout -b stage-1-rename-libtebako-fs

# Document current test state
cd build
ctest --output-on-failure > ../baseline-tests.log
```

---

## Week 1: Rename & Reorganize

### Day 1: Repository & Project Rename

**Morning: Repository**
```bash
# On GitHub
# Rename: tamatebako/libdwarfs → tamatebako/libtebako-fs
# Settings → General → Repository name

# Update local
git remote set-url origin https://github.com/tamatebako/libtebako-fs.git
```

**Afternoon: CMakeLists.txt**
- [ ] Line 61: `project(libdwarfs-wr ...)` → `project(libtebako-fs ...)`
- [ ] Line 850: `add_library(dwarfs-wr ...)` → `add_library(tebako-fs ...)`
- [ ] Line 1329: Target name in install
- [ ] Update all `dwarfs-wr` references to `tebako-fs`
- [ ] Verify: `grep -r "dwarfs-wr" CMakeLists.txt` returns nothing

**Testing:**
```bash
mkdir build-test && cd build-test
cmake ..
# Check output: Should say "Project: libtebako-fs"
```

### Day 2: Header Reorganization

**Create New Structure:**
```bash
mkdir -p include/tebako/fs
mkdir -p include/tebako/fs/internal
```

**Move Headers:**
```bash
# Core headers
mv include/tebako-io.h include/tebako/fs/io.h
mv include/tebako-memfs.h include/tebako/fs/memfs.h
mv include/tebako-fd.h include/tebako/fs/internal/fd_table.h
mv include/tebako-mount-table.h include/tebako/fs/internal/mount_table.h
# ... etc for all headers
```

**Create Compatibility Wrappers:**
```bash
# include/tebako-io.h (deprecated)
echo '#warning "tebako-io.h is deprecated, use <tebako/fs/io.h>"' > include/tebako-io.h
echo '#include <tebako/fs/io.h>' >> include/tebako-io.h
```

### Day 3-4: Update Source Files

**Update all #includes:**
```cpp
// Old
#include <tebako-io.h>
#include <tebako-memfs.h>

// New
#include <tebako/fs/io.h>
#include <tebako/fs/memfs.h>
```

**Files to update:** All in `src/` directory

**Testing after each file:**
```bash
cd build-test
make -j$(nproc)
# Ensure it compiles
```

### Day 5: Update Tests & Examples

**Update test files:**
- `tests/*.cpp` - Update includes

**Update examples:**
- `examples/*.cpp` - Update includes
- `examples/README.md` - Update documentation

### Day 6-7: Documentation & Testing

**Update README.md:**
```markdown
# libtebako-fs

(Formerly libdwarfs-wr)

Tebako's virtual filesystem layer providing...
```

**Create CHANGELOG.md:**
```markdown
# Changelog

## [2.0.0] - 2025-11-XX

### Changed
- **BREAKING**: Renamed from libdwarfs-wr to libtebako-fs
- **BREAKING**: Headers moved to tebako/fs/ namespace
- Removed folly dependency (pure C++17)
- Configured dwarfs with cereal/bitsery (no thrift)

### Added
- Compatibility headers for smooth migration
- Comprehensive documentation

### Deprecated
- Old header names (will be removed in v3.0.0)
```

**Full regression testing:**
```bash
cd build-test
cmake -DWITH_TESTS=ON -DWITH_COVERAGE=ON ..
make -j$(nproc)
ctest --output-on-failure
# All tests must pass!
```

---

## Week 2: Verify New dwarfs Integration

### Day 8: dwarfs Configuration Verification

**Check dwarfs source:**
```bash
cd ~/src/external/dwarfs
git log --oneline -n 20
grep -r "DWARFS_WITH_CEREAL" cmake/
grep -r "DWARFS_WITH_BITSERY" cmake/
grep -r "DWARFS_WITH_THRIFT" cmake/
```

**Verify our CMakeLists.txt passes correct flags:**
- [ ] Line ~505: `-DTEBAKO_BUILD=ON`
- [ ] Line ~506: `-DDWARFS_WITH_THRIFT=OFF`
- [ ] Line ~507: `-DDWARFS_WITH_CEREAL=ON`
- [ ] Line ~508: `-DDWARFS_WITH_BITSERY=ON`

### Day 9: Build & Link Verification

**Clean build:**
```bash
rm -rf build
mkdir build && cd build
cmake -DWITH_TESTS=ON ..
make -j$(nproc) 2>&1 | tee build.log
```

**Verify output contains:**
```
-- Thrift serialization: DISABLED
-- Cereal serialization: ENABLED
-- Bitsery serialization: ENABLED
```

**Check symbols:**
```bash
# Should find NO folly or thrift symbols
nm -g libtebako-fs.a | grep -iE "folly|thrift"
# Expected: empty output

nm -g wr-bin | grep -iE "folly|thrift"
# Expected: empty output
```

### Day 10: Test Suite Execution

**Run all tests:**
```bash
cd build
ctest --output-on-failure --verbose | tee test-results.log
```

**Expected:** 100% pass rate

**If failures:**
1. Document failure
2. Investigate root cause
3. Fix issue
4. Retest
5. Repeat until all pass

### Day 11: Static Linking Verification

**Check static linkage:**
```bash
# Linux
ldd wr-bin
# Should show minimal dynamic deps (libc, libstdc++, etc.)
# Should NOT show libfolly, libthrift

# macOS
otool -L wr-bin
# Same expectations
```

**Test packaging:**
```bash
# Create minimal tebako-like scenario
# Embed libtebako-fs.a in test application
# Verify it links statically
```

### Day 12: Performance Baseline

**Benchmark current performance:**
```bash
# File open/read operations
time ./benchmark_file_ops 10000

# Directory traversal
time ./benchmark_dir_scan

# Mount/unmount
time ./benchmark_mount
```

**Document baseline:** These numbers are reference for Stage 2-3

### Day 13: Cross-Platform Testing

**Test on all platforms:**
- [ ] Ubuntu 20.04 (GCC)
- [ ] Ubuntu 22.04 (Clang)
- [ ] macOS 12 (AppleClang)
- [ ] macOS 13 (AppleClang)
- [ ] Windows MSVC (if applicable)
- [ ] Alpine Linux (musl)

**For each platform:**
1. Clean build
2. Run tests
3. Verify no folly/thrift
4. Document results

### Day 14: Release Preparation

**Final checks:**
- [ ] All tests pass on all platforms
- [ ] Documentation complete
- [ ] Examples work
- [ ] CHANGELOG.md updated
- [ ] Version bumped to 2.0.0

**Release:**
```bash
git tag v2.00
git push origin v2.0.0
```

**Update Tebako:**
- [ ] Update Tebako to use libtebako-fs v2.0
- [ ] Test Tebako builds successfully
- [ ] Verify Ruby packaging still works

---

## Rollback Plan

### If Critical Issues Found

**Minor issues** (<3 test failures):
- Fix immediately
- Continue

**Major issues** (crashes, data corruption):
```bash
git checkout baseline-pre-rename
git branch stage-1-fixes
# Fix issues
# Restart Stage 1
```

---

## Success Criteria

### Must Achieve

- [ ] Repository renamed: tamatebako/libtebako-fs
- [ ] All files reference new name
- [ ] Headers reorganized: `include/tebako/fs/`
- [ ] 100% tests passing
- [ ] Zero folly/thrift symbols
- [ ] Static linking verified
- [ ]Cross-platform validated
- [ ] Tebako integration working
- [ ] Documentation updated
- [ ] v2.0.0 released

### Quality Gates

Each must pass before proceeding:
1. **Compilation** - Clean build, zero errors
2. **Tests** - 100% pass rate
3. **Symbols** - No folly/thrift leakage
4. **Performance** - No regression
5. **Integration** - Tebako still works

---

## Deliverables

### Code
- ✅ Renamed repository
- ✅ Reorganized headers
- ✅ Updated all source files
- ✅ libtebako-fs v2.0.0 tagged

### Documentation
- ✅ Updated README.md
- ✅ Created CHANGELOG.md
- ✅ MASTER_PLAN.md (this file)
- ✅ ARCHITECTURE.md (kept)
- ✅ Cleaned up obsolete docs

### Validation
- ✅ All tests passing
- ✅ Cross-platform verified
- ✅ Tebako integration confirmed

---

## After Stage 1

**Outcome:** libtebako-fs v2.0 - A solid, well-tested foundation

**Ready for:**
- Stage 2: ZIP backend
- Stage 3: Multi-language support

**NOT ready for:** Production use in new languages (need Stage 3)

---

## Questions & Decisions

### Q1: Keep backward compatibility?
**Decision:** YES - compatibility headers for 6-12 months

### Q2: Major version bump?
**Decision:** YES - v2.0.0 (breaking changes in header paths)

### Q3: Update Tebako immediately?
**Decision:** YES - Update Tebako in same release to use v2.0

### Q4: Deprecation period?
**Decision:** Old headers deprecated but functional until v3.0.0 (6-12 months)

---

## Ready to Start

**Prerequisites met:**
- [x] Master plan created ([`MASTER_PLAN.md`](docs/MASTER_PLAN.md))
- [x] Stage 1 plan detailed (this file)
- [x] Folly removal complete
- [ ] Baseline tests documented
- [ ] Workspace clean

**Next step:** Execute Day 1 tasks (repository rename, CMakeLists.txt update)