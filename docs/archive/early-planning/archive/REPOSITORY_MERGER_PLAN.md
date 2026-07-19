# Repository Merger Plan: libtfs → tebako

**Date**: 2025-12-22  
**Status**: Architectural Plan  
**Purpose**: Detailed plan for merging libtfs into tebako repository as `lib/tfs/`

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Rationale](#rationale)
3. [Current Structure Analysis](#current-structure-analysis)
4. [Proposed Repository Structure](#proposed-repository-structure)
5. [Git History Preservation](#git-history-preservation)
6. [Migration Steps](#migration-steps)
7. [CMake Integration](#cmake-integration)
8. [CI/CD Pipeline Updates](#cicd-pipeline-updates)
9. [Documentation Updates](#documentation-updates)
10. [Dependency Management](#dependency-management)
11. [Benefits vs Risks](#benefits-vs-risks)
12. [Timeline and Milestones](#timeline-and-milestones)
13. [Rollback Plan](#rollback-plan)
14. [Testing Strategy](#testing-strategy)

---

## Executive Summary

### Objective

Merge the libtfs repository into the tebako repository as a subdirectory at `tebako/lib/tfs/`, establishing a monorepo structure that simplifies development and maintenance of tightly coupled components.

### Key Requirements

1. ✅ **Preserve Git history** - All commits, tags, and branches from libtfs must be preserved
2. ✅ **Maintain library independence** - libtfs must remain buildable standalone for testing
3. ✅ **Clean integration** - CMake structure must support both monorepo and standalone builds
4. ✅ **No disruption** - Existing tebako functionality must continue working during transition
5. ✅ **Clear ownership** - Separation of concerns between tebako and libtfs code

### Timeline

- **Phase 1** (Week 1): Git history migration and structure setup
- **Phase 2** (Week 2): CMake integration and build system updates
- **Phase 3** (Week 3): CI/CD pipeline migration
- **Phase 4** (Week 4): Documentation and final validation

---

## Rationale

### Why Merge?

1. **Tight Coupling**: libtfs was specifically designed for tebako's use case
2. **Atomic Commits**: Changes affecting both repos can be committed atomically
3. **Simplified Development**: No need to coordinate releases across two repos
4. **Easier Versioning**: Single version number for the entire stack
5. **Reduced Overhead**: Single CI/CD pipeline, single issue tracker
6. **Dependency Management**: Unified vcpkg configuration

### Why Monorepo Structure?

1. **Proven Pattern**: Used by major projects (LLVM, React, Angular, etc.)
2. **Clear Boundaries**: Physical separation via `lib/tfs/` directory
3. **Flexible Build**: Can build libtfs independently or as part of tebako
4. **Easier Refactoring**: Atomic changes across library boundaries

---

## Current Structure Analysis

### Current libtfs Repository

Location: `/Users/mulgogi/src/tamatebako/libdwarfs`

```
libdwarfs/
├── .github/workflows/          # CI/CD workflows
├── CMakeLists.txt             # Root CMake file
├── vcpkg.json                 # Dependencies
├── README.adoc                # Main documentation
├── CHANGELOG.md               # Version history
├── docs/                      # Documentation
│   ├── ARCHITECTURE.md
│   ├── TESTING.adoc
│   ├── backends/
│   │   ├── ZIP_BACKEND.adoc
│   │   └── SQUASHFS_BACKEND.adoc
│   └── [other docs]
├── include/                   # Public headers
│   ├── tebako-*.h            # Legacy headers
│   └── tebako/fs/            # New VFS headers
│       ├── backend_factory.h
│       ├── filesystem.h
│       ├── backends/
│       │   ├── zip_backend.h
│       │   └── squashfs_backend.h
│       └── [other headers]
├── src/                       # Implementation
│   ├── backend_factory.cpp
│   ├── backends/
│   │   ├── zip_backend.cpp
│   │   └── squashfs_backend.cpp
│   ├── cli/                  # CLI tool
│   │   └── tebakofs.cpp
│   └── [other sources]
├── tests/                     # Test suite
│   ├── test_backend_factory.cpp
│   ├── test_zip_backend.cpp
│   ├── test_squashfs_backend.cpp
│   └── fixtures/
└── examples/                  # Example programs
    ├── basic_usage.cpp
    └── api_example.cpp
```

### Current tebako Repository

Location: `/Users/mulgogi/src/tamatebako/tebako`

```
tebako/
├── .github/workflows/         # CI/CD workflows
├── CMakeLists.txt            # Root CMake file
├── Gemfile                   # Ruby dependencies
├── tebako.gemspec            # Gem specification
├── README.adoc               # Main documentation
├── bin/                      # Ruby CLI scripts
├── lib/                      # Ruby library code
├── exe/                      # Ruby executables
├── src/                      # C++ shim code
│   ├── tebako-main.cpp
│   └── tebako-fs.cpp
├── include/                  # C++ headers
│   └── tebako/
│       ├── tebako-main.h
│       └── tebako-fs.h
├── cmake/                    # CMake modules
├── tests/                    # Test suite
├── spec/                     # RSpec tests
└── tools/                    # Build tools
```

---

## Proposed Repository Structure

### After Merger

```
tebako/
├── .github/workflows/         # UNIFIED CI/CD workflows
├── CMakeLists.txt            # Root CMake (updated)
├── Gemfile                   # Ruby dependencies
├── tebako.gemspec            # Gem specification (updated)
├── vcpkg.json                # MERGED dependencies
├── README.adoc               # Main documentation (updated)
├── CHANGELOG.md              # MERGED changelog
│
├── bin/                      # Ruby CLI scripts
├── lib/                      # Ruby library code
│   └── tfs/                  # ← MERGED libtfs
│       ├── CMakeLists.txt    #    Standalone build support
│       ├── vcpkg.json        #    Independent dependencies
│       ├── README.adoc       #    libtfs documentation
│       ├── CHANGELOG.md      #    libtfs changelog
│       ├── docs/             #    libtfs docs
│       │   ├── ARCHITECTURE.md
│       │   ├── TESTING.adoc
│       │   └── backends/
│       ├── include/          #    Public headers
│       │   ├── tebako-*.h   #    Legacy headers
│       │   └── tebako/fs/   #    VFS headers
│       ├── src/              #    Implementation
│       │   ├── backend_factory.cpp
│       │   ├── backends/
│       │   ├── cli/
│       │   └── [sources]
│       ├── tests/            #    Test suite
│       │   ├── CMakeLists.txt
│       │   └── [tests]
│       └── examples/         #    Examples
│
├── exe/                      # Ruby executables
├── src/                      # Tebako C++ shim code
├── include/                  # Tebako C++ headers
├── cmake/                    # CMake modules
├── tests/                    # Tebako tests
├── spec/                     # RSpec tests
├── tools/                    # Build tools
└── docs/                     # MERGED documentation
    ├── REPOSITORY_MERGER_PLAN.md (this file)
    ├── TEBAKO_INTEGRATION_ARCHITECTURE.md
    └── [other docs]
```

### Key Changes

1. **libtfs location**: Everything from libtfs goes into `lib/tfs/`
2. **Independent build**: `lib/tfs/CMakeLists.txt` supports standalone builds
3. **Unified vcpkg**: Root `vcpkg.json` merges both dependency lists
4. **Merged CI/CD**: Single `.github/workflows/` with combined pipelines
5. **Documentation**: `lib/tfs/docs/` for library-specific, root `docs/` for integration

---

## Git History Preservation

### Strategy: Git Subtree Merge

We will use Git's subtree merge strategy to preserve full commit history.

### Step-by-Step Commands

```bash
# 1. Navigate to tebako repository
cd /Users/mulgogi/src/tamatebako/tebako

# 2. Add libtfs as remote
git remote add libtfs-origin /Users/mulgogi/src/tamatebako/libdwarfs
git fetch libtfs-origin

# 3. Create merge branch
git checkout -b merge-libtfs

# 4. Merge with --allow-unrelated-histories
git merge -s ours --no-commit --allow-unrelated-histories libtfs-origin/main

# 5. Read libtfs tree into lib/tfs/
git read-tree --prefix=lib/tfs/ -u libtfs-origin/main

# 6. Commit the merge
git commit -m "Merge libtfs repository into lib/tfs/

This commit preserves the full Git history of the libtfs project
by merging it as a subtree at lib/tfs/.

libtfs repository: https://github.com/tamatebako/libtfs
Last commit: $(git rev-parse libtfs-origin/main)

The library remains buildable independently while also being
integrated into the tebako monorepo.
"

# 7. Verify history preservation
git log --follow lib/tfs/README.adoc
# Should show full libtfs commit history

# 8. Clean up remote
git remote remove libtfs-origin
```

### Verification

```bash
# Check that history is preserved
cd lib/tfs
git log --oneline | head -20
# Should show libtfs commits

# Check file annotations
git blame README.adoc
# Should show original libtfs authors

# Check tags (if needed)
git tag -l
# libtfs tags should be visible
```

---

## Migration Steps

### Phase 1: Repository Preparation (Week 1)

#### Day 1-2: Pre-migration Setup

**Tasks**:
1. Create backup branches in both repositories
2. Document current dependency versions
3. Tag current releases
4. Notify team of upcoming changes

**Commands**:
```bash
# In libtfs
cd /Users/mulgogi/src/tamatebako/libdwarfs
git tag pre-merger-backup
git checkout -b pre-merger-backup-branch
git push origin pre-merger-backup pre-merger-backup-branch

# In tebako
cd /Users/mulgogi/src/tamatebako/tebako
git tag pre-merger-backup
git checkout -b pre-merger-backup-branch
git push origin pre-merger-backup pre-merger-backup-branch
```

#### Day 3-4: Git History Migration

**Tasks**:
1. Execute git subtree merge (see commands above)
2. Verify history preservation
3. Test that libtfs files are accessible

**Validation**:
```bash
# Verify all files present
cd lib/tfs
ls -la
# Should match libtfs repository structure

# Verify history
git log --follow lib/tfs/README.adoc | wc -l
# Should show significant commit count

# Check specific libtfs commit
git show <libtfs-commit-hash>
# Should display correctly
```

#### Day 5: Directory Structure Finalization

**Tasks**:
1. Create `lib/tfs/CMakeLists.txt` with dual-mode support
2. Update paths in libtfs files to reference new location
3. Commit changes

### Phase 2: CMake Integration (Week 2)

#### Day 1-2: CMake Dual-Mode Setup

Create `lib/tfs/CMakeLists.txt` that supports both modes:

```cmake
# lib/tfs/CMakeLists.txt
cmake_minimum_required(VERSION 3.24)

# Determine if built standalone or as subdirectory
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build
    set(TFS_STANDALONE ON)
    project(libtfs VERSION 2.0.0 LANGUAGES CXX)
    
    # Setup vcpkg for standalone
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        message(FATAL_ERROR "CMAKE_TOOLCHAIN_FILE must be set for standalone build")
    endif()
else()
    # Built as part of tebako
    set(TFS_STANDALONE OFF)
endif()

# Library definition
add_library(tfs STATIC
    src/backend_factory.cpp
    src/backends/zip_backend.cpp
    src/backends/squashfs_backend.cpp
    # ... other sources
)

# Include directories
target_include_directories(tfs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Find dependencies
find_package(libzip CONFIG REQUIRED)
find_package(squashfs-tools-ng CONFIG REQUIRED)
find_package(argtable3 CONFIG REQUIRED)

target_link_libraries(tfs PUBLIC
    libzip::zip
    squashfs-tools-ng::squashfs
)

# Tests (only in standalone or if requested)
if(TFS_STANDALONE OR WITH_TFS_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# CLI tool
if(TFS_STANDALONE OR BUILD_TFS_CLI)
    add_executable(tebakofs
        src/tebakofs_main.cpp
        src/cli/tebakofs.cpp
    )
    target_link_libraries(tebakofs PRIVATE tfs argtable3::argtable3)
    install(TARGETS tebakofs DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Installation
if(TFS_STANDALONE)
    install(TARGETS tfs DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
```

#### Day 3-4: Update Root CMakeLists.txt

Modify `/Users/mulgogi/src/tamatebako/tebako/CMakeLists.txt`:

```cmake
# Add after line 40 (after project definition)

# Option to build with integrated libtfs or system libtfs
option(USE_SYSTEM_LIBTFS "Use system-installed libtfs instead of bundled" OFF)

if(USE_SYSTEM_LIBTFS)
    find_package(libtfs REQUIRED)
else()
    # Build bundled libtfs
    set(WITH_TFS_TESTS OFF CACHE BOOL "Disable libtfs tests in tebako build")
    set(BUILD_TFS_CLI OFF CACHE BOOL "Don't build tebakofs CLI in tebako build")
    add_subdirectory(lib/tfs)
endif()

# Replace lines 181-298 (DWARFS_WR external project) with:
# (keep for compatibility but make it optional)
if(DWARFS_PRELOAD OR USE_LEGACY_DWARFS)
    # Keep existing DWARFS_WR external project code
    # ... (existing lines 181-298)
else()
    # Use integrated libtfs
    set(__LIBDWARFS_WR "${CMAKE_CURRENT_BINARY_DIR}/lib/tfs/libtfs.a")
    # Alias for compatibility
    add_custom_target(${DWARFS_WR_PRJ})
    add_dependencies(${DWARFS_WR_PRJ} tfs)
endif()
```

#### Day 5: Build System Testing

**Tasks**:
1. Test standalone libtfs build
2. Test integrated tebako build
3. Verify all link dependencies resolve

**Test Commands**:
```bash
# Test 1: Standalone libtfs build
cd lib/tfs
mkdir build-standalone && cd build-standalone
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DWITH_TESTS=ON
cmake --build . -j$(nproc)
ctest --verbose
cd ../../..

# Test 2: Integrated tebako build
mkdir build-integrated && cd build-integrated
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DTEBAKO_VERSION=0.9.0 \
    -DSETUP_MODE=ON
cmake --build . --target setup
```

### Phase 3: CI/CD Pipeline Updates (Week 3)

#### Day 1-2: Workflow Consolidation

**Current Workflows** (libtfs):
- `.github/workflows/ubuntu.yml`
- `.github/workflows/macos.yml`
- `.github/workflows/alpine.yml`
- `.github/workflows/windows-msys.yml`
- `.github/workflows/lint.yml`
- `.github/workflows/codeql.yml`

**Current Workflows** (tebako):
- `.github/workflows/ubuntu.yml`
- `.github/workflows/macos.yml`
- `.github/workflows/alpine.yml`
- `.github/workflows/windows-msys.yml`
- `.github/workflows/lint-and-rspec.yml`

**Strategy**: Merge workflows while maintaining separation of concerns

Create `.github/workflows/libtfs.yml`:

```yaml
name: libtfs Tests

on:
  push:
    paths:
      - 'lib/tfs/**'
      - '.github/workflows/libtfs.yml'
  pull_request:
    paths:
      - 'lib/tfs/**'

jobs:
  test-standalone:
    name: Test libtfs (Standalone)
    strategy:
      matrix:
        os: [ubuntu-20.04, ubuntu-22.04, macos-14, windows-latest]
    runs-on: ${{ matrix.os }}
    
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0  # Full history for git log tests
      
      - name: Setup vcpkg
        run: |
          git clone https://github.com/Microsoft/vcpkg.git
          ./vcpkg/bootstrap-vcpkg.sh
      
      - name: Build libtfs
        working-directory: lib/tfs
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=$PWD/../../vcpkg/scripts/buildsystems/vcpkg.cmake \
            -DWITH_TESTS=ON
          cmake --build build -j$(nproc)
      
      - name: Run tests
        working-directory: lib/tfs/build
        run: ctest --verbose --output-on-failure
      
      - name: Upload coverage (Ubuntu only)
        if: matrix.os == 'ubuntu-20.04'
        uses: codecov/codecov-action@v3
        with:
          files: lib/tfs/build/coverage.xml
          flags: libtfs
```

Update `.github/workflows/ubuntu.yml` to include libtfs:

```yaml
# Add after tebako tests
- name: Test libtfs integrated build
  run: |
    cmake -B build-libtfs \
      -DCMAKE_BUILD_TYPE=Release \
      -DUSE_SYSTEM_LIBTFS=OFF \
      -DWITH_TFS_TESTS=ON
    cmake --build build-libtfs --target tfs
    cd build-libtfs/lib/tfs
    ctest --verbose
```

#### Day 3-4: Update Release Workflows

Create `.github/workflows/release.yml`:

```yaml
name: Release

on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  create-release:
    name: Create Release
    runs-on: ubuntu-latest
    
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      
      - name: Generate changelog
        run: |
          # Extract changelog for this version
          # Merge changelogs from tebako and lib/tfs
          
      - name: Create GitHub Release
        uses: softprops/action-gh-release@v1
        with:
          body_path: RELEASE_NOTES.md
          draft: false
          prerelease: false
```

#### Day 5: Badge and Status Updates

Update README.adoc badges:

```adoc
= Tebako

image:https://github.com/tamatebako/tebako/actions/workflows/ubuntu.yml/badge.svg[Ubuntu]
image:https://github.com/tamatebako/tebako/actions/workflows/libtfs.yml/badge.svg[libtfs]
image:https://codecov.io/gh/tamatebako/tebako/branch/main/graph/badge.svg[Coverage]
```

### Phase 4: Documentation and Finalization (Week 4)

#### Day 1-2: Documentation Updates

**Update root README.adoc**:

```adoc
== Architecture

Tebako consists of two main components:

=== Tebako Ruby Packager

The main application that packages Ruby applications into self-contained executables.

=== libtfs (Tebako File System)

An integrated library providing virtual filesystem support.
See link:lib/tfs/README.adoc[libtfs documentation] for details.

Full architecture documentation: link:docs/TEBAKO_INTEGRATION_ARCHITECTURE.md[]
```

**Create lib/tfs/README.adoc** (updated from original):

```adoc
= libtfs - Tebako File System

NOTE: This library is part of the Tebako monorepo but can be built independently.

== Building

=== As part of Tebako (recommended)

[source,bash]
----
cd /path/to/tebako
cmake -B build ...
cmake --build build
----

=== Standalone build

[source,bash]
----
cd /path/to/tebako/lib/tfs
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
----

== Documentation

* link:docs/ARCHITECTURE.md[Architecture]
* link:docs/TESTING.adoc[Testing Guide]
* link:docs/backends/ZIP_BACKEND.adoc[ZIP Backend]
* link:docs/backends/SQUASHFS_BACKEND.adoc[SquashFS Backend]

For integration with Tebako, see link:../../docs/TEBAKO_INTEGRATION_ARCHITECTURE.md[]
```

**Create docs/MONOREPO_STRUCTURE.md**:

```markdown
# Tebako Monorepo Structure

This document describes the monorepo organization and build system.

## Directory Structure

- `lib/tfs/` - Integrated libtfs library (VFS implementation)
- `src/` - Tebako C++ shim code  
- `lib/` - Tebako Ruby library code
- `bin/`, `exe/` - Ruby executables
- `docs/` - Integrated documentation

## Building

### Full Tebako Build
Builds tebako with integrated libtfs...

### libtfs Only
To build just the filesystem library...

## Development Workflow

### Working on libtfs
Changes to `lib/tfs/` are tested independently...

### Working on Tebako Integration
Changes affecting both components...
```

#### Day 3: Changelog Consolidation

**Create CHANGELOG.md merger script**:

```bash
#!/bin/bash
# merge-changelogs.sh

echo "# Tebako Changelog" > MERGED_CHANGELOG.md
echo "" >> MERGED_CHANGELOG.md

echo "## Tebako Releases" >> MERGED_CHANGELOG.md
cat CHANGELOG.md >> MERGED_CHANGELOG.md

echo "" >> MERGED_CHANGELOG.md
echo "## libtfs History (merged $(date))" >> MERGED_CHANGELOG.md
cat lib/tfs/CHANGELOG.md >> MERGED_CHANGELOG.md

mv MERGED_CHANGELOG.md CHANGELOG.md
```

#### Day 4-5: Final Validation

**Validation Checklist**:

- [ ] Standalone libtfs build works on all platforms
- [ ] Integrated tebako build works on all platforms  
- [ ] All libtfs tests pass in both modes
- [ ] All tebako tests pass with integrated libtfs
- [ ] Git history is fully preserved
- [ ] Documentation is updated and accurate
- [ ] CI/CD pipelines are green
- [ ] Release process documented

**Test Script** (`validate-merger.sh`):

```bash
#!/bin/bash
set -e

echo "=== Validation Script for libtfs → tebako merger ==="

# 1. Verify git history
echo "1. Checking git history preservation..."
cd lib/tfs
COMMIT_COUNT=$(git log --oneline | wc -l)
if [ "$COMMIT_COUNT" -lt 100 ]; then
    echo "ERROR: Not enough commits in history"
    exit 1
fi
cd ../..

# 2. Standalone build
echo "2. Testing standalone libtfs build..."
cd lib/tfs
rm -rf build-test
cmake -B build-test -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DWITH_TESTS=ON
cmake --build build-test
cd build-test && ctest && cd ..
cd ../..

# 3. Integrated build
echo "3. Testing integrated tebako build..."
rm -rf build-test
cmake -B build-test -DTEBAKO_VERSION=0.9.0 -DSETUP_MODE=ON -DUSE_SYSTEM_LIBTFS=OFF
cmake --build build-test --target setup

# 4. Documentation check
echo "4. Checking documentation links..."
command -v markdown-link-check || npm install -g markdown-link-check
find docs lib/tfs/docs -name "*.md" -exec markdown-link-check {} \;

echo "=== All validations passed ==="
```

---

## Dependency Management

### Current Dependencies

**libtfs (`lib/tfs/vcpkg.json`)**:
```json
{
  "dependencies": [
    "boost-asio", "boost-chrono", "boost-context", "boost-crc",
    "boost-filesystem", "boost-iostreams", "boost-multi-index",
    "boost-process", "boost-program-options", "boost-thread",
    "boost-variant",
    "argtable3", "brotli", "double-conversion", "fmt", "glog",
    "gflags", "libarchive", "libevent", "libzip", "lz4",
    "openssl", "pkgconf", "squashfs-tools-ng", "utfcpp",
    "xxhash", "zstd"
  ]
}
```

**tebako (`vcpkg.json`)**: Currently none (relies on external DWARFS_WR)

### Merged Dependencies

**Root `vcpkg.json`**:
```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg/master/scripts/vcpkg.schema.json",
  "name": "tebako",
  "version-string": "0.9.0",
  "description": "Tebako application packager with integrated libtfs",
  "dependencies": [
    "boost-asio",
    "boost-chrono",
    "boost-context",
    "boost-crc",
    "boost-filesystem",
    "boost-iostreams",
    "boost-multi-index",
    "boost-process",
    "boost-program-options",
    "boost-thread",
    "boost-variant",
    "argtable3",
    "brotli",
    "double-conversion",
    "fmt",
    "glog",
    "gflags",
    "libarchive",
    "libevent",
    "libzip",
    "lz4",
    "openssl",
    "pkgconf",
    "squashfs-tools-ng",
    "utfcpp",
    "xxhash",
    "zstd"
  ]
}
```

**Keep `lib/tfs/vcpkg.json`** for standalone builds:
```json
{
  "name": "libtfs",
  "version-string": "2.0.0",
  "description": "Tebako File System library (standalone build)",
  "dependencies": [
    "libzip",
    "squashfs-tools-ng",
    "argtable3",
    "boost-filesystem",
    "boost-iostreams"
  ]
}
```

---

## Benefits vs Risks

### Benefits

| Benefit | Impact | Rationale |
|---------|--------|-----------|
| **Atomic Commits** | High | Changes affecting both tebako and libtfs can be committed together |
| **Simplified Releases** | High | Single version number, single release process |
| **Easier Development** | High | No need to coordinate changes across repositories |
| **Unified CI/CD** | Medium | Single pipeline, consistent testing |
| **Better Visibility** | Medium | All issues and PRs in one place |
| **Reduced Overhead** | Low | One less repository to maintain |

### Risks

| Risk | Likelihood | Impact | Mitigation Strategy |
|------|------------|--------|---------------------|
| **Git History Loss** | Low | Critical | Use subtree merge; extensive validation |
| **Build Breakage** | Medium | High | Phased rollout; maintain backward compatibility |
| **CI/CD Disruption** | Medium | Medium | Test workflows before merging; keep separate pipelines initially |
| **Developer Confusion** | Low | Low | Clear documentation; migration guide |
| **Increased Build Time** | Low | Low | libtfs build is optional in tebako; use ccache |

### Risk Mitigation

1. **History Loss**: 
   - Multiple validation steps
   - Backup branches and tags
   - Test `git log --follow`

2. **Build Breakage**:
   - Dual-mode CMake setup
   - Backward compatibility layer
   - Extensive testing on all platforms

3. **CI/CD Disruption**:
   - Parallel workflows initially
   - Gradual migration
   - Rollback plan ready

---

## Timeline and Milestones

### Week 1: Foundation

**Milestone 1.1**: Repository Backup Complete
- [ ] Tags created in both repos
- [ ] Backup branches pushed
- [ ] Team notified

**Milestone 1.2**: Git History Merged
- [ ] Subtree merge executed
- [ ] History validation passed
- [ ] Files accessible at `lib/tfs/`

**Milestone 1.3**: Structure Finalized
- [ ] Directory layout matches plan
- [ ] Initial CMakeLists.txt created
- [ ] Committed to merge branch

### Week 2: CMake Integration

**Milestone 2.1**: Standalone Build Working
- [ ] `lib/tfs/` builds independently
- [ ] All tests pass
- [ ] CLI tool works

**Milestone 2.2**: Integrated Build Working
- [ ] Tebako builds with bundled libtfs
- [ ] Link dependencies resolve
- [ ] Setup command succeeds

**Milestone 2.3**: Cross-Platform Validation
- [ ] Ubuntu 20.04, 22.04 builds pass
- [ ] macOS arm64, x86_64 builds pass
- [ ] Windows MSYS2 build passes
- [ ] Alpine Linux build passes

### Week 3: CI/CD Migration

**Milestone 3.1**: Workflow Consolidation
- [ ] libtfs.yml created and working
- [ ] Existing workflows updated
- [ ] Coverage reporting configured

**Milestone 3.2**: Release Automation
- [ ] Release workflow created
- [ ] Changelog generation automated
- [ ] Tag handling verified

**Milestone 3.3**: Badge Updates
- [ ] README.adoc updated
- [ ] All badges working
- [ ] Status checks passing

### Week 4: Documentation and Launch

**Milestone 4.1**: Documentation Complete
- [ ] Root README updated
- [ ] lib/tfs/README updated
- [ ] MONOREPO_STRUCTURE.md created
- [ ] All links verified

**Milestone 4.2**: Final Validation
- [ ] validation-merger.sh passes
- [ ] All tests green on all platforms
- [ ] Performance benchmarks run

**Milestone 4.3**: Launch
- [ ] Merge to main
- [ ] Create v0.9.0 release
- [ ] Announce monorepo structure
- [ ] Archive old libtfs repo (read-only)

---

## Rollback Plan

### If Problems Occur During Migration

**Before Merge to Main**:

1. Delete merge branch
2. Restore from backup branches
3. Resume from backup state

**After Merge to Main** (within 24 hours):

```bash
# 1. Revert the merger commit
git revert <merger-commit-hash> -m 1

# 2. Force push to main (requires team approval)
git push origin main --force-with-lease

# 3. Restore from backup
git checkout pre-merger-backup
git checkout -b main-restored
git push origin main-restored:main --force-with-lease
```

**After 24 Hours**:

Forward-fix only:
1. Create fix branch
2. Address specific issues
3. Test thoroughly
4. Merge fix

### Rollback Decision Tree

```
Problem Detected
      ↓
   Severity?
      ↓
  ┌───┴───┐
  │       │
Critical  Minor
  │       │
  │       └→ Forward Fix
  │
Time since merge?
  │
  ├─ < 24h → Full Rollback
  └─ > 24h → Emergency Fix + Fast Forward
```

---

## Testing Strategy

### Pre-Merge Testing

**Phase 1: Unit Tests**
- [ ] All libtfs tests pass standalone
- [ ] All libtfs tests pass integrated
- [ ] All tebako tests pass with integrated libtfs

**Phase 2: Integration Tests**
- [ ] Tebako can package applications
- [ ] Packaged applications run correctly
- [ ] --tebako-extract works
- [ ] All backends (ZIP, SquashFS) functional

**Phase 3: Platform Tests**
- [ ] Ubuntu 20.04 build + test
- [ ] Ubuntu 22.04 build + test
- [ ] macOS 14 arm64 build + test
- [ ] macOS 14 x86_64 build + test
- [ ] Windows MSYS2 build + test
- [ ] Alpine 3.17 build + test

**Phase 4: Performance Tests**
- [ ] Benchmark libtfs standalone vs integrated
- [ ] Ensure no performance regression
- [ ] Build time acceptable (<15% increase)

### Post-Merge Validation

**Day 1** (Immediately after merge):
- [ ] CI/CD pipelines all green
- [ ] Release build succeeds
- [ ] Docker containers build
- [ ] Documentation site updated

**Week 1**:
- [ ] Community feedback addressed
- [ ] Any reported issues resolved
- [ ] Performance monitoring

**Month 1**:
- [ ] Adoption metrics reviewed
- [ ] Developer workflow optimized
- [ ] Documentation improved based on feedback

---

## Appendices

### A. Command Reference

**Full Migration Command Sequence**:

```bash
#!/bin/bash
# complete-migration.sh - Full migration script

set -e

TEBAKO_ROOT="/Users/mulgogi/src/tamatebako/tebako"
LIBTFS_ROOT="/Users/mulgogi/src/tamatebako/libdwarfs"

# Step 1: Backup
cd "$TEBAKO_ROOT"
git tag pre-merger-backup-$(date +%Y%m%d)
git checkout -b pre-merger-backup-branch
git push origin pre-merger-backup-branch

cd "$LIBTFS_ROOT"
git tag pre-merger-backup-$(date +%Y%m%d)
git checkout -b pre-merger-backup-branch
git push origin pre-merger-backup-branch

# Step 2: Merge
cd "$TEBAKO_ROOT"
git checkout -b merge-libtfs-$(date +%Y%m%d)
git remote add libtfs-origin "$LIBTFS_ROOT"
git fetch libtfs-origin

git merge -s ours --no-commit --allow-unrelated-histories libtfs-origin/main
git read-tree --prefix=lib/tfs/ -u libtfs-origin/main

git commit -m "Merge libtfs repository into lib/tfs/

Preserves full Git history of libtfs project.
See docs/REPOSITORY_MERGER_PLAN.md for details."

# Step 3: Validate
./validate-merger.sh

echo "Migration complete! Review changes and push when ready."
```

### B. File Mapping Table

| Original Path (libtfs) | New Path (tebako) | Notes |
|------------------------|-------------------|-------|
| `CMakeLists.txt` | `lib/tfs/CMakeLists.txt` | Modified for dual-mode |
| `vcpkg.json` | `lib/tfs/vcpkg.json` | Kept for standalone |
| `README.adoc` | `lib/tfs/README.adoc` | Updated with monorepo context |
| `docs/**` | `lib/tfs/docs/**` | Unchanged |
| `include/**` | `lib/tfs/include/**` | Unchanged |
| `src/**` | `lib/tfs/src/**` | Unchanged |
| `tests/**` | `lib/tfs/tests/**` | Unchanged |
| `examples/**` | `lib/tfs/examples/**` | Unchanged |
| `.github/workflows/**` | Merged into `.github/workflows/` | Consolidated |

### C. Checklist for Go-Live

**Pre-Launch**:
- [ ] All tests passing on all platforms
- [ ] Documentation reviewed and approved
- [ ] Team trained on new structure
- [ ] Rollback plan tested
- [ ] Performance benchmarks acceptable

**Launch Day**:
- [ ] Merge to main
- [ ] Tag release v0.9.0
- [ ] Update GitHub settings
- [ ] Announce on mailing list/Slack
- [ ] Archive old libtfs repository
- [ ] Update external references (if any)

**Post-Launch** (Week 1):
- [ ] Monitor CI/CD metrics
- [ ] Address community feedback
- [ ] Document lessons learned
- [ ] Update this plan based on experience

---

**Document Version**: 1.0  
**Last Updated**: 2025-12-22  
**Status**: Ready for Review  
**Next Action**: Team review and approval before execution