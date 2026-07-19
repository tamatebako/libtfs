# DwarFS Backend Update Plan

**Date**: 2025-01-18
**Status**: Draft
**Purpose**: Update libtfs DwarFS backend to match external Tebako dwarfs repository API changes

## Executive Summary

The external DwarFS repository (the tebako dwarfs fork) has undergone significant API modernization. The current `DwarfsBackend` implementation in libtfs needs updates to match the new API signatures and patterns.

### Critical Finding: `filesystem_v2_lite` vs `filesystem_v2`

**IMPORTANT**: The `filesystem_loader::load()` method returns `filesystem_v2_lite`, NOT `filesystem_v2`.

- `filesystem_v2_lite`: Core filesystem operations (find, getattr, read, opendir, readdir, etc.)
- `filesystem_v2`: Inherits from `filesystem_v2_lite`, adds extra features (check, dump, info_as_json, etc.)

**Decision**: We have two options:
1. **Use `filesystem_v2_lite` directly** (simpler, recommended)
2. **Use `filesystem_v2` with direct constructor** (keeps current type, doesn't use loader)

For most use cases, `filesystem_v2_lite` provides all the functionality we need. The `filesystem_v2` additional features are primarily for introspection/diagnostics which we don't use.

## API Changes Summary

### 1. Constructor Signatures Changed

**Current Implementation** (needs update):
```cpp
// Current - using filesystem_v2 with 3 parameters
dwarfs::reader::filesystem_options opts;
opts.image_offset = dwarfs::reader::filesystem_options::IMAGE_OFFSET_AUTO;
fs_ = std::make_unique<dwarfs::reader::filesystem_v2>(
    *logger_, os, std::filesystem::path(archive_path), opts);
```

**Option A: Use `filesystem_v2_lite` with filesystem_loader** (RECOMMENDED):
```cpp
// filesystem_loader returns filesystem_v2_lite
dwarfs::reader::filesystem_load_config config;
config.image_path = archive_path;
config.cache_size = 512 << 20;  // 512 MiB default
config.block_size = 512 << 10;   // 512 KiB default
config.num_workers = 2;

// Store as filesystem_v2_lite (change member type)
fs_ = std::make_unique<dwarfs::reader::filesystem_v2_lite>(
    dwarfs::reader::filesystem_loader::load(*logger_, os, config));
```

**Option B: Use `filesystem_v2` direct constructor** (if we keep current type):
```cpp
// Direct constructor with 4 parameters
dwarfs::reader::filesystem_options opts;
opts.image_offset = 0;
opts.image_size = std::numeric_limits<file_off_t>::max();

fs_ = std::make_unique<dwarfs::reader::filesystem_v2>(
    *logger_, os, std::filesystem::path(archive_path), opts);
```

### 2. inode_view API Changes

**Old Method** (current - WRONG):
```cpp
// Current implementation uses non-existent method
int bytes_read = fs_.read(inode_.inode_num(), ...);  // WRONG
```

**Correct Method**:
```cpp
// inode_view has inode_num() method (returns uint32_t)
uint32_t inode_num = inode_.inode_num();
int bytes_read = fs_.read(inode_num, ...);
```

### 3. dir_entry_view API Usage

**Current Implementation** (needs verification):
```cpp
auto entry = fs_->find(normalized);
if (entry) {
    return entry->inode();  // Returns inode_view object
}
```

**Correct Usage**:
```cpp
// find() returns std::optional<dir_entry_view>
auto entry = fs_->find(normalized);
if (entry) {
    // entry->inode() returns inode_view object (CORRECT)
    return entry->inode();
}
```

### 4. getattr() Error Handling

**Current Implementation** (inconsistent):
```cpp
// Some places pass ec, some don't
auto stat = fs_.getattr(*inode_opt, ec);
```

**Correct Pattern** (consistent):
```cpp
// Always use std::error_code for non-throwing
std::error_code ec;
auto stat = fs_->getattr(*inode_opt, ec);
if (ec) {
    // Handle error
    return nullptr;
}
```

## Detailed File Changes

### File: `include/tebako/fs/backends/dwarfs_backend.h`

**Status**: ✅ No header changes needed - public API remains compatible

The header file uses PIMPL pattern and abstract interfaces, so no changes are required to the public API.

### File: `src/backends/dwarfs_backend.cpp`

#### Change 1: Update Constructor Calls (Lines 318-337)

**Current Code**:
```cpp
bool mount_file(const std::string& archive_path) {
    if (is_mounted_) {
        return false;
    }

    try {
        dwarfs::reader::filesystem_options opts;
        opts.image_offset = dwarfs::reader::filesystem_options::IMAGE_OFFSET_AUTO;

        dwarfs::os_access_generic os;

        // Create filesystem from file (opts passed as const&)
        fs_ = std::make_unique<dwarfs::reader::filesystem_v2>(
            *logger_, os, std::filesystem::path(archive_path), opts);

        is_mounted_ = true;
        return true;
    } catch (...) {
        return false;
    }
}
```

**Updated Code**:
```cpp
bool mount_file(const std::string& archive_path) {
    if (is_mounted_) {
        return false;
    }

    try {
        // Use filesystem_loader for cleaner initialization
        dwarfs::reader::filesystem_load_config config;
        config.image_path = archive_path;
        config.cache_size = 512 << 20;  // 512 MiB default
        config.block_size = 512 << 10;   // 512 KiB default
        config.num_workers = 2;
        config.image_offset = std::nullopt;  // Auto-detect

        dwarfs::os_access_generic os;

        // Create filesystem using filesystem_loader
        fs_ = std::make_unique<dwarfs::reader::filesystem_v2>(
            dwarfs::reader::filesystem_loader::load(*logger_, os, config));

        is_mounted_ = true;
        return true;
    } catch (const std::exception& e) {
        // Log error for debugging
        return false;
    }
}
```

#### Change 2: Update Memory Mount (Lines 340-366)

**Current Code**:
```cpp
bool mount_memory(const void* data, size_t size) {
    if (is_mounted_) {
        return false;
    }

    try {
        dwarfs::reader::filesystem_options opts;
        opts.image_offset = 0;  // Memory buffer starts at offset 0

        // Create memory file view using our internal implementation
        auto mem_view = std::make_shared<tebako::memory_file_view_impl>(
            data, size, "/__tebako_dwarfs__");
        dwarfs::file_view view{mem_view};

        dwarfs::os_access_generic os;

        // Create filesystem from memory (opts passed as const&)
        fs_ = std::make_unique<dwarfs::reader::filesystem_v2>(
            *logger_, os, view, opts);

        is_mounted_ = true;
        return true;
    } catch (const std::exception& e) {
        // Log error for debugging
        return false;
    }
}
```

**Updated Code**:
```cpp
bool mount_memory(const void* data, size_t size) {
    if (is_mounted_) {
        return false;
    }

    if (!data || size == 0) {
        return false;
    }

    try {
        // Create memory file view using our internal implementation
        auto mem_view = std::make_shared<tebako::memory_file_view_impl>(
            data, size, "/__tebako_dwarfs__");
        dwarfs::file_view view{mem_view};

        dwarfs::os_access_generic os;

        // Create filesystem from memory view
        // Note: filesystem_loader doesn't support memory views directly
        // Use direct constructor with filesystem_options
        dwarfs::reader::filesystem_options opts;
        opts.image_offset = 0;
        opts.image_size = size;

        fs_ = std::make_unique<dwarfs::reader::filesystem_v2>(
            *logger_, os, view, opts);

        is_mounted_ = true;
        return true;
    } catch (const std::exception& e) {
        // Log error for debugging
        return false;
    }
}
```

#### Change 3: Fix DwarfsFileHandle read() method (Line 114)

**Current Code**:
```cpp
ssize_t read(void* buffer, size_t count) override {
    // ...
    try {
        // Use DwarFS reader's read function
        int bytes_read = fs_.read(inode_.inode_num(),  // <-- This is CORRECT
                                  static_cast<char*>(buffer),
                                  to_read,
                                  current_pos_);
        // ...
    }
}
```

**Status**: ✅ This is actually CORRECT - `inode_num()` is the right method

No change needed here. The implementation already uses `inode_num()` correctly.

#### Change 4: Verify DwarfsDirectoryIterator (Lines 220-297)

**Current Code**:
```cpp
DwarfsDirectoryIterator(dwarfs::reader::filesystem_v2& fs,
                       dwarfs::reader::inode_view dir_inode)
    : fs_(fs), current_index_(0) {

    try {
        // Get directory entries using DwarFS API
        auto dir = fs_.opendir(dir_inode);
        if (dir) {
            size_t offset = 0;
            while (true) {
                auto entry = fs_.readdir(*dir, offset++);
                if (!entry) break;  // End of directory

                // Skip ".", "..", and empty entries
                std::string name = entry->name();
                if (name.empty() || name == "." || name == "..") {
                    continue;
                }

                DirectoryEntry de;
                de.name = name;

                // Get the inode for this entry
                auto child_inode = entry->inode();  // <-- Returns inode_view
                std::error_code ec;
                auto stat = fs_.getattr(child_inode, ec);  // <-- Pass inode_view directly

                if (!ec) {
                    de.is_directory = S_ISDIR(stat.mode());
                    de.size = stat.size();
                    de.mtime = stat.mtime();
                }

                entries_.push_back(de);
            }
        }
    } catch (...) {
        // If we fail to read directory, leave entries empty
    }
}
```

**Status**: ✅ This looks CORRECT

- `entry->inode()` returns `inode_view` (correct)
- `fs_.getattr(child_inode, ec)` accepts `inode_view` (correct)

However, we should verify the `opendir` and `readdir` usage. Let me check if `directory_view` is the correct type.

**Potential Update Needed**:
```cpp
// Need to verify if opendir() returns std::optional<directory_view>
std::optional<dwarfs::reader::directory_view> dir = fs_.opendir(dir_inode);
```

#### Change 5: Update find_inode() method (Lines 383-414)

**Current Code**:
```cpp
std::optional<dwarfs::reader::inode_view> find_inode(const std::string& path) {
    if (!fs_) {
        return std::nullopt;
    }

    try {
        // Normalize path for DwarFS lookup
        std::string normalized = path;
        if (normalized.empty() || normalized == "/") {
            // Return root inode - find root directory
            auto entry = fs_->find("/");
            if (entry) {
                return entry->inode();  // <-- Returns inode_view (CORRECT)
            }
            return std::nullopt;
        }

        // Remove leading slash
        if (normalized.front() == '/') {
            normalized = normalized.substr(1);
        }

        // Find the entry and extract inode
        auto entry = fs_->find(normalized);
        if (entry) {
            return entry->inode();  // <-- Returns inode_view (CORRECT)
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}
```

**Status**: ✅ This is CORRECT

The `find()` method returns `std::optional<dir_entry_view>` and `entry->inode()` returns `inode_view`.

#### Change 6: Add Missing Headers

**Add to includes**:
```cpp
#include <dwarfs/reader/filesystem_loader.h>  // For filesystem_load_config
```

## Implementation Checklist

- [ ] **Step 1**: Update `mount_file()` to use `filesystem_loader`
- [ ] **Step 2**: Update `mount_memory()` with proper constructor signature
- [ ] **Step 3**: Add `filesystem_loader.h` include
- [ ] **Step 4**: Verify `opendir`/`readdir` return types
- [ ] **Step 5**: Test file-based mounting
- [ ] **Step 6**: Test memory-based mounting
- [ ] **Step 7**: Verify directory listing works
- [ ] **Step 8**: Run full test suite
- [ ] **Step 9**: Update documentation if needed

## Testing Strategy

### Unit Tests to Update

1. **test_dwarfs_backend.cpp** - Verify all tests still pass
2. **test_integration.cpp** - Verify end-to-end workflows

### Test Fixtures Needed

Create DwarFS test archives:
```bash
cd tests/fixtures/dwarfs
./generate_dwarfs_fixtures.sh  # If exists, or create
```

### Manual Testing Commands

```bash
# Build with tests
cmake --build build

# Run DwarFS-specific tests
ctest -R test_dwarfs --verbose

# Test tebakofs CLI with DwarFS
./build/tebakofs info /path/to/test.dwarfs
./build/tebakofs ls /path/to/test.dwarfs
./build/tebakofs cat /path/to/test.dwarfs /file.txt
```

## Potential Issues and Mitigations

### Issue 1: filesystem_loader API Compatibility

**Problem**: The `filesystem_loader::load()` might have subtle differences in behavior.

**Mitigation**:
- Test with real DwarFS archives
- Compare behavior before/after changes
- Fall back to direct constructor if loader has issues

### Issue 2: Memory Mount Configuration

**Problem**: `filesystem_load_config` uses `image_path` which doesn't work for memory buffers.

**Mitigation**:
- Keep direct constructor for memory mounts
- Only use `filesystem_loader` for file-based mounts

### Issue 3: Error Handling

**Problem**: New API uses `std::error_code` consistently - need to ensure proper error propagation.

**Mitigation**:
- Check all `getattr()` calls use `std::error_code`
- Ensure error codes are properly checked

## API Reference Quick Guide

### filesystem_loader Usage

```cpp
#include <dwarfs/reader/filesystem_loader.h>

// Create config
dwarfs::reader::filesystem_load_config config;
config.image_path = "/path/to/archive.dwarfs";
config.cache_size = 512 << 20;   // 512 MiB
config.block_size = 512 << 10;    // 512 KiB
config.num_workers = 2;

// Load filesystem
dwarfs::os_access_generic os;
dwarfs::stream_logger logger;
auto fs = dwarfs::reader::filesystem_loader::load(logger, os, config);
```

### Direct Constructor Usage

```cpp
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/filesystem_options.h>

// Create options
dwarfs::reader::filesystem_options opts;
opts.image_offset = 0;
opts.image_size = std::numeric_limits<file_off_t>::max();

// Create filesystem
dwarfs::os_access_generic os;
dwarfs::stream_logger logger;
auto fs = dwarfs::reader::filesystem_v2(logger, os, "/path/to/archive.dwarfs", opts);
```

## Related Files to Review

1. `<dwarfs-repo>/include/dwarfs/reader/filesystem_v2.h`
2. `<dwarfs-repo>/include/dwarfs/reader/filesystem_loader.h`
3. `<dwarfs-repo>/include/dwarfs/reader/filesystem_options.h`
4. `<dwarfs-repo>/include/dwarfs/reader/internal/metadata_types.h`

## Dependencies

This update requires the external dwarfs repository to be properly linked and available during compilation.

---

**Next Steps**:
1. Review this plan with the team
2. Implement the changes incrementally
3. Test thoroughly at each step
4. Update documentation as needed
