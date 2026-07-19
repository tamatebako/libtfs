# Phase 6: v0.12.0 Development - Continuation Plan

**Target Release**: Q1 2026
**Current Status**: v0.11.0 Production Ready
**Next Goal**: DwarFS Backend + Extraction Features
**Estimated Duration**: 8-10 weeks

---

## Overview

Phase 6 focuses on implementing the next major feature set for libtfs v0.12.0, including the DwarFS backend implementation and file extraction functionality.

### Objectives

1. **Complete DwarFS Backend** - Full read-only implementation
2. **Add Extraction API** - Extract files from mounted archives
3. **Performance Optimizations** - Directory caching and read buffering
4. **Cross-Platform Testing** - Validated on all target platforms
5. **Documentation Updates** - Complete guides for new features

---

## Phase 6 Priorities

### Priority 1: DwarFS Backend Implementation (4 weeks)

#### Week 1-2: Core Implementation
- [ ] Create `DwarfsBackend` class implementing `FileSystem` interface
- [ ] Implement `DwarfsFileHandle` for file reading
- [ ] Implement `DwarfsDirectoryIterator` for directory traversal
- [ ] Integrate with existing DwarFS v0.9+ libraries
- [ ] Handle metadata (permissions, timestamps, sizes)

**Deliverables**:
- `include/tebako/fs/backends/dwarfs_backend.h`
- `src/backends/dwarfs_backend.cpp`
- Integration with `BackendFactory::create_dwarfs()`

#### Week 3-4: Testing & Documentation
- [ ] Create `tests/test_dwarfs_backend.cpp` (47+ tests)
- [ ] Create `tests/test_dwarfs_integration.cpp` (13+ tests)
- [ ] Add DwarFS test fixtures
- [ ] Write `docs/backends/DWARFS_BACKEND.adoc`
- [ ] Performance benchmarks vs ZIP and SquashFS

**Success Criteria**:
- All DwarFS backend tests passing (60+ tests)
- Format auto-detection working
- Performance meets expectations (< 5ms mount time)
- Documentation complete

### Priority 2: Extraction Functionality (2 weeks)

#### Week 5-6: Implementation
- [ ] Design extraction API (C++ and C interfaces)
- [ ] Implement `tebako_fs_extract_all()` C API function
- [ ] Implement `FileSystem::extract()` C++ method
- [ ] Add recursive directory extraction
- [ ] Preserve permissions and timestamps
- [ ] Add progress callback support

**API Design**:
```cpp
// C++ API
class FileSystem {
  virtual bool extract(const std::string& src_path,
                      const std::string& dest_path,
                      ExtractOptions options = {}) = 0;
  virtual bool extract_all(const std::string& dest_path,
                           ExtractOptions options = {}) = 0;
};

struct ExtractOptions {
  bool preserve_permissions = true;
  bool preserve_timestamps = true;
  std::function<void(const std::string&, size_t, size_t)> progress_callback;
};
```

```c
// C API
int tebako_fs_extract(const char* src_path, const char* dest_path);
int tebako_fs_extract_all(const char* dest_path);
typedef void (*tebako_progress_callback_t)(const char* path, size_t current, size_t total);
int tebako_fs_extract_with_callback(const char* src_path, const char* dest_path, tebako_progress_callback_t callback);
```

**Deliverables**:
- Extraction API in C++ and C
- Tests for extraction functionality
- CLI integration: `tebakofs extract`
- Documentation

**Success Criteria**:
- All extraction tests passing
- Permissions and timestamps preserved
- Progress callbacks working
- CLI tool extraction complete

### Priority 3: Performance Optimizations (2 weeks)

#### Week 7-8: Caching & Buffering
- [ ] Implement directory listing cache
- [ ] Add cache invalidation strategy
- [ ] Implement larger read buffers
- [ ] Optimize sequential reads
- [ ] Add configuration options for cache size

**Performance Targets**:
- Directory cache: 100x speedup for repeated listings
- Read throughput: > 200 MB/s (from 100 MB/s)
- Memory overhead: < 20 MB for typical archives

**Deliverables**:
- Cache implementation
- Performance benchmarks
- Configuration documentation
- Updated performance baseline

**Success Criteria**:
- Performance targets met
- No memory leaks
- Configurable cache behavior
- Benchmarks show improvements

---

## Testing Requirements

### Test Coverage Goals
- DwarFS Backend: 47+ tests (matching ZIP/SquashFS)
- DwarFS Integration: 13+ tests
- Extraction API: 20+ tests
- Performance: 5+ benchmark tests
- **Total New Tests**: 85+ tests
- **Overall Pass Rate**: Maintain 100%

### Platform Validation
- macOS (Apple Silicon): Full DwarFS support
- Linux (Ubuntu): Full support with all features
- Linux (Alpine): Full support
- Windows (MSYS2): Best effort (DwarFS may be limited)

