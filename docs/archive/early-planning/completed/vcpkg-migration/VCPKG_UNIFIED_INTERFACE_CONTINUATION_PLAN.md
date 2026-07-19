# vcpkg Unified Interface - Continuation Plan

**Date**: 2025-12-27
**Status**: Phase 1 Complete - Enhanced Unified Interface Needed
**Priority**: P1 - Final Phase
**Estimated Time**: 4-6 hours

---

## 🎯 Mission

Complete the vcpkg migration by:
1. Ensuring the unified interface works seamlessly with all backends (DwarFS, ZIP, SquashFS)
2. Creating a working example application with vcpkg integration
3. Creating a libtfs overlay port for vcpkg
4. Updating all official documentation

---

## 📊 Current State

### ✅ Completed (Phase 1)
- vcpkg integration for DwarFS backend (100%)
- All DwarFS backend tests passing (45/45)
- Debug logging cleaned up
- Obsolete CMake variables removed
- Code compiles cleanly with vcpkg

### ⚠️ Needs Work (Phase 2)
- Unified interface verification across all backends
- Example application demonstrating libtfs usage
- vcpkg overlay port for libtfs
- Official documentation updates

---

## 📋 Tasks

### Task 1: Verify Unified Interface Across Backends (2 hours)

#### 1.1: Review Backend Factory API

**File**: [`include/tebako/fs/backend_factory.h`](../include/tebako/fs/backend_factory.h:1)

**Goal**: Ensure [`BackendFactory`](../include/tebako/fs/backend_factory.h:1) provides a consistent interface for all backends.

**Checklist**:
- [ ] Verify [`create_from_file()`](../src/backend_factory.cpp:1) works for all formats
- [ ] Verify [`create_from_memory()`](../src/backend_factory.cpp:1) works for all formats
- [ ] Ensure auto-detection works reliably
- [ ] Test backend switching at runtime

#### 1.2: Create Unified Interface Tests

**File**: `tests/test_unified_interface.cpp` (new)

**Goal**: Test that all backends work through the same interface.

**Test Cases**:
```cpp
// Test backend creation from file
TEST(UnifiedInterfaceTest, CreateBackendFromFile) {
  // Test DwarFS
  auto dwarfs = BackendFactory::create_from_file("test.dwarfs");
  ASSERT_NE(dwarfs, nullptr);
  EXPECT_EQ(dwarfs->backend_name(), "DwarFS");

  // Test ZIP
  auto zip = BackendFactory::create_from_file("test.zip");
  ASSERT_NE(zip, nullptr);
  EXPECT_EQ(zip->backend_name(), "ZIP");

  // Test SquashFS (when available)
  auto squashfs = BackendFactory::create_from_file("test.squashfs");
  if (squashfs) {
    EXPECT_EQ(squashfs->backend_name(), "SquashFS");
  }
}

// Test identical API across backends
TEST(UnifiedInterfaceTest, IdenticalAPIBehavior) {
  std::vector<std::unique_ptr<Backend>> backends;
  backends.push_back(BackendFactory::create_from_file("test.dwarfs"));
  backends.push_back(BackendFactory::create_from_file("test.zip"));

  for (auto& backend : backends) {
    ASSERT_TRUE(backend->mount(backend->archive_path(), "/mnt"));
    EXPECT_TRUE(backend->exists("/mnt/file.txt"));
    EXPECT_TRUE(backend->is_file("/mnt/file.txt"));

    auto handle = backend->open("/mnt/file.txt", O_RDONLY);
    ASSERT_NE(handle, nullptr);
    // ... etc
  }
}
```

#### 1.3: Ensure Polymorphic Behavior

**Files**:
- [`include/tebako/fs/backends/zip_backend.h`](../include/tebako/fs/backends/zip_backend.h:1)
- [`include/tebako/fs/backends/dwarfs_backend.h`](../include/tebako/fs/backends/dwarfs_backend.h:1)
- [`include/tebako/fs/backends/squashfs_backend.h`](../include/tebako/fs/backends/squashfs_backend.h:1)

**Checklist**:
- [ ] All backends inherit from [`Backend`](../include/tebako/fs/backend_factory.h:1) base class
- [ ] All virtual methods properly overridden
- [ ] No backend-specific code leaks into client code
- [ ] Runtime polymorphism works correctly

---

### Task 2: Create vcpkg Overlay Port for libtfs (1.5 hours)

#### 2.1: Create Port Directory Structure

**Directory**: `vcpkg_ports/libtfs/`

**Structure**:
```
vcpkg_ports/libtfs/
├── portfile.cmake
├── vcpkg.json
└── usage
```

#### 2.2: Create portfile.cmake

**File**: `vcpkg_ports/libtfs/portfile.cmake`

