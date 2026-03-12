# Plan: Remove libfolly Dependency from libdwarfs

## Overview

This document outlines the plan to remove the libfolly dependency from libdwarfs. Folly is currently used for only two features across the codebase, making removal straightforward.

## Current Folly Usage

### 1. `folly::Synchronized<T>` (7 instances)

Thread-safe wrapper providing reader-writer lock semantics:

- `src/tebako-io-helpers.cpp:133` - `folly::Synchronized<tebako_path_s*> tebako_cwd`
- `src/dl-ctl.cpp:49` - `folly::Synchronized<tebako_dlerror_data> tebako_dlerror_stash`
- `src/dl-ctl.cpp:54` - `class sync_tebako_dltable : public folly::Synchronized<tebako_dltable*>`
- `include/tebako-kfd.h:44` - `folly::Synchronized<tebako_kfdtable> s_tebako_kfdtable`
- `include/tebako-fd.h:67` - `folly::Synchronized<tebako_fdtable> s_tebako_fdtable`
- `include/tebako-memfs-table.h:43` - `folly::Synchronized<tebako_memfs_table> s_tebako_memfs_table`
- `include/tebako-mount-table.h:40` - `folly::Synchronized<tebako_mount_table> s_tebako_mount_table`
- `include/tebako-dirent.h:108` - `folly::Synchronized<tebako_dstable> s_tebako_dstable`

### 2. `folly::to<T>()` (3 instances)

String-to-type conversions:

- `src/tebako-memfs.cpp:116` - `folly::to<double>(decompress_ratio)`
- `src/tebako-memfs.cpp:128` - `folly::to<file_off_t>(image_offset)`
- `src/tebako-memfs.cpp:143` - `folly::to<size_t>(workers)`

## Replacement Strategy

### Phase 1: Create Replacement Utilities

#### 1.1 Custom `Synchronized<T>` Template

**File**: `include/tebako-synchronized.h`

**Implementation**:
```cpp
#pragma once

#include <shared_mutex>
#include <memory>
#include <utility>

namespace tebako {

template<typename T>
class Synchronized {
private:
    T data_;
    mutable std::shared_mutex mutex_;

public:
    // Read lock holder (const access)
    class ConstLockedPtr {
    private:
        std::shared_lock<std::shared_mutex> lock_;
        const T* ptr_;

    public:
        ConstLockedPtr(std::shared_mutex& m, const T* p)
            : lock_(m), ptr_(p) {}

        const T* operator->() const { return ptr_; }
        const T& operator*() const { return *ptr_; }
    };

    // Write lock holder (mutable access)
    class LockedPtr {
    private:
        std::unique_lock<std::shared_mutex> lock_;
        T* ptr_;

    public:
        LockedPtr(std::shared_mutex& m, T* p)
            : lock_(m), ptr_(p) {}

        T* operator->() { return ptr_; }
        T& operator*() { return *ptr_; }
    };

    // Constructors
    Synchronized() : data_() {}
    explicit Synchronized(T&& val) : data_(std::move(val)) {}
    explicit Synchronized(const T& val) : data_(val) {}

    // Deleted copy operations (not thread-safe)
    Synchronized(const Synchronized&) = delete;
    Synchronized& operator=(const Synchronized&) = delete;

    // Lock methods
    LockedPtr wlock() {
        return LockedPtr(mutex_, &data_);
    }

    ConstLockedPtr rlock() const {
        return ConstLockedPtr(mutex_, &data_);
    }

    // Exchange operation (atomic swap)
    T exchange(T&& new_val) {
        auto lock = wlock();
        T old = std::move(*lock);
        *lock = std::move(new_val);
        return old;
    }
};

} // namespace tebako
```

**Features**:
- Thread-safe using `std::shared_mutex` (C++17)
- Multiple readers or single writer semantics
- RAII lock management
- Compatible API with `folly::Synchronized`

#### 1.2 String Conversion Utilities

**File**: `include/tebako-conversions.h`

