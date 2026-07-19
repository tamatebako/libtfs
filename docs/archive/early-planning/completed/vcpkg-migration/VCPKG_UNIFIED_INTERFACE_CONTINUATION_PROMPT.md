# vcpkg Unified Interface - Continuation Prompt

**Date**: 2025-12-27
**Status**: Ready for Phase 2 Implementation
**Priority**: P1 - Final Phase
**Estimated Time**: 4-6 hours

---

## 🎯 Your Mission

Complete the libtfs vcpkg integration by creating a working example application with vcpkg overlay port and ensuring the unified interface works seamlessly across all backends.

**Current State**:
- ✅ vcpkg integration complete for DwarFS backend
- ✅ All 45 DwarFS backend tests passing (100%)
- ✅ Code cleaned up and production-ready
- ⚠️ Need vcpkg overlay port for libtfs
- ⚠️ Need working example with vcpkg
- ⚠️ Need unified interface verification

**Your Task**:
1. Create vcpkg overlay port for libtfs (1.5 hours)
2. Create example application demonstrating unified interface (2 hours)
3. Verify unified interface across all backends (1 hour)
4. Update official documentation (0.5 hours)

---

## 📋 Implementation Steps

### Phase 1: Create vcpkg Overlay Port (1.5 hours)

Create the following structure in [`vcpkg_ports/libtfs/`](../vcpkg_ports/libtfs/):

```
vcpkg_ports/libtfs/
├── portfile.cmake
├── vcpkg.json
└── usage
```

**Key Files to Create**:

1. **portfile.cmake** - See [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:158-179`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:158) for template

2. **vcpkg.json** - See [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:185-210`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:185) for template

3. **usage** - See [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:216-231`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:216) for template

4. **CMake config** - Add to [`CMakeLists.txt:520`](../CMakeLists.txt:520) the installation rules from [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:268-292`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:268)

**Verification**:
```bash
# Test the port locally
vcpkg install libtfs --overlay-ports=vcpkg_ports
```

---

### Phase 2: Create Example Application (2 hours)

Create the following in [`examples/vcpkg_example/`](../examples/vcpkg_example/):

```
examples/vcpkg_example/
├── CMakeLists.txt
├── vcpkg.json
├── vcpkg-configuration.json
├── main.cpp
├── README.md
├── create_test_archives.sh
└── test_archives/
```

**Key Files**:

1. **main.cpp** - Complete example demonstrating:
   - Backend creation via factory
   - Auto-format detection
   - Mounting archives
   - Directory traversal
   - File reading and seeking
   - Metadata access
   - See [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:358-570`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:358) for full implementation

2. **CMakeLists.txt** - See [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:328-349`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:328)

3. **vcpkg.json** - See [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:306-313`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:306)

4. **vcpkg-configuration.json** - See [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:319-335`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:319)

5. **create_test_archives.sh** - See [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:621-663`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:621)

**Build and Run**:
```bash
cd examples/vcpkg_example
chmod +x create_test_archives.sh
./create_test_archives.sh

mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build .
./archive_reader
```

**Expected Output**: The example should successfully:
- Detect archive formats (DwarFS, ZIP)
- Mount each archive
- List directory contents
- Read files
- Display metadata
- Demonstrate seek operations

---

### Phase 3: Verify Unified Interface (1 hour)

#### Create Unified Interface Test

**File**: [`tests/test_unified_interface.cpp`](../tests/test_unified_interface.cpp) (new)

**Test Cases** (see [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:114-149`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:114)):

1. `CreateBackendFromFile` - Test all backends create correctly
2. `IdenticalAPIBehavior` - Verify identical API across backends
3. `PolymorphicBehavior` - Test runtime polymorphism

#### Update CMakeLists.txt

Add to [`CMakeLists.txt:746`](../CMakeLists.txt:746):

