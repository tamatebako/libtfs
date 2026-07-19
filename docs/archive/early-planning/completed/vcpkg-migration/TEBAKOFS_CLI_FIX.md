# tebakofs CLI - API Updates Required

**Status**: The tebakofs CLI was written against an older API  
**Action Needed**: Update to use current unified interface

## Required Changes

### 1. Add Missing Includes

**File**: [`include/tebako/fs/cli/tebakofs.h`](../include/tebako/fs/cli/tebakofs.h)

Add these includes:
```cpp
#include <tebako/fs/directory_iterator.h>
#include <tebako/fs/file_handle.h>
```

### 2. Fix API Calls

**File**: [`src/cli/tebakofs.cpp`](../src/cli/tebakofs.cpp)

#### Line 246: Change detect_and_create to create_from_file
```cpp
// OLD:
auto backend = BackendFactory::detect_and_create(path);

// NEW:
auto backend = BackendFactory::create_from_file(path);
```

#### Lines 271, 395, 428, 471, 529: Fix string concatenation
```cpp
// OLD:
std::string full_path = "/mnt" + (path.empty() || path[0] != '/' ? "/" : "") + path;

// NEW:
std::string full_path = std::string("/mnt") + (path.empty() || path[0] != '/' ? "/" : "") + path;
```

#### Lines 407, 571: Change open_file to open
```cpp
// OLD:
auto handle = fs->open_file(full_path);

// NEW:
auto handle = fs->open(full_path, O_RDONLY);
```

#### Add missing include in cpp file:
```cpp
#include <fcntl.h>  // For O_RDONLY
#include <sstream>  // For ostringstream
```

### 3. Fix DirectoryEntry.mode Field

**Line 287**: DirectoryEntry doesn't have a `mode` field - remove this line:
```cpp
// REMOVE:
entry.mode = 0644;  // Default for file
```

**Lines 336-341**: Update print_entry to use default permissions:
```cpp
void TebakofsCLI::print_entry(const DirectoryEntry& entry,
                              const std::string& path,
                              bool long_format) {
  if (long_format) {
    mode_t mode = entry.is_directory ? 0755 : 0644;
    std::cout << format_permissions(mode)
              << "  " << std::setw(10) << format_size(entry.size)
              << "  " << format_time(entry.mtime)
              << "  " << path << std::endl;
  } else {
    std::cout << path << std::endl;
  }
}
```

## Summary

The tebakofs CLI needs these updates to work with the current unified interface:
- ✅ Add proper includes
- ✅ Change `detect_and_create` → `create_from_file`
- ✅ Fix string concatenation (cast const char* to std::string)
- ✅ Change `open_file` → `open` with `O_RDONLY` flag
- ✅ Remove `entry.mode` field usage
- ✅ Use default permissions in `print_entry`

**Estimated Time**: 10 minutes to apply all fixes properly