---

## Documentation Requirements

### New Documentation
1. `docs/backends/DWARFS_BACKEND.adoc` - Complete DwarFS guide
2. `docs/EXTRACTION_API.md` - Extraction API documentation
3. Updates to `docs/TEBAKO_INTEGRATION.md` - New features
4. Updates to `ROADMAP.md` - v0.13.0 planning

### Updated Documentation
- `README.adoc` - Feature list, examples
- `docs/TESTING.adoc` - New test suites
- `docs/PERFORMANCE_BASELINE.md` - Updated metrics
- `CHANGELOG.md` - v0.12.0 section

---

## Release Criteria for v0.12.0

### Code Quality
- [ ] All 225+ tests passing (140 existing + 85 new)
- [ ] No memory leaks (Valgrind/ASAN clean)
- [ ] Zero compiler warnings
- [ ] Code coverage > 90%

### Features
- [ ] DwarFS backend fully functional
- [ ] Extraction API complete (C++ and C)
- [ ] Performance optimizations implemented
- [ ] CLI tool supports all new features

### Documentation
- [ ] All backend docs complete
- [ ] Extraction API documented
- [ ] Migration guide from v0.11.0
- [ ] Examples updated

### Performance
- [ ] Mount time: < 5ms (DwarFS)
- [ ] Read throughput: > 200 MB/s
- [ ] Directory cache: 100x improvement
- [ ] Test suite: < 40s

### Platform Support
- [ ] macOS validated
- [ ] Ubuntu validated
- [ ] Alpine validated
- [ ] Windows best effort

---

## Risk Mitigation

### Technical Risks
1. **DwarFS Integration Complexity**
   - Mitigation: Start with simple read-only operations
   - Fallback: Limit initial feature set

2. **Performance Regression**
   - Mitigation: Continuous benchmarking
   - Fallback: Make optimizations optional

3. **Platform Compatibility**
   - Mitigation: Test early and often
   - Fallback: Document platform limitations

### Schedule Risks
1. **DwarFS Backend Complexity**
   - Mitigation: 4 weeks allocated (generous)
   - Fallback: Simplify initial implementation

2. **Testing Overhead**
   - Mitigation: Write tests alongside implementation
   - Fallback: Focus on critical path tests

---

## Success Metrics

### Quantitative
- 225+ tests (100% pass rate)
- < 5ms DwarFS mount time
- > 200 MB/s read throughput
- 100x cache speedup
- < 40s test suite time

### Qualitative
- Clean, maintainable code
- Comprehensive documentation
- Positive user feedback
- No critical bugs

---

## Timeline

### Week 1-2: DwarFS Core (Jan 2026)
- DwarfsBackend class
- File and directory operations
- Basic testing

### Week 3-4: DwarFS Testing (Jan-Feb 2026)
- Comprehensive test suite
- Performance benchmarks
- Documentation

### Week 5-6: Extraction API (Feb 2026)
- API design and implementation
- Testing
- CLI integration

### Week 7-8: Performance (Feb-Mar 2026)
- Caching implementation
- Read buffer optimization
- Final benchmarks

### Week 9-10: Polish & Release (Mar 2026)
- Bug fixes
- Documentation review
- Release preparation
- v0.12.0 release

---

## Dependencies

### External
- DwarFS libraries (already integrated)
- vcpkg dependencies (already available)
- Google Test (already available)

### Internal
- v0.11.0 production deployment
- Performance baseline established
- Test infrastructure in place

---

## Next Steps After Phase 6

### v0.13.0 Planning (Q2 2026)
- TAR backend implementation
- Write operations (tentative)
- Compression options
- ISO 9660 backend (tentative)

### v1.0.0 Planning (Late 2026)
- API stability guarantees
- Full platform matrix
- Complete automation
- Production hardening

---

## Getting Started

### Prerequisites
1. v0.11.0 deployed and validated
2. Development environment set up
3. DwarFS libraries verified
4. Test infrastructure ready

### First Steps
1. Read `docs/backends/ZIP_BACKEND.adoc` and `SQUASHFS_BACKEND.adoc` as templates
2. Review DwarFS v0.9+ API documentation
3. Create initial `DwarfsBackend` skeleton
4. Write first failing test
5. Implement until test passes

---

## Questions to Address

1. Should DwarFS backend support write operations? (Probably not in v0.12.0)
2. What cache eviction policy to use? (LRU recommended)
3. Should extraction be atomic or stream-based? (Stream for large archives)
4. Windows DwarFS support feasible? (Investigate early)

---

**Phase 6 will deliver a feature-complete v0.12.0 with DwarFS support and extraction capabilities, setting the stage for v1.0.0 API stability.**