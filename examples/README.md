# libdwarfs-wr Examples

This directory contains comprehensive example programs demonstrating how to use the libdwarfs-wr API.

## Overview

The examples showcase the main features of libdwarfs-wr:

1. **basic_usage.cpp** - Basic DwarFS operations
   - Loading and mounting a DwarFS image
   - Reading files from the mounted filesystem
   - Proper cleanup and unmounting

2. **api_example.cpp** - Comprehensive API demonstration
   - File operations (open, read, lseek, pread, close)
   - Directory operations (opendir, readdir, closedir)
   - Stat operations (stat, fstat, lstat)
   - Path navigation (chdir, getcwd)
   - Access checking and file attributes
   - Advanced features (openat, fstatat)

## Building the Examples

The examples are **optional** and not built by default. To build them:

### Enable Examples During Configuration

```bash
cmake -DBUILD_EXAMPLES=ON [other options] ..
```

### Full Build Example

```bash
# Create build directory
mkdir build
cd build

# Configure with examples enabled
cmake -DBUILD_EXAMPLES=ON ..

# Build
cmake --build .

# The example executables will be in the examples subdirectory
./examples/basic_usage <dwarfs_image> <file_to_read>
./examples/api_example <dwarfs_image> [file_path] [dir_path]
```

## Running the Examples

### Prerequisites

You need a DwarFS filesystem image to run the examples. You can create one using `mkdwarfs`:

```bash
# Create a test directory with some files
mkdir test_fs
echo "Hello, DwarFS!" > test_fs/hello.txt
echo "Example file" > test_fs/example.txt
mkdir test_fs/subdir
echo "Nested file" > test_fs/subdir/nested.txt

# Create a DwarFS image
mkdwarfs -i test_fs -o test.dwarfs
```

### Basic Usage Example

The basic_usage example demonstrates the fundamental workflow:

```bash
./examples/basic_usage test.dwarfs /hello.txt
```

Expected output:
```
=== Basic DwarFS Usage Example ===

Step 1: Loading DwarFS image: test.dwarfs
  Image size: XXXXX bytes

Step 2: Mounting DwarFS image at root
  Successfully mounted DwarFS image

Step 3: Reading file: /hello.txt
  File descriptor: XX
  File size: 15 bytes
  File mode: 100644
  Bytes read: 15

File contents:
--- BEGIN ---
Hello, DwarFS!
--- END ---

Step 4: Closing file
  File closed successfully

Step 5: Unmounting DwarFS filesystem
  Successfully unmounted filesystem

=== Example completed successfully ===
```

### Comprehensive API Example

The api_example program demonstrates advanced features:

```bash
./examples/api_example test.dwarfs /hello.txt /subdir
```

This will show:
- File operations (open, read, lseek, pread)
- Directory listing
- File statistics and metadata
- Path navigation
- Advanced features like openat and fstatat

## API Usage Patterns

### Mounting a DwarFS Image

```cpp
// Load image into memory
std::ifstream file(image_path, std::ios::binary | std::ios::ate);
std::streamsize size = file.tellg();
file.seekg(0, std::ios::beg);
std::vector<char> buffer(size);
file.read(buffer.data(), size);

// Mount the filesystem
int ret = mount_root_memfs(
    buffer.data(),  // Image data
    size,           // Image size
    NULL,           // debuglevel (default)
    NULL,           // cachesize (default: 512MB)
    NULL,           // workers (default: 2)
    NULL,           // mlock (default: NONE)
    NULL,           // decompress_ratio (default: 0.8)
    "auto"          // image_offset (auto-detect)
);
```

### Reading a File

```cpp
// Open file
int fd = tebako_open(2, "/path/to/file.txt", O_RDONLY);

// Read content
char buffer[4096];
ssize_t bytes_read = tebako_read(fd, buffer, sizeof(buffer));

// Close file
tebako_close(fd);
```

### Listing a Directory

```cpp
// Open directory
DIR* dirp = tebako_opendir("/path/to/directory");

// Read entries
struct dirent* entry;
while ((entry = tebako_readdir(dirp)) != NULL) {
    std::cout << entry->d_name << std::endl;
}

// Close directory
tebako_closedir(dirp);
```

### Getting File Information

```cpp
// Get file statistics
struct stat st;
int ret = tebako_stat("/path/to/file.txt", &st);

if (ret == 0) {
    std::cout << "Size: " << st.st_size << " bytes\n";
    std::cout << "Type: " << (S_ISDIR(st.st_mode) ? "directory" : "file") << "\n";
}
```

### Cleanup

```cpp
// Always unmount when done
unmount_root_memfs();
```

## Error Handling

All libdwarfs-wr functions follow POSIX conventions:

- Return `-1` on error and set `errno`
- Return `0` or positive value on success
- NULL pointer return indicates error for pointer-returning functions

Example error handling:

```cpp
int fd = tebako_open(2, path, O_RDONLY);
if (fd < 0) {
    std::cerr << "Error: " << strerror(errno) << "\n";
    return 1;
}
```

## Important Notes

1. **Read-Only Filesystem**: DwarFS is read-only. Operations like O_WRONLY, O_RDWR, O_CREAT, O_TRUNC will fail with EROFS.

2. **Memory Requirements**: The entire DwarFS image is loaded into memory. Ensure sufficient RAM is available.

3. **Thread Safety**: The libdwarfs-wr API uses internal synchronization and is thread-safe.

4. **Path Format**: Use absolute paths (starting with `/`) or set the current working directory using `tebako_chdir()`.

5. **Cleanup**: Always call `unmount_root_memfs()` before program exit to properly release resources.

## Troubleshooting

### Build Errors

If you encounter build errors:

1. Ensure BUILD_EXAMPLES=ON is set during configuration
2. Check that all dependencies are available
3. Verify the main library builds successfully first

### Runtime Errors

Common issues:

- **ENOENT**: File or directory not found - check the path
- **EROFS**: Read-only filesystem - cannot write to DwarFS
- **EBADF**: Bad file descriptor - file may not be open or already closed

## API Reference

For complete API documentation, see:

- [`include/tebako-io.h`](../include/tebako-io.h) - Main I/O functions
- [`include/tebako-memfs.h`](../include/tebako-memfs.h) - Filesystem mounting
- [`include/tebako-fd.h`](../include/tebako-fd.h) - File descriptor management

## Contributing

When adding new examples:

1. Follow the existing code style
2. Include comprehensive comments
3. Add proper error handling
4. Update this README with the new example
5. Ensure the example builds on all supported platforms

## License

These examples are part of the libdwarfs-wr project and follow the same license terms.
See the license header in each source file for details.