**Implementation**:
```cpp
#pragma once

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace tebako {
namespace util {

// Generic string_to template
template<typename T>
T string_to(const char* str);

// Specialization for double
template<>
inline double string_to<double>(const char* str) {
    if (!str || *str == '\0') {
        throw std::invalid_argument("Cannot convert empty string to double");
    }

    char* end;
    errno = 0;
    double result = std::strtod(str, &end);

    if (errno == ERANGE) {
        throw std::out_of_range(
            std::string("Value out of range: ") + str);
    }
    if (end == str || *end != '\0') {
        throw std::invalid_argument(
            std::string("Cannot convert '") + str + "' to double");
    }

    return result;
}

// Specialization for size_t
template<>
inline size_t string_to<size_t>(const char* str) {
    if (!str || *str == '\0') {
        throw std::invalid_argument("Cannot convert empty string to size_t");
    }

    char* end;
    errno = 0;
    unsigned long long result = std::strtoull(str, &end, 10);

    if (errno == ERANGE) {
        throw std::out_of_range(
            std::string("Value out of range: ") + str);
    }
    if (end == str || *end != '\0') {
        throw std::invalid_argument(
            std::string("Cannot convert '") + str + "' to size_t");
    }

    return static_cast<size_t>(result);
}

// Specialization for file_off_t (usually int64_t or long long)
template<>
inline file_off_t string_to<file_off_t>(const char* str) {
    if (!str || *str == '\0') {
        throw std::invalid_argument("Cannot convert empty string to file_off_t");
    }

    char* end;
    errno = 0;
    long long result = std::strtoll(str, &end, 10);

    if (errno == ERANGE) {
        throw std::out_of_range(
            std::string("Value out of range: ") + str);
    }
    if (end == str || *end != '\0') {
        throw std::invalid_argument(
            std::string("Cannot convert '") + str + "' to file_off_t");
    }

    return static_cast<file_off_t>(result);
}

// String overload for convenience
template<typename T>
inline T string_to(const std::string& str) {
    return string_to<T>(str.c_str());
}

} // namespace util
} // namespace tebako
```

**Features**:
- Uses standard C library functions
- Proper error handling with exceptions
- Type-safe conversions
- Compatible API with `folly::to<>()`

### Phase 2: Update Source Files

#### 2.1 Update Header Files

**File**: `include/tebako-pch-pp.h`

Changes:
- Remove: `#include <folly/Conv.h>`
- Remove: `#include <folly/Synchronized.h>`
- Add: `#include <tebako-synchronized.h>`
- Add: `#include <tebako-conversions.h>`

#### 2.2 Update Implementation Files

**All files**: Replace namespace references
- `folly::Synchronized<T>` → `tebako::Synchronized<T>`
- `folly::to<T>(str)` → `tebako::util::string_to<T>(str)`

Files to update:
1. `src/tebako-io-helpers.cpp`
2. `src/dl-ctl.cpp`
3. `src/tebako-memfs.cpp`
4. `include/tebako-kfd.h`
5. `include/tebako-fd.h`
6. `include/tebako-memfs-table.h`
7. `include/tebako-mount-table.h`
8. `include/tebako-dirent.h`

### Phase 3: Update Build System

#### 3.1 Update CMakeLists.txt

Changes needed:
- Remove `__LIBFOLLY` references (lines around 477, 1241)
- Remove folly from link libraries
- Remove folly build dependencies

#### 3.2 Update vcpkg.json

Check if folly is listed and remove if present.

### Phase 4: Testing

#### 4.1 Compilation Test
```bash
mkdir build && cd build
cmake ..
make
```

#### 4.2 Unit Tests
```bash
ctest
```

#### 4.3 Integration Tests
Run existing test suite to verify functionality.

## Benefits

1. **Reduced Dependencies**: Eliminates folly and its transitive dependencies
2. **Simpler Build**: Faster builds without folly compilation
3. **Smaller Binary**: No unused folly code
4. **Better Portability**: Standard C++17 only
5. **Easier Maintenance**: Fewer dependencies to track and update

## Risks and Mitigation

### Risk 1: Thread Safety
**Mitigation**: Comprehensive multi-threading tests, especially for Synchronized<T> replacement

### Risk 2: Performance
**Mitigation**: Benchmark critical paths; `std::shared_mutex` should have comparable performance

### Risk 3: API Compatibility
**Mitigation**: Ensure replacement APIs match folly's behavior exactly; add compatibility tests

## Timeline

- Phase 1: Create utilities (1-2 hours)
- Phase 2: Update source files (2-3 hours)
- Phase 3: Update build system (1 hour)
- Phase 4: Testing and validation (2-3 hours)

**Total estimated effort**: 6-9 hours

## Success Criteria

1. ✅ Code compiles without folly
2. ✅ All existing tests pass
3. ✅ No performance regression
4. ✅ Thread safety maintained
5. ✅ Documentation updated