# Master Plan: libtebako-fs Transformation

## Overview

Transform libdwarfs-wr into libtebako-fs - a modular, multi-language, multi-backend virtual filesystem layer.

**Approach:** Three sequential stages, each complete and tested before proceeding.

---

## Stage 1: Rename & Solidify (Week 1-2) 🎯 CURRENT

**Goal:** Rename to libtebako-fs, ensure rock-solid foundation with new dwarfs (no thrift/folly)

### Tasks

#### 1.1 Repository & Build System (Day 1)
- [ ] Rename GitHub repository: `tamatebako/libdwarfs` → `tamatebako/libtebako-fs`
- [ ] Update CMakeLists.txt: `project(libtebako-fs)`
- [ ] Update target names: `dwarfs-wr` → `tebako-fs`
- [ ] Update pkg-config name
- [ ] Verify dwarfs at `~/src/external/dwarfs` has cereal/bitsery support

#### 1.2 Test Current Changes (Day 2-3)
- [ ] Clean build from scratch
- [ ] Verify: Thrift DISABLED, Cereal/Bitsery ENABLED
- [ ] Run full test suite (ctest)
- [ ] Verify no folly/thrift symbols in binaries
- [ ] Check static linking works
- [ ] Fix any issues found

#### 1.3 Header Reorganization (Day 4-5)
- [ ] Create `include/tebako/fs/` directory structure
- [ ] Move headers: `tebako-*.h` → `tebako/fs/*.h`
- [ ] Keep old headers as deprecated wrappers (backward compat)
- [ ] Update all source files to use new paths
- [ ] Update examples

#### 1.4 Documentation Cleanup (Day 6)
- [ ] Keep only: MASTER_PLAN.md, ARCHITECTURE.md, ARCHIVE_FORMATS_AS_FILESYSTEMS.md, MULTI_LANGUAGE_ARCHITECTURE.md
- [ ] Delete obsolete planning decks
- [ ] Update README.md with new name and structure
- [ ] Create CHANGELOG.md for v2.0.0

#### 1.5 Final Testing (Day 7-8)
- [ ] Full regression testing
- [ ] Performance benchmarks
- [ ] Memory leak checks
- [ ] Thread safety validation
- [ ] Cross-platform builds (Linux, macOS, Windows)

#### 1.6 Release (Day 9-10)
- [ ] Tag v2.0.0
- [ ] Update Tebako to use libtebako-fs v2.0
- [ ] Verify Tebako integration works
- [ ] Announce rename

**Deliverables:**
- ✅ libtebako-fs v2.0.0 released
- ✅ All tests passing
- ✅ Backward compatible
- ✅ Static linking verified
- ✅ Zero folly/thrift dependencies

---

## Stage 2: ZIP Backend (Week 3) 🔄 NEXT

**Goal:** Add ZIP backend using miniz for multi-backend support

### Tasks

#### 2.1 Backend Interface Definition (Day 1)
- [ ] Create `include/tebako/fs/backend.h`
- [ ] Define `IFilesystemBackend` interface
- [ ] Refactor existing DwarFS code to implement interface
- [ ] Create backend registry system

#### 2.2 Integrate miniz (Day 2)
- [ ] Add miniz as git submodule or FetchContent
- [ ] CMake: `option(TEBAKO_BACKEND_ZIP)`
- [ ] Build miniz library
- [ ] Create miniz link target

#### 2.3 Implement ZIP Backend (Day 3-4)
- [ ] Create `src/backends/zip/zip_backend.cpp`
- [ ] Implement central directory parsing
- [ ] Implement file index building
- [ ] Implement read operation (random access)
- [ ] Implement directory listing
- [ ] Implement stat

#### 2.4 Testing (Day 5)
- [ ] Create test ZIP archives
- [ ] Unit tests for ZIP backend
- [ ] Random access verification
- [ ] Performance benchmarks (vs full extraction)
- [ ] Memory usage validation

#### 2.5 Integration (Day 6-7)
- [ ] Update VFS core to route to ZIP backend
- [ ] Add ZIP mounting to API
- [ ] Create ZIP examples
- [ ] Update documentation

**Deliverables:**
- ✅ ZIP backend fully functional
- ✅ miniz integrated (~70KB addition)
- ✅ Both DwarFS and ZIP working
- ✅ Performance validated (50ms per file)

---

## Stage 3: Language Adapter Layer (Week 4-6) 🚀 FUTURE

**Goal:** Generalize language layer, support Julia as proof-of-concept

### Tasks

#### 3.1 Extract VFS Core (Week 4, Day 1-3)
- [ ] Identify language-agnostic code
- [ ] Move to `src/vfs/core/`
- [ ] Create stable C API (`vfs_open`, `vfs_read`, etc.)
- [ ] Ensure zero language-specific dependencies

