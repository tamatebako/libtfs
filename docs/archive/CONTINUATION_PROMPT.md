# Stage 2 Week 1 Day 2: Backend Registry Implementation

**Date**: 2025-12-21
**Current Status**: Week 1 Day 1 Complete ✅
**Next Task**: Implement Backend Registry with Format Detection

---

## Context: What's Been Done

### Week 1 Day 1 Complete (2025-12-21) ✅

Successfully created the foundational VFS interface layer:

1. **[`include/tebako/fs/filesystem.h`](../include/tebako/fs/filesystem.h)** (206 lines)
   - Abstract base class for all filesystem backends
   - 13 pure virtual methods covering lifecycle, file ops, directory ops, metadata
   - Fully documented with Doxygen comments and examples
   - Thread-safety requirements specified

2. **[`include/tebako/fs/file_handle.h`](../include/tebako/fs/file_handle.h)** (139 lines)
   - Abstract file handle for POSIX-like read operations
   - Methods: read(), seek(), tell(), eof(), close(), path(), size()
   - Non-thread-safe (per-thread handles)

3. **[`include/tebako/fs/directory_iterator.h`](../include/tebako/fs/directory_iterator.h)** (141 lines)
   - DirectoryEntry struct: name, is_directory, size, mtime
   - Iterator interface: has_next(), next(), reset()
   - Non-thread-safe (per-thread iterators)

**Architecture Verified:**
- ✅ Pure OOP design with abstract base classes
- ✅ POSIX-compatible API
- ✅ CMake builds successfully
- ✅ Ready for backend implementations

---

## Next Task: Week 1 Day 2 - Backend Registry

### Objective

Implement a plugin-based registry system that:
1. Registers filesystem backends (DwarFS, ZIP, future: TAR, SquashFS)
2. Detects archive format automatically (magic numbers, extensions)
3. Creates backend instances on demand
4. Thread-safe singleton pattern

### Files to Create

#### 1. Header: `include/tebako/fs/backend_registry.h`

**Design from [`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md:179-239)**:

```cpp
namespace tebako {
namespace fs {

// Type definitions
using BackendFactory = std::function<std::unique_ptr<FileSystem>()>;
using FormatDetector = std::function<bool(const std::string& path)>;

// Backend metadata
struct BackendInfo {
    std::string name;
    std::string description;
    std::vector<std::string> extensions;  // e.g., {".dwarfs", ".dfs"}
    FormatDetector detector;
    BackendFactory factory;
    int priority;  // Higher = checked first
};

// Thread-safe singleton registry
class BackendRegistry {
public:
    static BackendRegistry& instance();

    // Registration
    void register_backend(const BackendInfo& info);
    void unregister_backend(const std::string& name);

    // Backend lookup
    std::unique_ptr<FileSystem> create_backend(const std::string& name);
    std::unique_ptr<FileSystem> detect_and_create(const std::string& path);

    // Query
    std::vector<std::string> available_backends() const;
    bool has_backend(const std::string& name) const;
    BackendInfo get_backend_info(const std::string& name) const;

private:
    BackendRegistry() = default;
    std::map<std::string, BackendInfo> backends_;
    mutable std::shared_mutex mutex_;
};

}  // namespace fs
}  // namespace tebako
```

**Key Requirements:**
- Singleton pattern with static instance() method
- Thread-safe using `std::shared_mutex` (readers-writer lock)
- Store backends in `std::map<std::string, BackendInfo>`
- Format detection tries backends by priority (highest first)

#### 2. Implementation: `src/backend_registry.cpp`

**Implementation Pattern:**

```cpp
#include <tebako/fs/backend_registry.h>
#include <algorithm>
#include <fstream>
#include <shared_mutex>

namespace tebako {
namespace fs {

BackendRegistry& BackendRegistry::instance() {
    static BackendRegistry registry;
    return registry;
}

void BackendRegistry::register_backend(const BackendInfo& info) {
    std::unique_lock lock(mutex_);
    backends_[info.name] = info;
}

void BackendRegistry::unregister_backend(const std::string& name) {
    std::unique_lock lock(mutex_);
    backends_.erase(name);
}

std::unique_ptr<FileSystem> BackendRegistry::create_backend(
    const std::string& name) {
    std::shared_lock lock(mutex_);
    auto it = backends_.find(name);
    if (it == backends_.end()) {
        return nullptr;
    }
    return it->second.factory();
}

std::unique_ptr<FileSystem> BackendRegistry::detect_and_create(
    const std::string& path) {
    std::shared_lock lock(mutex_);
    
    // Sort by priority
    std::vector<const BackendInfo*> sorted;
    for (const auto& [name, info] : backends_) {
        sorted.push_back(&info);
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const auto* a, const auto* b) {
            return a->priority > b->priority;
        });
    
    // Try each detector
    for (const auto* info : sorted) {
        if (info->detector(path)) {
            return info->factory();
        }
    }
    
    return nullptr;
}

std::vector<std::string> BackendRegistry::available_backends() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    for (const auto& [name, info] : backends_) {
        names.push_back(name);
    }
    return names;
}

bool BackendRegistry::has_backend(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return backends_.find(name) != backends_.end();
}

BackendInfo BackendRegistry::get_backend_info(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = backends_.find(name);
    if (it == backends_.end()) {
        throw std::runtime_error("Backend not found: " + name);
    }
    return it->second;
}

}  // namespace fs
}  // namespace tebako
```

**Key Implementation Details:**
- Use `std::shared_lock` for read operations (allows concurrent reads)
- Use `std::unique_lock` for write operations (exclusive access)
- `detect_and_create()` sorts backends by priority before testing
- Error handling: return nullptr for missing backends, throw for invalid queries

### Testing

#### 3. Unit Tests: `tests/test_backend_registry.cpp`

**Test Coverage Required:**

```cpp
#include <gtest/gtest.h>
#include <tebako/fs/backend_registry.h>