```cmake
# Unified interface tests
add_executable(test_unified_interface
  "tests/test_unified_interface.cpp"
)
target_link_libraries(test_unified_interface PRIVATE
  tfs
  tebako_dirent_helper_c
  GTest::gtest
  GTest::gtest_main
)
gtest_add_tests(TARGET test_unified_interface)
```

#### Run Tests

```bash
cd build
cmake --build . --target test_unified_interface
./test_unified_interface
```

---

### Phase 4: Update Documentation (0.5 hours)

#### 4.1: Create VCPKG_INTEGRATION.md

**File**: [`docs/VCPKG_INTEGRATION.md`](../docs/VCPKG_INTEGRATION.md) (new)

**Content**:
```markdown
# vcpkg Integration Guide

## Overview

libtfs is fully integrated with vcpkg, providing a modern, cross-platform package management solution.

## Prerequisites

- CMake 3.24+
- C++20 compiler
- vcpkg

## Using libtfs in Your Project

### Method 1: vcpkg manifest mode (Recommended)

Create `vcpkg.json`:
```json
{
  "dependencies": [
    "libtfs"
  ]
}
```

Create `vcpkg-configuration.json`:
```json
{
  "default-registry": {
    "kind": "git",
    "baseline": "...",
    "repository": "https://github.com/microsoft/vcpkg"
  },
  "overlay-ports": [
    "path/to/libdwarfs/vcpkg_ports"
  ]
}
```

In `CMakeLists.txt`:
```cmake
find_package(libtfs CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE libtfs::tfs)
```

### Method 2: Classic mode

```bash
vcpkg install libtfs --overlay-ports=/path/to/libdwarfs/vcpkg_ports
```

## Supported Backends

- **DwarFS** (.dwarfs) - High compression, fast decompression
- **ZIP** (.zip) - Universal compatibility
- **SquashFS** (.squashfs) - Linux standard (coming soon)

## Example

See [`examples/vcpkg_example/`](../examples/vcpkg_example/) for a complete working example.

## API Reference

### Backend Creation

```cpp
#include <tebako/fs/backend_factory.h>

// Auto-detect format
auto backend = BackendFactory::create_from_file("archive.dwarfs");

// From memory
auto backend = BackendFactory::create_from_memory(data, size, "/mnt");
```

### Common Operations

```cpp
// Mount
backend->mount(archive_path, "/mnt");

// Check file existence
bool exists = backend->exists("/mnt/file.txt");

// Open and read file
auto handle = backend->open("/mnt/file.txt", O_RDONLY);
char buffer[1024];
ssize_t bytes = handle->read(buffer, sizeof(buffer));

// List directory
auto iter = backend->list_directory("/mnt");
while (iter->has_next()) {
  auto entry = iter->next();
  std::cout << entry.name << "\n";
}

// Get metadata
auto size = backend->file_size("/mnt/file.txt");
auto mtime = backend->modification_time("/mnt/file.txt");
auto perms = backend->permissions("/mnt/file.txt");

// Unmount
backend->unmount();
```

## Troubleshooting

### Build Issues

If you encounter build issues:

1. Ensure vcpkg is up to date: `git pull` in vcpkg directory
2. Clear vcpkg cache: `vcpkg remove --outdated --recurse`
3. Rebuild: `cmake --build build --clean-first`

### Runtime Issues

If archive mounting fails:

1. Verify archive format is supported
2. Check file permissions
3. Ensure archive is not corrupted

## Links

- [Example Application](../examples/vcpkg_example/)
- [API Documentation](API.md)
- [Backend Documentation](backends/)
```

#### 4.2: Update README.adoc

Add to [`README.adoc`](../README.adoc:1) after the "Purpose" section:

```adoc
== vcpkg Integration

libtfs is fully integrated with vcpkg for modern dependency management:

[source,json]
----
{
  "dependencies": [
    "libtfs"
  ]
}
----

See link:docs/VCPKG_INTEGRATION.md[vcpkg Integration Guide] and
link:examples/vcpkg_example/[Example Application] for details.
```

