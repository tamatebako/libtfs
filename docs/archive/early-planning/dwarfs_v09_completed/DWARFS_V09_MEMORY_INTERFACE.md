# DwarFS v0.9+ Memory Interface Analysis

**Date**: 2025-12-22  
**Purpose**: Document how modern DwarFS handles memory-backed filesystems

---

## Executive Summary

Modern DwarFS v0.9+ has **replaced the `mmif` interface** with a more flexible `file_view` abstraction. To create a memory-backed filesystem, we must:

1. Implement a custom `file_view_impl` class
2. Wrap it in a `dwarfs::file_view` object
3. Pass the `file_view` to `filesystem_v2_lite` constructor

---

## Key Classes

### `dwarfs::file_view`

A lightweight wrapper around `file_view_impl` that provides:
- Zero-copy access via `raw_bytes()`
- Segmented reading via `segment_at()`
- File extent information
- Size and path metadata

**Location**: `/Users/mulgogi/src/external/dwarfs/include/dwarfs/file_view.h`

### `dwarfs::detail::file_view_impl`

Abstract interface that implementations must inherit from.

**Required Virtual Methods**:

```cpp
virtual file_size_t size() const = 0;
virtual std::filesystem::path const& path() const = 0;
virtual file_segment segment_at(file_range range) const = 0;
virtual file_extents_iterable extents(std::optional<file_range> range) const = 0;
virtual bool supports_raw_bytes() const noexcept = 0;
virtual std::span<std::byte const> raw_bytes() const = 0;
virtual void copy_bytes(void* dest, file_range range, std::error_code& ec) const = 0;
virtual size_t default_segment_size() const = 0;
virtual void release_until(file_off_t offset, std::error_code& ec) const = 0;
```

**Location**: `/Users/mulgogi/src/external/dwarfs/include/dwarfs/detail/file_view_impl.h`

### Additional Support Classes

- **`file_segment`**: Represents a contiguous memory region
- **`file_segment_impl`**: Abstract interface for segment implementations
- **`file_range`**: Offset + size pair
- **`file_extents_iterable`**: Iterator over file extents

---

## Reference Implementation: Test Mock

The test suite provides a complete example in `mmap_mock.cpp`:

**Location**: `/Users/mulgogi/src/external/dwarfs/test/mmap_mock.cpp`

### Key Patterns:

1. **Class Declaration**:
```cpp
class mmap_mock final : public detail::file_view_impl,
                        public std::enable_shared_from_this<mmap_mock> {
  // Implementation
};
```

2. **Memory Storage**:
```cpp
std::string data_;  // Holds the actual memory buffer
```

3. **Zero-Copy Access**:
```cpp
std::span<std::byte const> raw_bytes() const override {
  return {reinterpret_cast<std::byte const*>(data_.data()), data_.size()};
}
```

4. **Factory Function**:
```cpp
file_view make_mock_file_view(std::string data, 
                              mock_file_view_options const& opts) {
  return file_view{std::make_shared<mmap_mock>(std::move(data), opts)};
}
```

---

## Filesystem Integration

### Constructor Signature:

```cpp
filesystem_v2_lite(logger& lgr, 
                  os_access const& os, 
                  file_view const& mm);
```

**Location**: `/Users/mulgogi/src/external/dwarfs/include/dwarfs/reader/filesystem_v2.h:96`

### Usage Pattern:

```cpp
// 1. Create file_view_impl
auto impl = std::make_shared<memory_file_view_impl>(data, size);

// 2. Wrap in file_view
dwarfs::file_view view{impl};

// 3. Create filesystem
dwarfs::reader::filesystem_v2_lite fs{lgr, os, view};
```

---

## Migration Strategy

### Old Approach (mmif-based):
```cpp
// tebako-mfs.h - OBSOLETE
class mfs {
  dwarfs::mmif mmif_;  // No longer exists!
};
```

### New Approach (file_view-based):
```cpp
// include/tebako/fs/internal/memory_file_view.h - NEW
class memory_file_view_impl : public dwarfs::detail::file_view_impl {
  const void* data_;
  size_t size_;
  // Implement all required methods
};
```

---

## Implementation Requirements

### 1. Memory File View Implementation

**File**: `include/tebako/fs/internal/memory_file_view.h`

- Inherit from `dwarfs::detail::file_view_impl`
- Implement all 9 required virtual methods
- Support zero-copy via `raw_bytes()`
- Handle const-correctness (read-only memory)

### 2. Memory File Segment Implementation

**File**: `include/tebako/fs/internal/memory_file_segment.h`

- Inherit from `dwarfs::detail::file_segment_impl`
- Provide memory slice access
- Support segment operations

### 3. Update memfs Class

**File**: `include/tebako/fs/memfs.h`

- Change from `std::shared_ptr<mfs>` to `dwarfs::file_view`
- Update `load()` method to create file_view
- Ensure zero-copy memory access is maintained

---

## Critical Design Considerations

### 1. Zero-Copy Access ✅

The `raw_bytes()` method MUST return a direct span to the original buffer:
```cpp
std::span<std::byte const> raw_bytes() const override {
  return {reinterpret_cast<std::byte const*>(data_), size_};
}
```

**DO NOT** copy the buffer!

### 2. Thread Safety ✅

All methods MUST be const and thread-safe:
```cpp
virtual file_size_t size() const = 0;  // const = thread-safe
```

No mutable state unless protected by mutex.

### 3. Lifetime Management ✅

Use `std::enable_shared_from_this` to ensure proper lifecycle:
```cpp
class memory_file_view_impl : public detail::file_view_impl,
                               public std::enable_shared_from_this<memory_file_view_impl>
```

### 4. Extents Handling

For a simple memory buffer, use a single extent:
```cpp
file_extents_iterable extents(std::optional<file_range> range) const override {
  std::vector<detail::file_extent_info> extents = {
    {extent_kind::data, file_range{0, size_}}
  };
  return {shared_from_this(), extents, range.value_or(file_range{0, size_})};
}
```

---

## Testing Validation

After implementation, verify:

1. **Compilation**: All targets build without errors
2. **Memory Safety**: Zero leaks detected via `leaks --atExit`
3. **Functional**: All 6 memory mounting tests pass
4. **Thread Safety**: Stress test with 100 iterations
5. **Zero-Copy**: Verify no memcpy in hot path (via profiling)

---

## References

- **DwarFS Headers**: `/Users/mulgogi/src/external/dwarfs/include/dwarfs/`
- **Test Examples**: `/Users/mulgogi/src/external/dwarfs/test/mmap_mock.cpp`
- **Implementation**: `/Users/mulgogi/src/external/dwarfs/src/internal/`

---

## Conclusion

The migration from `mmif` to `file_view` requires:
- Creating a custom `file_view_impl` implementation
- Implementing 9 virtual methods
- Maintaining zero-copy and thread-safe design
- Following the established pattern from test mocks

This is a **proper architectural solution**, not a hack. The result will be maintainable, extensible, and fully compatible with modern dwarFS.