// Mock backend for testing
class MockBackend : public tebako::fs::FileSystem {
    // Implement all pure virtual methods as stubs
};

TEST(BackendRegistry, Singleton) {
    auto& r1 = BackendRegistry::instance();
    auto& r2 = BackendRegistry::instance();
    EXPECT_EQ(&r1, &r2);
}

TEST(BackendRegistry, RegisterAndCreate) {
    auto& registry = BackendRegistry::instance();
    
    BackendInfo info;
    info.name = "MockBackend";
    info.description = "Test backend";
    info.factory = []() { return std::make_unique<MockBackend>(); };
    info.priority = 100;
    
    registry.register_backend(info);
    
    EXPECT_TRUE(registry.has_backend("MockBackend"));
    
    auto backend = registry.create_backend("MockBackend");
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->backend_name(), "MockBackend");
}

TEST(BackendRegistry, FormatDetection) {
    auto& registry = BackendRegistry::instance();
    
    BackendInfo info;
    info.name = "TestBackend";
    info.detector = [](const std::string& path) {
        return path.ends_with(".test");
    };
    info.factory = []() { return std::make_unique<MockBackend>(); };
    info.priority = 100;
    
    registry.register_backend(info);
    
    auto backend = registry.detect_and_create("/path/to/file.test");
    ASSERT_NE(backend, nullptr);
    
    auto no_match = registry.detect_and_create("/path/to/file.other");
    EXPECT_EQ(no_match, nullptr);
}

TEST(BackendRegistry, PriorityOrdering) {
    auto& registry = BackendRegistry::instance();
    
    // Register two backends with different priorities
    BackendInfo low_priority;
    low_priority.name = "Low";
    low_priority.priority = 50;
    low_priority.detector = [](const std::string&) { return true; };
    low_priority.factory = []() { /* return low priority backend */ };
    
    BackendInfo high_priority;
    high_priority.name = "High";
    high_priority.priority = 100;
    high_priority.detector = [](const std::string&) { return true; };
    high_priority.factory = []() { /* return high priority backend */ };
    
    registry.register_backend(low_priority);
    registry.register_backend(high_priority);
    
    auto backend = registry.detect_and_create("/any/path");
    EXPECT_EQ(backend->backend_name(), "High");
}