#### 3.2 Create Adapter Interface (Week 4, Day 4-5)
- [ ] Define `AdapterHooks` structure
- [ ] Create adapter registration system
- [ ] Document adapter requirements

#### 3.3 Extract Ruby Adapter (Week 4, Day 6-7)
- [ ] Move Ruby code to `src/adapters/ruby/`
- [ ] Implement adapter hooks for Ruby
- [ ] Test Ruby still works identically
- [ ] Add: `option(TEBAKO_ADAPTER_RUBY)`

#### 3.4 Create Julia Adapter (Week 5)
- [ ] Research Julia's libuv integration
- [ ] Create `src/adapters/julia/`
- [ ] Implement `jl_fs_*` intercepts
- [ ] Handle uv_stat_t conversions
- [ ] Add: `option(TEBAKO_ADAPTER_JULIA)`

#### 3.5 Julia Testing (Week 6, Day 1-3)
- [ ] Create test Julia package
- [ ] Test Julia loading from VFS
- [ ] Performance validation
- [ ] Memory usage check

#### 3.6 Documentation & Release (Week 6, Day 4-5)
- [ ] Multi-language architecture guide
- [ ] Julia adapter documentation
- [ ] Example Julia packages
- [ ] Release v2.1.0

**Deliverables:**
- ✅ VFS core language-agnostic
- ✅ Ruby and Julia adapters working
- ✅ Clear template for Python/Node.js
- ✅ Adapter interface documented

---

## Success Criteria

### Stage 1
- [ ] New name libtebako-fs successfully deployed
- [ ] All tests pass
- [ ] Zero folly/thrift symbols
- [ ] Tebako integration works
- [ ] Performance unchanged

### Stage 2
- [ ] ZIP backend functional
- [ ] Both DwarFS and ZIP backends working simultaneously
- [ ] Random access validated (>20x faster than full extraction)
- [ ] Examples demonstrate ZIP usage

### Stage 3
-[ ] VFS core has zero language dependencies
- [ ] Ruby adapter maintains compatibility
- [ ] Julia adapter working with Julia packages
- [ ] Clear pattern for adding languages

---

## Timeline Summary

| Stage | Duration | Key Deliverable |
|-------|----------|-----------------|
| **Stage 1** | 2 weeks | libtebako-fs v2.0 - solid foundation |
| **Stage 2** | 1 week | ZIP backend added |
| **Stage 3** | 3 weeks | Multi-language support (Julia) |
| **Total** | **6 weeks** | Complete transformation |

---

## Documentation Cleanup

### Files to KEEP

**Master Planning:**
1. `MASTER_PLAN.md` (this file) - Overall roadmap

**Architecture:**
2. `ARCHITECTURE.md` - Design decisions (jemalloc, naming, separation)
3. `ARCHIVE_FORMATS_AS_FILESYSTEMS.md` - ZIP/miniz implementation
4. `MULTI_LANGUAGE_ARCHITECTURE.md` - Julia/Python support plan

**Move these 4 to top-level `docs/` - they are the definitive documentation**

### Files to DELETE (Obsolete Planning)

**Old planning documents:**
- ❌ `remove-folly-plan.md` - Historical, tasks complete
- ❌ `FOLLY_REMOVAL_SUMMARY.md` - Historical
- ❌ `remove-folly-status.md` - Obsolete status file
- ❌ `DEPENDENCY_STRATEGY.md` - Superseded by ARCHITECTURE.md
- ❌ `STATIC_LINKING_STRATEGY.md` - Problem solved, obsolete
- ❌ `NEXT_STEPS.md` - Replaced by MASTER_PLAN.md
- ❌ `TESTING_PLAN.md` - Testing complete
- ❌ `TEST_RESULTS.md` - Historical
- ❌ `REGRESSION_PREVENTION.md` - CI/CD now in workflows
- ❌ `TEBAKO_VFS_ARCHITECTURE.md` - Merged into ARCHITECTURE.md
- ❌ `PURPOSE_AND_SCOPE.md` - Covered in ARCHITECTURE.md
- ❌ `RENAMING_PROPOSAL.md` - Approved, executing in Stage 1
- ❌ `FINAL_SOLUTION.md` - Historical summary

**Keep in git history but remove from active docs/**

---

## Next Immediate Actions

### Before Starting Stage 1

1. **Approve this plan** - Review and confirm approach
2. **Verify dwarfs** - Ensure ~/src/external/dwarfs has cereal/bitsery
3. **Clean workspace** - Delete obsolete docs
4. **Create Stage 1 branch** - `git checkout -b stage-1-rename-libtebako-fs`

### Starting Stage 1

1. Run comprehensive tests of current state (baseline)
2. Begin repository rename
3. Update build system
4. Test continuously

**Ready to proceed?**