**Content**:
```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/libdwarfs
    REF v${VERSION}
    SHA512 0  # Will be updated
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWITH_TESTS=OFF
        -DPREFER_SYSTEM_GTEST=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME libtfs CONFIG_PATH lib/cmake/libtfs)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
```

#### 2.3: Create vcpkg.json

**File**: `vcpkg_ports/libtfs/vcpkg.json`

**Content**:
```json
{
  "name": "libtfs",
  "version": "0.11.0",
  "description": "Tamatebako Filesystem Library - Unified interface for ZIP, DwarFS, and SquashFS archives",
  "homepage": "https://github.com/tamatebako/libdwarfs",
  "license": "BSD-2-Clause",
  "dependencies": [
    {
      "name": "vcpkg-cmake",
      "host": true
    },
    {
      "name": "vcpkg-cmake-config",
      "host": true
    },
    "dwarfs",
    "libzip",
    {
      "name": "argtable3",
      "default-features": false
    }
  ],
  "features": {
    "tests": {
      "description": "Build tests",
      "dependencies": [
        "gtest"
      ]
    }
  }
}
```

#### 2.4: Create usage file

**File**: `vcpkg_ports/libtfs/usage`

**Content**:
```
libtfs provides CMake targets:

    find_package(libtfs CONFIG REQUIRED)
    target_link_libraries(main PRIVATE libtfs::tfs)

libtfs provides a unified interface for multiple archive formats:
- DwarFS (.dwarfs)
- ZIP (.zip)
- SquashFS (.squashfs)

Example usage:
    #include <tebako/fs/backend_factory.h>

    auto backend = tebako::fs::BackendFactory::create_from_file("archive.dwarfs");
    backend->mount(backend->archive_path(), "/mnt");
```

#### 2.5: Create CMake config file

**File**: `cmake/libtfsConfig.cmake.in`

**Content**:
```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

find_dependency(dwarfs)
find_dependency(libzip)

include("${CMAKE_CURRENT_LIST_DIR}/libtfsTargets.cmake")

check_required_components(libtfs)
```

#### 2.6: Update CMakeLists.txt for installation

**File**: [`CMakeLists.txt`](../CMakeLists.txt:1)

**Add**:
```cmake
# Install CMake config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libtfsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/libtfsConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libtfs"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/libtfsConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/libtfsConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/libtfsConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libtfs"
)

install(EXPORT libtfsTargets
  FILE libtfsTargets.cmake
  NAMESPACE libtfs::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libtfs"
)
```

---

### Task 3: Create Example Application (2 hours)

#### 3.1: Create Example Directory

**Directory**: `examples/vcpkg_example/`

**Structure**:
```
examples/vcpkg_example/
├── CMakeLists.txt
├── vcpkg.json
├── vcpkg-configuration.json
├── main.cpp
├── README.md
└── test_archives/
    ├── sample.dwarfs
    ├── sample.zip
    └── sample.squashfs (optional)
```

#### 3.2: Create vcpkg.json

**File**: `examples/vcpkg_example/vcpkg.json`

**Content**:
```json
{
  "name": "libtfs-example",
  "version": "0.1.0",
  "dependencies": [
    "libtfs"
  ]
}
```

#### 3.3: Create vcpkg-configuration.json

**File**: `examples/vcpkg_example/vcpkg-configuration.json`

**Content**:
```json
{
  "default-registry": {
    "kind": "git",
    "baseline": "...",
    "repository": "https://github.com/microsoft/vcpkg"
  },
  "registries": [
    {
      "kind": "filesystem",
      "path": "../../vcpkg_ports",
      "packages": [ "libtfs" ]
    }
  ],
  "overlay-ports": [
    "../../vcpkg_ports"
  ]
}
```

#### 3.4: Create CMakeLists.txt

**File**: `examples/vcpkg_example/CMakeLists.txt`

**Content**:
```cmake
cmake_minimum_required(VERSION 3.24)
project(libtfs_example VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libtfs package
find_package(libtfs CONFIG REQUIRED)

# Create example executable
add_executable(archive_reader main.cpp)
target_link_libraries(archive_reader PRIVATE libtfs::tfs)

# Copy test archives to build directory
file(COPY test_archives DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Installation
install(TARGETS archive_reader
  RUNTIME DESTINATION bin
)
```

#### 3.5: Create main.cpp

**File**: `examples/vcpkg_example/main.cpp`