TEST(BackendRegistry, ThreadSafety) {
    // Test concurrent registration and lookup
    // Use std::thread to spawn multiple threads
    // Verify no data races or crashes
}
```

**Test Requirements:**
- Test singleton pattern
- Test registration and retrieval
- Test format detection
- Test priority ordering
- Test thread safety (concurrent operations)
- Test error cases (missing backend, etc.)

---

## Implementation Checklist

### Morning Session (2-3 hours)

- [ ] Create `include/tebako/fs/backend_registry.h`
  - [ ] BackendFactory typedef
  - [ ] FormatDetector typedef
  - [ ] BackendInfo struct
  - [ ] BackendRegistry class declaration
  - [ ] Doxygen documentation

- [ ] Create `src/backend_registry.cpp`
  - [ ] Singleton implementation
  - [ ] register_backend()
  - [ ] unregister_backend()
  - [ ] create_backend()
  - [ ] detect_and_create() with priority sorting
  - [ ] Query methods (available_backends, has_backend, get_backend_info)

### Afternoon Session (2-3 hours)

- [ ] Create `tests/test_backend_registry.cpp`
  - [ ] MockBackend stub implementation
  - [ ] Test singleton
  - [ ] Test registration/creation
  - [ ] Test format detection
  - [ ] Test priority ordering
  - [ ] Test thread safety
  - [ ] Test error handling

- [ ] Update CMakeLists.txt
  - [ ] Add `src/backend_registry.cpp` to tfs library sources
  - [ ] Add `tests/test_backend_registry.cpp` to test target

- [ ] Build and validate
  - [ ] `cmake .. && make -j$(nproc)`
  - [ ] `ctest -R test_backend_registry --verbose`
  - [ ] All tests passing

---

## Success Criteria

Week 1 Day 2 is complete when:

- [x] `backend_registry.h` created with complete interface
- [x] `backend_registry.cpp` implemented with thread-safe operations
- [x] `test_backend_registry.cpp` written with comprehensive coverage
- [x] CMakeLists.txt updated
- [x] All tests passing
- [x] Code compiles without warnings
- [x] Documentation updated in CONTINUATION_PLAN.md

---

## Architecture Notes

### Thread Safety Strategy

The registry uses **readers-writer lock** pattern:
- Multiple readers can access simultaneously (`std::shared_lock`)
- Writers get exclusive access (`std::unique_lock`)
- Critical sections are minimal for performance

### Format Detection Strategy

Three-tier detection (implemented in concrete backends):
1. **Magic numbers** (highest priority): Check file header bytes
2. **Extensions** (medium priority): Check file extension
3. **Structure validation** (lowest priority): Validate internal structure

Priority system allows overriding (e.g., `.zip` file with DwarFS magic → use DwarFS)

### Extensibility

The registry is **open for extension, closed for modification**:
- New backends register through BackendInfo
- No changes to registry code needed
- Factory pattern decouples registry from concrete backends

---

## Reference Documentation

- **Architecture**: [`STAGE_2_VFS_DESIGN.md`](STAGE_2_VFS_DESIGN.md:179-239) - Backend Registry section
- **Implementation Guide**: [`STAGE_2_QUICK_START.md`](STAGE_2_QUICK_START.md:74-109) - Day 2 details
- **Progress**: [`CONTINUATION_PLAN.md`](CONTINUATION_PLAN.md) - Overall timeline

---

## After Completion

Update documentation:
- Mark Week 1 Day 2 complete in TODO list
- Update CONTINUATION_PLAN.md Progress Log
- Update README.md Stage 2 status
- Commit with message: "feat(stage2): implement backend registry with format detection"

Then proceed to **Week 1 Day 3**: Refactor DwarfsBackend

---

**Ready to start? Let's implement the Backend Registry!** 🚀