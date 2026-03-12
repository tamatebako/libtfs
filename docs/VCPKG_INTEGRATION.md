# vcpkg Integration Guide

## Overview

libtfs is fully integrated with vcpkg, providing a modern, cross-platform package management solution for unified archive handling.

## Prerequisites

- CMake 3.24+
- C++20 compiler
- vcpkg

## Using libtfs in Your Project

### Method 1: vcpkg manifest mode (Recommended)

This is the modern, declarative way to manage dependencies with vcpkg.

#### Step 1: Create `vcpkg.json`

```json
{
  "name": "your-project",
  "version": "1.0.0",
  "dependencies": [
    "libtfs"
  ]
}
```

#### Step 2: Create `vcpkg-configuration.json`

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg/master/scripts/vcpkg-configuration.schema.json",
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "11bbc873e00e9e58d4e9dffb30b7a5493a030e0b"
  },
  "overlay-ports": [
    "/path/to/libdwarfs/vcpkg_ports"
  ]
}
```

#### Step 3: Configure CMake

In your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(your_project)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libtfs CONFIG REQUIRED)

add_executable(your_app main.cpp)
target_link_libraries(your_app PRIVATE libtfs::tfs)
```

#### Step 4: Build

```bash
export VCPKG_ROOT=/path/to/vcpkg

mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

### Method 2: Classic mode

Install libtfs globally:

```bash
vcpkg install libtfs --overlay-ports=/path/to/libdwarfs/vcpkg_ports
```

Then in your CMakeLists.txt:

```cmake
find_package(libtfs CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE libtfs::tfs)
```

Build with:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

## Supported Backends

libtfs provides a unified interface for multiple archive formats:

| Backend | Format | Extension | Status | Features |
|---------|--------|-----------|--------|----------|
| DwarFS | DwarFS | `.dwarfs` | ✅ Stable | High compression, fast decompression |
| ZIP | ZIP | `.zip` | ✅ Stable | Universal compatibility |
| SquashFS | SquashFS | `.squashfs` | 🚧 Coming soon | Linux standard format |

## Example Usage

### Basic Example

```cpp
#include <tebako/fs/backend_factory.h>
#include <iostream>

int main() {
  // Auto-detect format and create backend
  auto backend = tebako::fs::BackendFactory::create_from_file("archive.dwarfs");
  
  if (!backend) {
    std::cerr << "Failed to create backend\n";
    return 1;
  }
  
  std::cout << "Backend: " << backend->backend_name() << "\n";
  std::cout << "Version: " << backend->backend_version() << "\n";
  
  // Mount the archive
  if (!backend->mount("archive.dwarfs", "/mnt")) {
    std::cerr << "Failed to mount archive\n";
    return 1;
  }
  
  // Check if file exists
  if (backend->exists("/mnt/README.txt")) {
    std::cout << "README.txt found!\n";
  }
  
  // Unmount
  backend->unmount();
  
  return 0;
}
```

### Complete Example

See [`examples/vcpkg_example/`](../examples/vcpkg_example/) for a comprehensive example demonstrating:

- Backend creation via factory
- Auto-format detection
- Mounting archives
- Directory traversal
- File reading and seeking
- Metadata access

## API Reference

### Backend Creation

```cpp
#include <tebako/fs/backend_factory.h>

// Auto-detect format from file
auto backend = BackendFactory::create_from_file("archive.dwarfs");

// Create from memory buffer
auto backend = BackendFactory::create_from_memory(data, size, "/mnt");

// Specify format explicitly (ZIP example)
auto backend = BackendFactory::create_zip("archive.zip");
```

### Mounting and Unmounting

```cpp
// Mount archive
bool success = backend->mount(archive_path, "/mnt");

// Unmount archive
backend->unmount();

// Get mount point
std::string mp = backend->mount_point();

// Get archive path
std::string path = backend->archive_path();
```

### File Operations

```cpp
// Check existence
bool exists = backend->exists("/mnt/file.txt");

// Check if file or directory
bool is_file = backend->is_file("/mnt/file.txt");
bool is_dir = backend->is_directory("/mnt/dir");

// Open file
auto handle = backend->open("/mnt/file.txt", O_RDONLY);

// Read from file
char buffer[1024];
ssize_t bytes = handle->read(buffer, sizeof(buffer));

// Seek in file
off_t pos = handle->seek(100, SEEK_SET);  // Absolute
pos = handle->seek(10, SEEK_CUR);         // Relative to current
pos = handle->seek(-10, SEEK_END);        // Relative to end

// Get current position
off_t current = handle->tell();
```

### Directory Operations

```cpp
// List directory
auto iter = backend->list_directory("/mnt");