**Content**:
```cpp
/**
 * @file main.cpp
 * @brief Example demonstrating libtfs unified interface for multiple archive formats
 */

#include <tebako/fs/backend_factory.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

using namespace tebako::fs;

/**
 * @brief Display file information
 */
void print_file_info(Backend& backend, const std::string& path) {
  if (backend.is_file(path)) {
    auto size = backend.file_size(path);
    auto mtime = backend.modification_time(path);
    auto perms = backend.permissions(path);

    std::cout << "  File: " << path << "\n"
              << "    Size: " << size << " bytes\n"
              << "    Modified: " << mtime << "\n"
              << "    Permissions: " << std::oct << perms << std::dec << "\n";
  }
}

/**
 * @brief List directory contents recursively
 */
void list_directory(Backend& backend, const std::string& path, int depth = 0) {
  std::string indent(depth * 2, ' ');

  auto iter = backend.list_directory(path);
  if (!iter) {
    std::cerr << indent << "Failed to list directory: " << path << "\n";
    return;
  }

  while (iter->has_next()) {
    auto entry = iter->next();
    std::string full_path = path + "/" + entry.name;

    if (entry.is_directory) {
      std::cout << indent << "[DIR]  " << entry.name << "\n";
      list_directory(backend, full_path, depth + 1);
    } else {
      std::cout << indent << "[FILE] " << entry.name
                << " (" << entry.size << " bytes)\n";
    }
  }
}

/**
 * @brief Read and display file contents
 */
void read_file(Backend& backend, const std::string& path) {
  auto handle = backend.open(path, O_RDONLY);
  if (!handle) {
    std::cerr << "Failed to open file: " << path << "\n";
    return;
  }

  std::cout << "\n--- Contents of " << path << " ---\n";

  char buffer[4096];
  ssize_t bytes_read;

  while ((bytes_read = handle->read(buffer, sizeof(buffer))) > 0) {
    std::cout.write(buffer, bytes_read);
  }

  std::cout << "\n--- End of file ---\n\n";
}

/**
 * @brief Process a single archive file
 */
void process_archive(const std::string& archive_path) {
  std::cout << "\n" << std::string(70, '=') << "\n";
  std::cout << "Processing: " << archive_path << "\n";
  std::cout << std::string(70, '=') << "\n";

  // Create backend using factory
  auto backend = BackendFactory::create_from_file(archive_path);
  if (!backend) {
    std::cerr << "ERROR: Failed to create backend for " << archive_path << "\n";
    return;
  }

  std::cout << "Backend: " << backend->backend_name() << "\n";
  std::cout << "Version: " << backend->backend_version() << "\n";

  // Mount the archive
  std::string mount_point = "/mnt/" + backend->backend_name();
  if (!backend->mount(archive_path, mount_point)) {
    std::cerr << "ERROR: Failed to mount " << archive_path << "\n";
    return;
  }

  std::cout << "Mounted at: " << mount_point << "\n\n";

  // List contents
  std::cout << "Directory structure:\n";
  list_directory(*backend, mount_point);

  // Read a sample file if it exists
  std::string sample_file = mount_point + "/README.txt";
  if (backend->exists(sample_file)) {
    read_file(*backend, sample_file);
  }

  // Demonstrate file operations
  std::cout << "\nFile operations demo:\n";
  auto iter = backend->list_directory(mount_point);
  if (iter && iter->has_next()) {
    auto entry = iter->next();
    if (!entry.is_directory) {
      std::string file_path = mount_point + "/" + entry.name;
      print_file_info(*backend, file_path);

      // Demonstrate seeking
      auto handle = backend->open(file_path, O_RDONLY);
      if (handle) {
        std::cout << "\nSeek operations:\n";
        std::cout << "  Initial position: " << handle->tell() << "\n";

        handle->seek(10, SEEK_SET);
        std::cout << "  After SEEK_SET(10): " << handle->tell() << "\n";

        handle->seek(5, SEEK_CUR);
        std::cout << "  After SEEK_CUR(5): " << handle->tell() << "\n";

        handle->seek(-10, SEEK_END);
        std::cout << "  After SEEK_END(-10): " << handle->tell() << "\n";
      }
    }
  }

  // Unmount
  backend->unmount();
  std::cout << "\nUnmounted successfully.\n";
}

int main(int argc, char* argv[]) {
  std::cout << "libtfs Example - Unified Archive Interface\n";
  std::cout << "==========================================\n";

  // Process command-line archives or use defaults
  std::vector<std::string> archives;

  if (argc > 1) {
    for (int i = 1; i < argc; ++i) {
      archives.push_back(argv[i]);
    }
  } else {
    // Use built-in test archives
    archives = {
      "test_archives/sample.dwarfs",
      "test_archives/sample.zip"
    };
  }

  // Process each archive
  for (const auto& archive : archives) {
    try {
      process_archive(archive);
    } catch (const std::exception& e) {
      std::cerr << "ERROR: Exception processing " << archive << ": " << e.what() << "\n";
    }
  }

  std::cout << "\n" << std::string(70, '=') << "\n";
  std::cout << "Example completed successfully!\n";

  return 0;
}
```

