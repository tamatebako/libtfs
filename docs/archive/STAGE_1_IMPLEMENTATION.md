# Stage 1 Implementation: Rename & Solidify

**Duration**: 2 weeks
**Status**: Ready to execute
**Goal**: Rename to libtfs with clean, modern foundation

---

## Overview

Transform libdwarfs-wr into libtfs with:
- Clean repository and code naming
- Organized header structure
- Updated build system
- Comprehensive testing
- v2.0.0 release

**No backward compatibility** - This is a clean break.

---

## Pre-Stage Checklist

### Environment Verification
- [ ] Git repo clean (no uncommitted changes)
- [ ] All current tests passing
- [ ] Dwarfs at `/Users/mulgogi/src/external/dwarfs` accessible
- [ ] Build succeeds with current configuration

### Preparation
```bash
# Save current state
cd /Users/mulgogi/src/tamatebako/libdwarfs
git tag baseline-pre-rename-$(date +%Y%m%d)

# Create feature branch
git checkout -b stage-1-modernize-libtfs

# Run baseline tests
mkdir -p build && cd build
cmake -DTEBAKO_BUILD=ON -DWITH_TESTS=ON ..
make -j$(nproc)
ctest --output-on-failure > ../baseline-tests.log 2>&1
```

---

## Week 1: Rename & Reorganize

### Day 1: Repository & Project Rename

#### Morning: GitHub Repository
```bash
# On GitHub UI:
# Settings → Repository name → Change to "libtfs"
# URL becomes: https://github.com/tamatebako/libtfs

# Update local remote
git remote set-url origin https://github.com/tamatebako/libtfs.git
git remote -v  # Verify
```

#### Afternoon: CMakeLists.txt Updates
**File**: `CMakeLists.txt`

Changes needed:
1. Line ~34: `project(dwarfs ...)` → `project(libtfs ...)`
2. Search/replace all `dwarfs-wr` → `tfs`
3. Search/replace all `libdwarfs-wr` → `libtfs`
4. Update any hardcoded paths

Test:
```bash
cd build
cmake ..
# Verify output shows "Project: libtfs"
```

### Day 2-3: Header Reorganization

#### Create New Structure
```bash
mkdir -p include/tebako/fs
mkdir -p include/tebako/fs/internal
```

#### Move Headers (preserve for now, will clean up after testing)
```bash
# Core public headers
cp include/tebako-io.h include/tebako/fs/io.h
cp include/tebako-memfs.h include/tebako/fs/memfs.h
cp include/tebako-common.h include/tebako/fs/common.h
cp include/tebako-dirent.h include/tebako/fs/dirent.h

# Internal headers
cp include/tebako-fd.h include/tebako/fs/internal/fd_table.h
cp include/tebako-mount-table.h include/tebako/fs/internal/mount_table.h
cp include/tebako-memfs-table.h include/tebako/fs/internal/memfs_table.h
cp include/tebako-kfd.h include/tebako/fs/internal/kfd.h

# Utility headers
cp include/tebako-synchronized.h include/tebako/fs/util/synchronized.h
cp include/tebako-conversions.h include/tebako/fs/util/conversions.h
```

#### Update Include Paths
All source files in `src/` need updates:
```cpp
// Old
#include <tebako-io.h>
#include <tebako-memfs.h>

// New
#include <tebako/fs/io.h>
#include <tebako/fs/memfs.h>
```

Test after each batch of files.

### Day 4: Source File Updates

Update all `#include` statements in:
- `src/*.cpp` (all files)
- `tests/*.cpp` (all test files)
- `examples/*.cpp`

Test build after each batch.

### Day 5: Documentation Updates

#### Update README.md
- Change all "libdwarfs-wr" → "libtfs"
- Update build instructions
- Update repository URLs
- Add v2.0.0 note about breaking changes

#### Create CHANGELOG.md
```markdown
# Changelog

## [2.0.0] - 2025-01-XX

### Changed - BREAKING
- Renamed from libdwarfs-wr to libtfs
- Headers moved to tebako/fs/ namespace
- CMake project name changed
- FlatBuffers now primary serialization (no cereal/bitsery)

### Removed
- Folly dependency (pure C++17)
- Backward compatibility headers

### Fixed
- Static linking compatibility
- Header organization

## [1.x.x] - Historical
See git history for previous versions.
```

---

## Week 2: Testing & Release

### Day 6-7: Comprehensive Testing

#### Build Tests
```bash
# Clean build
rm -rf build
mkdir build && cd build

# Configure
cmake -DTEBAKO_BUILD=ON \
      -DDWARFS_WITH_THRIFT=OFF \
      -DDWARFS_WITH_FLATBUFFERS=ON \
      -DWITH_TESTS=ON \
      ..

# Build
make -j$(nproc) 2>&1 | tee build.log

# Verify no folly/thrift
nm -g libtfs.a | grep -iE "folly|thrift"
# Expected: empty output

# Run tests
ctest --output-on-failure --verbose | tee test-results.log
```

#### Cross-Platform
Test on:
- [ ] Ubuntu 22.04
- [ ] macOS arm64
- [ ] macOS x86_64
- [ ] Alpine Linux

### Day 8: Integration Testing

Test with actual Tebako:
```bash
# In tebako repo
# Update to use libtfs v2.0
# Build tebako
# Run tebako packaging tests
# Verify Ruby apps work
```

### Day 9: Performance Baseline

Run benchmarks, compare with baseline:
```bash
# File operations benchmark
# Mount/unmount timing
# Memory usage profiling
```

Document any regressions.

### Day 10: Release

#### Final Checks
- [ ] All tests passing
- [ ] Documentation updated
- [ ] CHANGELOG.md complete
- [ ] No build warnings
- [ ] Static linking verified

#### Release Process
```bash
# Tag release
git tag -a v2.0.0 -m "Release v2.0.0: Rename to libtfs"
git push origin v2.0.0

# GitHub release
# - Upload artifacts
# - Add release notes from CHANGELOG
```

---

## Success Criteria

- [ ] Repository renamed to libtfs
- [ ] All CMake references updated
- [ ] Headers in `include/tebako/fs/`
- [ ] All source files updated
- [ ] Documentation current
- [ ] 100% tests passing
- [ ] Tebako integration working
- [ ] v2.0.0 tagged

---

## Rollback Plan

If critical issues found:
```bash
git checkout baseline-pre-rename-$(date +%Y%m%d)
git branch stage-1-fixes
# Fix issues
# Retry Stage 1
```

---

## Next Stage

After completion: [STAGE_2_IMPLEMENTATION.md](STAGE_2_IMPLEMENTATION.md)