#### 4.3: Move Completed Documentation

Move to [`old-docs/completed/vcpkg-migration/`](../old-docs/completed/vcpkg-migration/):

```bash
mkdir -p old-docs/completed/vcpkg-migration
mv docs/VCPKG_MIGRATION_STATUS.md old-docs/completed/vcpkg-migration/
mv docs/VCPKG_MIGRATION_CONTINUATION_PLAN.md old-docs/completed/vcpkg-migration/
mv docs/VCPKG_MIGRATION_CONTINUATION_PROMPT.md old-docs/completed/vcpkg-migration/
```

---

## 📁 Key Files

### Implementation Files
- [`src/backend_factory.cpp`](../src/backend_factory.cpp:1) - Factory implementation
- [`src/backends/zip_backend.cpp`](../src/backends/zip_backend.cpp:1) - ZIP backend
- [`src/backends/dwarfs_backend.cpp`](../src/backends/dwarfs_backend.cpp:1) - DwarFS backend
- [`CMakeLists.txt`](../CMakeLists.txt:1) - Build configuration

### New Files to Create
- `vcpkg_ports/libtfs/portfile.cmake` - vcpkg port definition
- `vcpkg_ports/libtfs/vcpkg.json` - Port manifest
- `vcpkg_ports/libtfs/usage` - Usage instructions
- `cmake/libtfsConfig.cmake.in` - CMake config template
- `examples/vcpkg_example/main.cpp` - Example application
- `examples/vcpkg_example/CMakeLists.txt` - Example build config
- `examples/vcpkg_example/vcpkg.json` - Example manifest
- `examples/vcpkg_example/create_test_archives.sh` - Test data generator
- `tests/test_unified_interface.cpp` - Unified interface tests
- `docs/VCPKG_INTEGRATION.md` - vcpkg documentation

### Documentation Files
- [`README.adoc`](../README.adoc:1) - Main readme (update)
- [`docs/VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md`](VCPKG_UNIFIED_INTERFACE_CONTINUATION_PLAN.md:1) - This plan
- `docs/VCPKG_INTEGRATION.md` - vcpkg guide (create)

---

## 🚀 Quick Start

### Step 1: Create vcpkg Port
```bash
mkdir -p vcpkg_ports/libtfs
# Create portfile.cmake, vcpkg.json, usage
# Update CMakeLists.txt with install rules
```

### Step 2: Create Example
```bash
mkdir -p examples/vcpkg_example
# Create main.cpp, CMakeLists.txt, vcpkg.json, etc.
chmod +x examples/vcpkg_example/create_test_archives.sh
cd examples/vcpkg_example && ./create_test_archives.sh
```

### Step 3: Build and Test
```bash
# Test the port
vcpkg install libtfs --overlay-ports=vcpkg_ports

# Build example
cd examples/vcpkg_example/build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build .
./archive_reader
```

### Step 4: Verify
```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs/build
./test_unified_interface
./test_backend_factory
```

---

## ✨ Success Criteria

When complete, you'll have:

1. ✅ vcpkg overlay port for libtfs that works with `vcpkg install`
2. ✅ Working example application demonstrating unified interface
3. ✅ Example builds with vcpkg toolchain
4. ✅ Unified interface tests passing
5. ✅ Complete documentation
6. ✅ Ready for production use

This represents a **complete, production-ready vcpkg integration** with comprehensive examples and documentation.

---

## 📝 Implementation Priority

1. **HIGH**: vcpkg port (required for example to work)
2. **HIGH**: Example application (demonstrates value)
3. **MEDIUM**: Unified interface tests (ensures quality)
4. **MEDIUM**: Documentation (enables users)

---

**Good luck!** 🚀

**Estimated Time**: 4-6 hours for complete implementation
**Start With**: Create the vcpkg overlay port structure