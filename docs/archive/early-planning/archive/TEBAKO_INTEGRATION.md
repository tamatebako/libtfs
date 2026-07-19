# Tebako Integration Guide

## Overview

This guide explains how to integrate libtfs v0.11.0 into the Tebako project for Ruby application packaging.

**Prerequisites**:
- libtfs v0.11.0 built and installed
- Tebako repository cloned
- Ruby 3.0+ with FFI gem

## Build Integration

### Step 1: Install libtfs

```bash
cd /path/to/libtfs
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

### Step 2: Configure Tebako Build

Update Tebako's CMakeLists.txt:

```cmake
# Find libtfs
find_package(libtfs REQUIRED)

# Link against libtfs
target_link_libraries(tebako PRIVATE libtfs::tfs)
```

### Step 3: Verify Installation

```bash
# Check libtfs library
ls -la /usr/local/lib/libtfs.a

# Check headers
ls -la /usr/local/include/tebako/fs/c_api.h
```

## Ruby FFI Bindings

### Step 4: Create FFI Module

**File**: `lib/tebako/filesystem.rb`

```ruby
require 'ffi'

module Tebako
  module FileSystem
    extend FFI::Library

    # Load libtfs library
    ffi_lib 'tfs'

    # Lifecycle Management
    attach_function :tebako_fs_init_from_file, [:string, :string], :int
    attach_function :tebako_fs_init, [:pointer, :size_t, :string], :int
    attach_function :tebako_fs_unmount, [], :void
    attach_function :tebako_is_initialized, [], :int

    # File Operations
    attach_function :tebako_open, [:string, :int], :int
    attach_function :tebako_read, [:int, :pointer, :size_t], :ssize_t
    attach_function :tebako_lseek, [:int, :off_t, :int], :off_t
    attach_function :tebako_close, [:int], :int

    # Directory Operations
    attach_function :tebako_opendir, [:string], :pointer
    attach_function :tebako_readdir, [:pointer], :pointer
    attach_function :tebako_closedir, [:pointer], :int

    # Metadata Operations
    attach_function :tebako_stat, [:string, :pointer], :int
    attach_function :tebako_fstat, [:int, :pointer], :int

    # Path Detection
    attach_function :tebako_path_is_embedded, [:string], :int
    attach_function :tebako_fd_is_embedded, [:int], :int

    # Error Handling
    attach_function :tebako_get_errno, [], :int
    attach_function :tebako_strerror, [:int], :string

    # Utility Functions
    attach_function :tebako_get_mount_point, [], :string
    attach_function :tebako_get_archive_path, [], :string
    attach_function :tebako_get_backend_name, [], :string
  end
end
```

### Step 5: Initialize Filesystem

```ruby
# Mount embedded archive
result = Tebako::FileSystem.tebako_fs_init(
  embedded_data_ptr,
  embedded_data_size,
  "/__tebako__"
)

if result == 0
  puts "Filesystem mounted successfully"
else
  errno = Tebako::FileSystem.tebako_get_errno
  error = Tebako::FileSystem.tebako_strerror(errno)
  raise "Mount failed: #{error}"
end
```

## Testing Integration

### Unit Tests

Create `spec/tebako/filesystem_spec.rb`:

```ruby
RSpec.describe Tebako::FileSystem do
  describe '.tebako_fs_init' do
    it 'mounts archive successfully' do
      # Test implementation
    end
  end

  describe '.tebako_open' do
    it 'opens files from mounted archive' do
      # Test implementation
    end
  end
end
```

### Integration Tests

Test with actual Ruby script packaging:

```ruby
# test/integration/package_test.rb
require 'test_helper'

class PackageTest < Minitest::Test
  def test_package_and_run
    # Package a simple Ruby script
    # Run packaged executable
    # Verify it can read files from embedded archive
  end
end
```

## Troubleshooting

### Common Issues

**Issue**: `undefined symbol: tebako_fs_init`
**Solution**: Ensure libtfs.a is linked before other libraries

**Issue**: Segmentation fault on mount
**Solution**: Verify memory buffer remains valid until unmount

**Issue**: `ENOENT` errors when reading files
**Solution**: Check mount point matches file paths

### Debug Techniques

Enable verbose logging:
```ruby
ENV['TEBAKO_DEBUG'] = '1'
```

Check mounted files:
```ruby
mount_point = Tebako::FileSystem.tebako_get_mount_point
puts "Mounted at: #{mount_point}"
```

### Platform-Specific Notes

**macOS**:
- Uses Homebrew dependencies
- Code signing may require entitlements

**Linux**:
- System libraries vs vcpkg dependencies
- Check LD_LIBRARY_PATH if dynamic linking

**Windows**:
- MSVC runtime library compatibility
- Path separator handling (backslash vs forward slash)

## Performance Considerations

- Mount time: < 10ms typical
- Read throughput: > 100 MB/s
- Memory overhead: < 10 MB per archive
- Thread-safe: All operations concurrent-safe

## See Also

- [C API Reference](../include/tebako/fs/c_api.h)
- [Testing Guide](TESTING.adoc)
- [Performance Baseline](PERFORMANCE_BASELINE.md)
- [Ruby Integration Example](../examples/ruby_integration_example.rb)