#### 3.6: Create README.md

**File**: `examples/vcpkg_example/README.md`

**Content**:
```markdown
# libtfs vcpkg Example

This example demonstrates how to use libtfs with vcpkg for unified archive handling.

## Features

- Unified interface for DwarFS, ZIP, and SquashFS archives
- Automatic format detection
- File reading and seeking
- Directory traversal
- Metadata access

## Building

### Prerequisites

- CMake 3.24 or later
- vcpkg
- C++20 compiler

### Build Steps

```bash
# Configure vcpkg
export VCPKG_ROOT=/path/to/vcpkg

# Create build directory
mkdir build && cd build

# Configure with vcpkg toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build .

# Run
./archive_reader
```

## Usage

Run with default test archives:
```bash
./archive_reader
```

Run with custom archives:
```bash
./archive_reader /path/to/archive1.dwarfs /path/to/archive2.zip
```

## Expected Output

The program will:
1. Detect archive format automatically
2. Mount the archive
3. List directory structure
4. Read sample files
5. Demonstrate file operations (seek, read, metadata)
6. Unmount the archive

## API Demonstration

This example showcases the key libtfs APIs:

- `BackendFactory::create_from_file()` - Auto-detect and create backend
- `Backend::mount()` - Mount archive
- `Backend::list_directory()` - Traverse directories
- `Backend::open()` - Open files
- `FileHandle::read()` / `seek()` - File I/O
- `Backend::file_size()`, `permissions()`, etc. - Metadata access
```

#### 3.7: Create Test Archives Script

**File**: `examples/vcpkg_example/create_test_archives.sh`

**Content**:
```bash
#!/bin/bash
set -e

# Create test archives directory
mkdir -p test_archives
cd test_archives

# Create sample content
mkdir -p sample_content
echo "Hello from libtfs example!" > sample_content/README.txt
echo "This is a test file." > sample_content/test.txt
mkdir -p sample_content/subdir
echo "Nested file content" > sample_content/subdir/nested.txt

# Create DwarFS archive
if command -v mkdwarfs &> /dev/null; then
    mkdwarfs -i sample_content -o sample.dwarfs --no-progress
    echo "Created sample.dwarfs"
else
    echo "mkdwarfs not found, skipping DwarFS archive creation"
fi

# Create ZIP archive
zip -r sample.zip sample_content/
echo "Created sample.zip"

# Create SquashFS archive (if available)
if command -v mksquashfs &> /dev/null; then
    mksquashfs sample_content sample.squashfs -noappend
    echo "Created sample.squashfs"
else
    echo "mksquashfs not found, skipping SquashFS archive creation"
fi

# Cleanup
rm -rf sample_content

echo ""
echo "Test archives created successfully!"
ls -lh
```

---

### Task 4: Update Official Documentation (0.5 hours)

#### 4.1: Update README.adoc

**File**: [`README.adoc`](../README.adoc:1)

**Add sections**:

1. vcpkg Integration section
2. Example usage with vcpkg
3. Links to example directory

#### 4.2: Create vcpkg Documentation

**File**: `docs/VCPKG_INTEGRATION.md`

**Content**: Comprehensive guide on using libtfs with vcpkg

#### 4.3: Move Outdated Documentation

**Move to**: `old-docs/completed/`

**Files to move**:
- `docs/VCPKG_MIGRATION_STATUS.md` → `old-docs/completed/`
- `docs/VCPKG_MIGRATION_CONTINUATION_PLAN.md` → `old-docs/completed/`
- Any other migration-specific temporary docs

---

## 🚀 Quick Start for Next Session

### Step 1: Verify Current State
```bash
cd /Users/mulgogi/src/tamatebako/libdwarfs
git status
```

### Step 2: Run Unified Interface Tests
```bash
cd build
./test_backend_factory
./test_zip_backend
./test_dwarfs_backend
```

### Step 3: Create vcpkg Port
```bash
mkdir -p vcpkg_ports/libtfs
# Follow Task 2 instructions
```

### Step 4: Create Example
```bash
mkdir -p examples/vcpkg_example
# Follow Task 3 instructions
```

---

## ✨ Success Criteria

When complete:

1. ✅ All three backends (DwarFS, ZIP, SquashFS) work through unified interface
2. ✅ vcpkg overlay port for libtfs created and working
3. ✅ Example application compiles and runs with vcpkg
4. ✅ Example demonstrates all key features
5. ✅ Official documentation updated
6. ✅ Old documentation archived

---

## 📝 Notes

- The unified interface is already largely complete via Backend base class
- Main work is creating the vcpkg port and example
- Example should be simple but comprehensive
- Documentation is critical for users to understand vcpkg workflow

**Estimated Total Time**: 4-6 hours for complete implementation