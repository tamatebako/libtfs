# DwarFS vcpkg Port Build Issue - Bug Report

**Date**: 2025-12-27
**Severity**: High - Blocks compilation
**Affected Component**: DwarFS vcpkg port CMake configuration
**Platform**: macOS arm64 (likely affects all platforms)

---

## Summary

The `dwarfs_tool_support` CMake target is missing the `DWARFS_BUILD_ID` compiler definition, causing compilation failure when building with vcpkg.

## Error Details

**File**: `/Users/mulgogi/src/external/dwarfs/tools/src/tool/tool.cpp`
**Line**: 118
**Error**:
```
error: use of undeclared identifier 'DWARFS_BUILD_ID'; did you mean 'DWARFS_GIT_ID'?
  118 |       tool_name, DWARFS_GIT_ID, date, extra_info, DWARFS_BUILD_ID);
      |                                                   ^~~~~~~~~~~~~~~
      |                                                   DWARFS_GIT_ID
```

**Build Command**:
```bash
/usr/bin/c++ -DBOOST_CHRONO_NO_LIB ... -DDWARFS_HAVE_FLATBUFFERS=1 \
  -I/Users/mulgogi/src/external/dwarfs/tools/include \
  -fPIC -g -std=gnu++20 -arch arm64 -fPIC \
  -o CMakeFiles/dwarfs_tool_support.dir/tools/src/tool/tool.cpp.o \
  -c /Users/mulgogi/src/external/dwarfs/tools/src/tool/tool.cpp
```

**Note**: No `-DDWARFS_BUILD_ID="..."` in the compile flags.

## Root Cause

The `dwarfs_tool_support` target is built with different compiler definitions than `dwarfs_tool`:

### Current Configuration (BROKEN)

**dwarfs_tool target** (WORKS):
```cmake
target_compile_definitions(dwarfs_tool PRIVATE
  DWARFS_BUILD_ID="arm64 Darwin using AppleClang 17.0.0.17000603"
  DWARFS_USE_JEMALLOC
  # ... other definitions
)
```

**dwarfs_tool_support target** (FAILS):
```cmake
# Missing DWARFS_BUILD_ID definition!
target_compile_definitions(dwarfs_tool_support PRIVATE
  # Only has DWARFS_HAVE_FLATBUFFERS=1
)
```

## Reproduction Steps

1. Clone DwarFS repository
2. Configure with vcpkg:
   ```bash
   cmake -B build \
     -DCMAKE_BUILD_TYPE=Debug \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
   ```
3. Build:
   ```bash
   cmake --build build --target dwarfs_tool_support
   ```
4. Observe compilation failure in `tools/src/tool/tool.cpp`

## Proposed Fix

### Option 1: Add DWARFS_BUILD_ID to dwarfs_tool_support (Recommended)

In the DwarFS CMakeLists.txt (or vcpkg portfile), ensure `dwarfs_tool_support` has the same build definitions as `dwarfs_tool`:

```cmake
# Generate build ID string
set(BUILD_ID_STRING "${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_SYSTEM_NAME} using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# Apply to both targets
foreach(TARGET dwarfs_tool dwarfs_tool_support)
  target_compile_definitions(${TARGET} PRIVATE
    DWARFS_BUILD_ID="${BUILD_ID_STRING}"
    $<$<BOOL:${USE_JEMALLOC}>:DWARFS_USE_JEMALLOC>
  )
endforeach()
```

### Option 2: Make DWARFS_BUILD_ID Optional in Code

Modify `tools/src/tool/tool.cpp` to handle missing `DWARFS_BUILD_ID`:

```cpp
#ifndef DWARFS_BUILD_ID
#define DWARFS_BUILD_ID "unknown"
#endif

// Then use DWARFS_BUILD_ID safely
std::string version_string = fmt::format("...", DWARFS_BUILD_ID);
```

## Impact

**High Priority**: This blocks compilation of DwarFS on all platforms when using vcpkg or when the build system doesn't explicitly define `DWARFS_BUILD_ID` for `dwarfs_tool_support`.

**Workaround**: Manually add the definition to the build command or skip building `dwarfs_tool_support`.

## Environment

- **Platform**: macOS Sequoia (arm64)
- **Compiler**: AppleClang 17.0.0.17000603
- **CMake**: 4.1.2
- **vcpkg**: Latest
- **DwarFS Version**: 0.16.0 (from vcpkg port)
- **Build Type**: Debug

## Related Files

- `/Users/mulgogi/src/external/dwarfs/vcpkg_ports/dwarfs/portfile.cmake`
- `/Users/mulgogi/src/external/dwarfs/CMakeLists.txt`
- `/Users/mulgogi/src/external/dwarfs/tools/src/tool/tool.cpp`

## Verification

After applying the fix, verify with:

```bash
cd /Users/mulgogi/src/external/vcpkg/buildtrees/dwarfs/arm64-osx-dbg
grep -r "DWARFS_BUILD_ID" CMakeFiles/dwarfs_tool_support.dir/flags.make

# Should show:
# CXX_DEFINES = ... -DDWARFS_BUILD_ID="arm64 Darwin using AppleClang 17.0.0.17000603" ...
```

## Additional Context

This issue was encountered while integrating DwarFS with the libtfs (Tebako File System) library via vcpkg. The libtfs project successfully builds all other components and tests, but the DwarFS dependency rebuild triggered by vcpkg fails due to this missing definition.

The `dwarfs_tool` target compiles successfully with the definition, but `dwarfs_tool_support` (which shares the same source file `tool.cpp`) fails because it lacks the definition.

---

## Recommendation

**Fix Priority**: High
**Proposed Solution**: Option 1 (Add definition to CMake)
**Estimated Effort**: 5 minutes

This is a straightforward CMake configuration issue that can be fixed by ensuring both targets receive the same compiler definitions.

---

## Issue 2: Missing Internal Headers Installation

**Status**: CRITICAL - Blocks libtfs compilation

### Error

```
fatal error: 'dwarfs/internal/packed_ptr.h' file not found
   50 | #include <dwarfs/internal/packed_ptr.h>
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
```

### Root Cause

The DwarFS vcpkg port is not installing internal headers, only public API headers. However, `metadata_types_flatbuffers.h` (which IS installed) requires `internal/packed_ptr.h`.

**File Locations:**
- **Source**: `/Users/mulgogi/src/external/dwarfs/include/dwarfs/internal/packed_ptr.h` ✅ EXISTS
- **Installed**: `build/vcpkg_installed/arm64-osx/include/dwarfs/internal/` ❌ MISSING

### Proposed Fix

In the DwarFS vcpkg port installation rules (portfile.cmake or CMakeLists.txt), ensure internal headers are installed:

```cmake
# Install public headers
install(DIRECTORY include/dwarfs
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
  # DO NOT exclude internal/ directory if public headers depend on it
)
```

Or more specifically:

```cmake
# Install all necessary headers
install(DIRECTORY
  include/dwarfs/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dwarfs
  FILES_MATCHING PATTERN "*.h"
)
```

### Impact

**CRITICAL**: Blocks all consumers that include DwarFS reader headers (like libtfs).

### Verification

After fix, verify with:

```bash
ls build/vcpkg_installed/arm64-osx/include/dwarfs/internal/
# Should show: packed_ptr.h and other internal headers
```

### Recommendation

**Fix Priority**: CRITICAL
**Proposed Solution**: Install internal headers directory
**Estimated Effort**: 2 minutes

The DwarFS vcpkg port must install ALL headers that public headers depend on, including those in the `internal/` directory.