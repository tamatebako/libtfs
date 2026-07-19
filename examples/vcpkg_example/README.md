# libtfs vcpkg Example

This example demonstrates how to use libtfs with vcpkg for unified archive handling.

NOTE: the `libtfs` vcpkg port is published in Stage 1; until then this example is illustrative.

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

# Create test archives first
chmod +x create_test_archives.sh
./create_test_archives.sh

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