while (iter->has_next()) {
  auto entry = iter->next();
  
  std::cout << "Name: " << entry.name << "\n";
  std::cout << "Type: " << (entry.is_directory ? "DIR" : "FILE") << "\n";
  std::cout << "Size: " << entry.size << "\n";
}
```

### Metadata Operations

```cpp
// Get file size
size_t size = backend->file_size("/mnt/file.txt");

// Get modification time
time_t mtime = backend->modification_time("/mnt/file.txt");

// Get permissions
mode_t perms = backend->permissions("/mnt/file.txt");
```

### Backend Information

```cpp
// Get backend name
std::string name = backend->backend_name();  // "DwarFS", "ZIP", etc.

// Get backend version
std::string version = backend->backend_version();
```

## Troubleshooting

### Build Issues

#### Issue: Cannot find libtfs package

**Solution**: Ensure overlay-ports is correctly specified:

```bash
# In vcpkg-configuration.json
"overlay-ports": [
  "/absolute/path/to/libdwarfs/vcpkg_ports"
]

# Or use environment variable
export VCPKG_OVERLAY_PORTS=/path/to/libdwarfs/vcpkg_ports
```

#### Issue: Compilation errors with C++20

**Solution**: Ensure your compiler supports C++20:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

#### Issue: Linking errors

**Solution**: Ensure you're linking against the correct target:

```cmake
target_link_libraries(your_app PRIVATE libtfs::tfs)
```

### Runtime Issues

#### Issue: Archive mounting fails

**Possible causes**:
1. Archive format not supported
2. File permissions issue
3. Corrupted archive

**Solution**:
```cpp
auto backend = BackendFactory::create_from_file(path);
if (!backend) {
  std::cerr << "Unsupported format or file not found\n";
  return;
}

if (!backend->mount(path, "/mnt")) {
  std::cerr << "Mount failed - check permissions and file integrity\n";
  return;
}
```

#### Issue: File operations return errors

**Solution**: Always check return values:

```cpp
if (!backend->exists(path)) {
  std::cerr << "File does not exist: " << path << "\n";
  return;
}

auto handle = backend->open(path, O_RDONLY);
if (!handle) {
  std::cerr << "Failed to open file: " << path << "\n";
  return;
}
```

### vcpkg Issues

#### Issue: vcpkg cache is outdated

**Solution**:
```bash
# Clear vcpkg cache
vcpkg remove --outdated --recurse

# Update vcpkg
cd $VCPKG_ROOT
git pull
./bootstrap-vcpkg.sh  # or bootstrap-vcpkg.bat on Windows
```

#### Issue: Baseline mismatch

**Solution**: Update baseline in vcpkg-configuration.json:

```bash
# Get latest baseline
cd $VCPKG_ROOT
git rev-parse HEAD

# Update vcpkg-configuration.json with the output
```

## Advanced Usage

### Custom Mount Points

```cpp
auto backend = BackendFactory::create_from_file("archive.dwarfs");
backend->mount("archive.dwarfs", "/custom/mount/point");
```

### Multiple Archives

```cpp
std::vector<std::unique_ptr<Backend>> backends;

backends.push_back(BackendFactory::create_from_file("archive1.dwarfs"));
backends.push_back(BackendFactory::create_from_file("archive2.zip"));

for (auto& backend : backends) {
  std::string mount = "/mnt/" + backend->backend_name();
  backend->mount(backend->archive_path(), mount);
  // ... use backend ...
  backend->unmount();
}
```

### Error Handling

```cpp
try {
  auto backend = BackendFactory::create_from_file("archive.dwarfs");
  if (!backend) {
    throw std::runtime_error("Failed to create backend");
  }
  
  if (!backend->mount("archive.dwarfs", "/mnt")) {
    throw std::runtime_error("Failed to mount archive");
  }
  
  // ... operations ...
  
  backend->unmount();
} catch (const std::exception& e) {
  std::cerr << "Error: " << e.what() << "\n";
  return 1;
}
```

## Performance Considerations

### DwarFS Backend
- **Best for**: Maximum compression, read-heavy workloads
- **Compression**: Very high (often 2-3x better than gzip)
- **Decompression**: Fast (multi-threaded)
- **Memory**: Higher memory usage during decompression

### ZIP Backend
- **Best for**: Universal compatibility, moderate compression
- **Compression**: Standard (similar to gzip)
- **Decompression**: Good performance
- **Memory**: Lower memory footprint

## Links

- [Example Application](../examples/vcpkg_example/)
- [Main README](../README.adoc)
- [Backend Documentation](backends/)
- [DwarFS Backend Details](backends/ZIP_BACKEND.adoc)
- [ZIP Backend Details](backends/ZIP_BACKEND.adoc)

## Contributing

To report issues or contribute to libtfs:
- Repository: https://github.com/tamatebako/libdwarfs
- Issues: https://github.com/tamatebako/libdwarfs/issues

## License

libtfs is licensed under the BSD-2-Clause License. See LICENSE file for details.