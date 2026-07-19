# Tebako Integration Architecture

**Date**: 2025-12-22  
**Status**: Architecture Design Document  
**Purpose**: Define clean integration of libtfs within Tebako ecosystem with real-world benchmarking

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Architecture Overview](#architecture-overview)
3. [Component Responsibilities](#component-responsibilities)
4. [Embedded Image Architecture](#embedded-image-architecture)
5. [Ruby C API Integration](#ruby-c-api-integration)
6. [Benchmarking Infrastructure](#benchmarking-infrastructure)
7. [Implementation Roadmap](#implementation-roadmap)

---

## Executive Summary

### Purpose

This document defines the architectural integration of **libtfs** (Tebako File System library) within the **Tebako** application packaging ecosystem, establishing clean separation of concerns and defining interfaces between components.

### Key Principles

1. **Clean Separation**: Execution shim, libtfs library, and Ruby runtime are independent layers
2. **Format Agnostic**: libtfs abstracts filesystem format (ZIP, SquashFS, DwarFS)
3. **Minimal Integration**: Ruby modifications isolated to file I/O hooks
4. **Real Benchmarking**: Use perl-5.42.0.tar.gz (31MB) for realistic performance data
5. **Thin Shim**: Execution wrapper is orchestration only, no business logic

### Architecture at a Glance

```
┌──────────────────────────────────────────────────────────┐
│           Tebako Packaged Executable                     │
│  ┌────────────────────────────────────────────────────┐  │
│  │  Execution Shim (50-200 lines)                     │  │
│  │  • Parse --tebako-extract flag                     │  │
│  │  • Locate embedded image                           │  │
│  │  • Initialize libtfs OR extract                    │  │
│  │  • Hand off to Ruby                                │  │
│  └─────────────┬──────────────────────────────────────┘  │
│                │                                          │
│  ┌─────────────▼──────────────────────────────────────┐  │
│  │  libtfs Library (Format-agnostic VFS)             │  │
│  │  ┌──────────────┬──────────────┬────────────────┐ │  │
│  │  │ ZIP Backend  │ SquashFS BE  │ DwarFS Backend │ │  │
│  │  └──────────────┴──────────────┴────────────────┘ │  │
│  └─────────────┬──────────────────────────────────────┘  │
│                │                                          │
│  ┌─────────────▼──────────────────────────────────────┐  │
│  │  Ruby Runtime (Modified)                           │  │
│  │  • File I/O hooks → libtfs                         │  │
│  │  • open/read/stat/readdir patched                  │  │
│  │  • Minimal changes (50-100 lines)                  │  │
│  └────────────────────────────────────────────────────┘  │
│                                                           │
│  ┌────────────────────────────────────────────────────┐  │
│  │  Embedded Filesystem Image                         │  │
│  │  [ZIP|SquashFS|DwarFS archive]                     │  │
│  │  • Appended to executable                          │  │
│  │  • Located via marker + offset                     │  │
│  └────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

---

## Architecture Overview

### 1. Three-Layer Design

#### Layer 1: Execution Shim (Orchestration)

**Responsibility**: Minimal startup logic only

```c
// Pseudo-code for execution shim
int main(int argc, char* argv[]) {
    // 1. Parse command line
    if (has_flag(argc, argv, "--tebako-extract")) {
        return extract_mode(argv);
    }
    
    // 2. Locate embedded image
    EmbeddedImage image = locate_embedded_image();
    
    // 3. Initialize libtfs
    tebako_fs_init(image.data, image.size, image.format);
    
    // 4. Hand off to Ruby
    return ruby_main(argc, argv);
}
```

**Key Characteristics**:
- **Size**: 50-200 lines of C code
- **No business logic**: Pure orchestration
- **No format knowledge**: libtfs handles all formats
- **Stateless**: All state in libtfs or Ruby

#### Layer 2: libtfs Library (VFS Abstraction)

**Responsibility**: Format-agnostic filesystem operations

```cpp
// libtfs public API
namespace tebako {
namespace fs {

// Initialize from memory-embedded image
bool init(const void* data, size_t size, const char* format_hint);

// POSIX-like C API for Ruby integration
extern "C" {
    int tebako_open(const char* path, int flags);
    ssize_t tebako_read(int fd, void* buf, size_t count);
    int tebako_stat(const char* path, struct stat* st);
    DIR* tebako_opendir(const char* path);
    // ... etc
}

} // namespace fs
} // namespace tebako
```

**Key Characteristics**:
- **Backend agnostic**: ZIP, SquashFS, DwarFS via unified interface
- **C API**: For Ruby C extension integration
- **Thread-safe**: Supports Ruby's threading model
- **No Ruby knowledge**: Pure filesystem abstraction

#### Layer 3: Ruby Runtime (Modified)

**Responsibility**: Route file I/O through libtfs for embedded paths

```c
// In Ruby's file.c (simplified)
static int rb_file_open_internal(const char* path, int flags) {
    // Check if path is in embedded filesystem
    if (tebako_path_is_embedded(path)) {
        return tebako_open(path, flags);
    }
    // Otherwise use host filesystem
    return open(path, flags);
}
```

**Key Characteristics**:
- **Minimal changes**: 50-100 lines across 5-10 files
- **Isolated hooks**: Only in file I/O functions
- **Fallback logic**: Host FS for non-embedded paths
- **Transparent**: Ruby code sees normal filesystem

---

## Component Responsibilities

### Execution Shim Responsibilities

**MUST DO**:
- ✅ Parse `--tebako-extract` flag
- ✅ Locate embedded image (read marker + offset)
- ✅ Initialize libtfs with image data
- ✅ Handle extract mode (copy image to disk)
- ✅ Invoke Ruby main entry point

**MUST NOT DO**:
- ❌ Understand archive formats
- ❌ Perform filesystem operations
- ❌ Contain business logic
- ❌ Manage file descriptors
- ❌ Handle Ruby integration

**Interface**:
```c
// Shim calls libtfs
bool tebako_fs_init(const void* data, size_t size, const char* mount_point);

// Shim calls extract
int tebako_extract_image(const void* data, size_t size, const char* dest_path);

// Shim calls Ruby
int ruby_main(int argc, char* argv[]);
```

### libtfs Library Responsibilities

**MUST DO**:
- ✅ Provide format-agnostic VFS API
- ✅ Auto-detect archive format (magic bytes)
- ✅ Mount images from memory
- ✅ Implement POSIX-like operations
- ✅ Thread-safe concurrent access
- ✅ Extract functionality (for --tebako-extract)

**MUST NOT DO**:
- ❌ Know about Ruby internals
- ❌ Handle command-line parsing
- ❌ Manage application lifecycle
- ❌ Contain shim logic
- ❌ Determine mount points (passed in)

**Interface**:
```cpp
namespace tebako {
namespace fs {

// Initialization
bool init_from_memory(const void* data, size_t size, 
                     const char* mount_point);

// POSIX-like C API
extern "C" {
    int tebako_open(const char* path, int flags);
    ssize_t tebako_read(int fd, void* buf, size_t count);
    off_t tebako_lseek(int fd, off_t offset, int whence);
    int tebako_close(int fd);
    int tebako_stat(const char* path, struct stat* st);
    DIR* tebako_opendir(const char* path);
    struct dirent* tebako_readdir(DIR* dir);
    int tebako_closedir(DIR* dir);
}

// Extraction
int extract_all(const char* dest_path);

} // namespace fs
} // namespace tebako
```

### Ruby Runtime Responsibilities

**MUST DO**:
- ✅ Hook file I/O at C level
- ✅ Route embedded paths to libtfs
- ✅ Route normal paths to host OS
- ✅ Maintain transparent behavior for Ruby code

**MUST NOT DO**:
- ❌ Understand archive formats
- ❌ Manage embedded image
- ❌ Contain extraction logic
- ❌ Parse command-line flags (belongs to shim)

**Integration Points** (in Ruby C source):

| File | Function | Change |
|------|----------|--------|
| `file.c` | `rb_file_open_internal` | Check if embedded, route to libtfs |
| `io.c` | `rb_io_read` | Use tebako FD if embedded |
| `dir.c` | `dir_s_open` | Route to tebako_opendir if embedded |
| `file.c` | `rb_file_s_stat` | Route to tebako_stat if embedded |
| `file.c` | `rb_file_s_lstat` | Route to tebako_stat if embedded |

**Path Detection**:
```c
// Simple detection - paths starting with mount point
static inline bool tebako_path_is_embedded(const char* path) {
    return strncmp(path, TEBAKO_MOUNT_POINT, 
                   strlen(TEBAKO_MOUNT_POINT)) == 0;
}
```

---

## Embedded Image Architecture

### 1. Image Location

The embedded image is **appended** to the executable:

```
┌──────────────────────────────────────┐
│  Executable Code + Data              │  ← Regular ELF/PE/Mach-O
├──────────────────────────────────────┤
│  Marker: "TEBAKO_FS_IMAGE_V1"        │  ← 20-byte marker
├──────────────────────────────────────┤
│  Metadata (64 bytes):                │
│    - Format: "ZIP" | "SQFS" | "DWARFS"│
│    - Image size (uint64_t)           │
│    - Image offset (uint64_t)         │
│    - Checksum (uint32_t)             │
│    - Version (uint16_t)              │
│    - Reserved (26 bytes)             │
├──────────────────────────────────────┤
│  Filesystem Image Data               │  ← Actual archive
│  [ZIP|SquashFS|DwarFS archive]       │
└──────────────────────────────────────┘
```

### 2. Image Discovery Algorithm

```c
typedef struct {
    char format[8];        // "ZIP", "SQFS", "DWARFS"
    uint64_t image_size;   // Size of image in bytes
    uint64_t image_offset; // Offset from start of file
    uint32_t checksum;     // CRC32 of image data
    uint16_t version;      // Metadata version
    uint8_t reserved[26];  // Future use
} tebako_image_metadata;

// In execution shim
EmbeddedImage locate_embedded_image(void) {
    // 1. Get executable path
    const char* exe_path = get_executable_path();
    
    // 2. Open executable
    int fd = open(exe_path, O_RDONLY);
    
    // 3. Seek to end and read backwards for marker
    off_t file_size = lseek(fd, 0, SEEK_END);
    off_t search_start = file_size - sizeof(tebako_image_metadata) - 20;
    lseek(fd, search_start, SEEK_SET);
    
    // 4. Read marker
    char marker[21];
    read(fd, marker, 20);
    marker[20] = '\0';
    
    if (strcmp(marker, "TEBAKO_FS_IMAGE_V1") != 0) {
        die("No embedded image found");
    }
    
    // 5. Read metadata
    tebako_image_metadata meta;
    read(fd, &meta, sizeof(meta));
    
    // 6. Map image into memory
    void* image_data = mmap(NULL, meta.image_size, PROT_READ, 
                           MAP_PRIVATE, fd, meta.image_offset);
    
    return (EmbeddedImage){
        .data = image_data,
        .size = meta.image_size,
        .format = meta.format
    };
}
```

### 3. Image Formats Supported

| Format | Magic Bytes | Extensions | Pros | Cons |
|--------|-------------|------------|------|------|
| **ZIP** | `PK\x03\x04` | `.zip` | Universal, simple | Slow seek, default perms |
| **SquashFS** | `hsqs`/`sqsh` | `.sqfs` | Fast seek, POSIX perms | Less universal |
| **DwarFS** | `DWARFS` | `.dwarfs` | Best compression | Build complexity |

**Format Selection** (at packaging time):
```bash
# Package with ZIP (default, maximum compatibility)
tebako package --format=zip app.rb

# Package with SquashFS (better performance)
tebako package --format=squashfs app.rb

# Package with DwarFS (best compression)
tebako package --format=dwarfs app.rb
```

### 4. --tebako-extract Implementation

```c
// In execution shim
int extract_mode(char* argv[]) {
    // 1. Locate embedded image
    EmbeddedImage image = locate_embedded_image();
    
    // 2. Determine destination
    const char* dest = argv[2] ? argv[2] : "./tebako-extracted";
    
    // 3. Initialize libtfs (read-only)
    if (!tebako_fs_init(image.data, image.size, "/tebako")) {
        die("Failed to initialize filesystem");
    }
    
    // 4. Extract all files
    printf("Extracting to %s...\n", dest);
    int result = tebako_fs_extract_all(dest);
    
    // 5. Cleanup
    tebako_fs_unmount();
    
    return result;
}
```

**User Experience**:
```bash
# Extract embedded filesystem
./my-app --tebako-extract
# → Creates ./tebako-extracted/ with all files

# Extract to specific location
./my-app --tebako-extract /tmp/app-files
# → Creates /tmp/app-files/ with all files

# Normal execution
./my-app
# → Runs application with embedded filesystem mounted
```

---

## Ruby C API Integration

### 1. Integration Points

**File**: `ruby/file.c`

```c
// Original Ruby function
static int rb_file_open_internal(const char* path, int flags) {
    return open(path, flags);
}

// Modified (add before existing code)
static int rb_file_open_internal(const char* path, int flags) {
    // NEW: Check if path is in embedded filesystem
    if (tebako_is_initialized() && tebako_path_is_embedded(path)) {
        return tebako_open(path, flags);
    }
    
    // EXISTING: Fall through to host filesystem
    return open(path, flags);
}
```

**File**: `ruby/io.c`

```c
static ssize_t rb_io_read_internal(int fd, void* buf, size_t count) {
    // NEW: Check if FD is from libtfs
    if (tebako_fd_is_embedded(fd)) {
        return tebako_read(fd, buf, count);
    }
    
    // EXISTING: Host OS read
    return read(fd, buf, count);
}
```

**File**: `ruby/dir.c`

```c
static DIR* rb_dir_open_internal(const char* path) {
    // NEW: Check if embedded
    if (tebako_is_initialized() && tebako_path_is_embedded(path)) {
        return tebako_opendir(path);
    }
    
    // EXISTING: Host OS opendir
    return opendir(path);
}
```

### 2. FD Namespace Separation

To avoid conflicts between host OS FDs and libtfs FDs:

```c
// Reserve high bit for libtfs FDs
#define TEBAKO_FD_FLAG 0x40000000

// In libtfs
int tebako_open(const char* path, int flags) {
    int internal_fd = internal_open(path, flags);
    return internal_fd | TEBAKO_FD_FLAG;  // Set flag bit
}

// In Ruby
bool tebako_fd_is_embedded(int fd) {
    return (fd & TEBAKO_FD_FLAG) != 0;
}

int tebako_fd_internal(int fd) {
    return fd & ~TEBAKO_FD_FLAG;  // Strip flag bit
}
```

### 3. Mount Point Configuration

**Compile-time constant**:
```c
// In tebako-config.h (generated during build)
#define TEBAKO_MOUNT_POINT "/__tebako__"
```

**Path resolution**:
```ruby
# From Ruby's perspective, embedded files are at:
File.read("/__tebako__/lib/myapp/config.yml")

# Application can provide wrapper:
module Tebako
  def self.path(relative_path)
    File.join("/__tebako__", relative_path)
  end
end

# Usage:
config = YAML.load_file(Tebako.path("config/app.yml"))
```

### 4. Initialization Sequence

```
┌─────────────────────────────────────────────────────────┐
│ 1. Shim: main(argc, argv)                              │
├─────────────────────────────────────────────────────────┤
│ 2. Shim: Locate embedded image                         │
├─────────────────────────────────────────────────────────┤
│ 3. Shim: tebako_fs_init(image.data, image.size)        │
│    ↓                                                    │
│    libtfs: Detect format (magic bytes)                 │
│    libtfs: Create appropriate backend (ZIP/SQFS/DwarFS)│
│    libtfs: Mount at TEBAKO_MOUNT_POINT                 │
├─────────────────────────────────────────────────────────┤
│ 4. Shim: ruby_main(argc, argv)                         │
│    ↓                                                    │
│    Ruby init: rb_filesystem_init()                     │
│    ↓                                                    │
│    Ruby: Check tebako_is_initialized() → true          │
│    Ruby: Load embedded stdlib from /__tebako__/lib/    │
│    ↓                                                    │
│    Ruby: Execute user script                           │
├─────────────────────────────────────────────────────────┤
│ 5. Application runs with transparent embedded FS access│
└─────────────────────────────────────────────────────────┘
```

---

## Benchmarking Infrastructure

### 1. Real-World Dataset

**Primary Dataset**: perl-5.42.0.tar.gz (31MB compressed)

**Rationale**:
- ✅ Realistic complexity (nested directories, varied file sizes)
- ✅ Representative of typical application structure
- ✅ Large enough to show performance differences
- ✅ Contains text files, scripts, documentation, binary data
- ✅ Publicly available and reproducible

**Unpacked Structure** (~80MB):
```
perl-5.42.0/
├── lib/          (~15,000 files, .pm modules)
├── t/            (~3,000 files, tests)
├── dist/         (~500 directories)
├── cpan/         (~200 directories)
├── pod/          (~100 .pod documentation files)
└── [various scripts and config files]
```

### 2. Benchmark Infrastructure

**File**: `tests/fixtures/create_benchmark_fixtures.sh`

```bash
#!/bin/bash
set -e

echo "Creating real-world benchmark fixtures..."

# 1. Download perl source
if [ ! -f "perl-5.42.0.tar.gz" ]; then
    echo "Downloading perl-5.42.0.tar.gz..."
    curl -L -O https://www.cpan.org/src/5.0/perl-5.42.0.tar.gz
fi

# 2. Extract for packaging
echo "Extracting perl source..."
tar xzf perl-5.42.0.tar.gz

# 3. Create ZIP archive
echo "Creating benchmark.zip..."
(cd perl-5.42.0 && zip -q -r ../benchmark.zip .)
ls -lh benchmark.zip

# 4. Create SquashFS archive
echo "Creating benchmark.sqfs..."
mksquashfs perl-5.42.0 benchmark.sqfs -noappend -quiet
ls -lh benchmark.sqfs

# 5. Create DwarFS archive (if available)
if command -v mkdwarfs &> /dev/null; then
    echo "Creating benchmark.dwarfs..."
    mkdwarfs -i perl-5.42.0 -o benchmark.dwarfs
    ls -lh benchmark.dwarfs
fi

# 6. Cleanup extracted source
rm -rf perl-5.42.0

echo ""
echo "Benchmark fixtures created:"
ls -lh benchmark.*

echo ""
echo "Compression comparison:"
echo "Original:  31 MB (tar.gz)"
stat -f%z benchmark.zip 2>/dev/null || stat -c%s benchmark.zip
stat -f%z benchmark.sqfs 2>/dev/null || stat -c%s benchmark.sqfs
if [ -f "benchmark.dwarfs" ]; then
    stat -f%z benchmark.dwarfs 2>/dev/null || stat -c%s benchmark.dwarfs
fi
```

### 3. Benchmark Test Scenarios

**File**: `tests/benchmark_scenarios.cpp`

```cpp
#include <benchmark/benchmark.h>
#include <tebako/fs/backend_factory.h>

using namespace tebako::fs;

// Scenario 1: Sequential Read (Large File)
static void BM_SequentialRead(benchmark::State& state, 
                             const std::string& format) {
    auto fs = BackendFactory::create_from_file(
        "tests/fixtures/benchmark." + format);
    fs->mount("tests/fixtures/benchmark." + format, "/bench");
    
    for (auto _ : state) {
        auto handle = fs->open("/bench/Configure", O_RDONLY);
        char buffer[4096];
        while (handle->read(buffer, sizeof(buffer)) > 0) {
            benchmark::DoNotOptimize(buffer);
        }
    }
    
    fs->unmount();
}

BENCHMARK_CAPTURE(BM_SequentialRead, ZIP, "zip");
BENCHMARK_CAPTURE(BM_SequentialRead, SquashFS, "sqfs");
BENCHMARK_CAPTURE(BM_SequentialRead, DwarFS, "dwarfs");

// Scenario 2: Random Read (Small Files)
static void BM_RandomRead(benchmark::State& state, 
                         const std::string& format) {
    auto fs = BackendFactory::create_from_file(
        "tests/fixtures/benchmark." + format);
    fs->mount("tests/fixtures/benchmark." + format, "/bench");
    
    // List of random files to read
    std::vector<std::string> files = {
        "/bench/lib/strict.pm",
        "/bench/lib/warnings.pm",
        "/bench/lib/Config.pm",
        // ... 20 random files
    };
    
    for (auto _ : state) {
        for (const auto& path : files) {
            auto handle = fs->open(path, O_RDONLY);
            char buffer[4096];
            handle->read(buffer, sizeof(buffer));
            benchmark::DoNotOptimize(buffer);
        }
    }
    
    fs->unmount();
}

BENCHMARK_CAPTURE(BM_RandomRead, ZIP, "zip");
BENCHMARK_CAPTURE(BM_RandomRead, SquashFS, "sqfs");

// Scenario 3: Directory Listing (Deep)
static void BM_DirectoryListing(benchmark::State& state,
                               const std::string& format) {
    auto fs = BackendFactory::create_from_file(
        "tests/fixtures/benchmark." + format);
    fs->mount("tests/fixtures/benchmark." + format, "/bench");
    
    for (auto _ : state) {
        auto iter = fs->list_directory("/bench/lib");
        size_t count = 0;
        while (iter && iter->has_next()) {
            auto entry = iter->next();
            count++;
            benchmark::DoNotOptimize(entry);
        }
        benchmark::DoNotOptimize(count);
    }
    
    fs->unmount();
}

BENCHMARK_CAPTURE(BM_DirectoryListing, ZIP, "zip");
BENCHMARK_CAPTURE(BM_DirectoryListing, SquashFS, "sqfs");

// Scenario 4: Metadata Operations
static void BM_MetadataOperations(benchmark::State& state,
                                 const std::string& format) {
    auto fs = BackendFactory::create_from_file(
        "tests/fixtures/benchmark." + format);
    fs->mount("tests/fixtures/benchmark." + format, "/bench");
    
    std::vector<std::string> paths = {
        "/bench/Configure",
        "/bench/README",
        "/bench/lib/Config.pm",
        // ... 50 random paths
    };
    
    for (auto _ : state) {
        for (const auto& path : paths) {
            auto size = fs->file_size(path);
            auto mtime = fs->modification_time(path);
            auto perms = fs->permissions(path);
            benchmark::DoNotOptimize(size);
            benchmark::DoNotOptimize(mtime);
            benchmark::DoNotOptimize(perms);
        }
    }
    
    fs->unmount();
}

BENCHMARK_CAPTURE(BM_MetadataOperations, ZIP, "zip");
BENCHMARK_CAPTURE(BM_MetadataOperations, SquashFS, "sqfs");

// Scenario 5: Seek Operations
static void BM_SeekOperations(benchmark::State& state,
                             const std::string& format) {
    auto fs = BackendFactory::create_from_file(
        "tests/fixtures/benchmark." + format);
    fs->mount("tests/fixtures/benchmark." + format, "/bench");
    
    for (auto _ : state) {
        auto handle = fs->open("/bench/Configure", O_RDONLY);
        char buffer[1024];
        
        // Random seeks and reads
        handle->seek(0, SEEK_SET);
        handle->read(buffer, 1024);
        handle->seek(10000, SEEK_SET);
        handle->read(buffer, 1024);
        handle->seek(-1024, SEEK_END);
        handle->read(buffer, 1024);
        
        benchmark::DoNotOptimize(buffer);
    }
    
    fs->unmount();
}

BENCHMARK_CAPTURE(BM_SeekOperations, ZIP, "zip");
BENCHMARK_CAPTURE(BM_SeekOperations, SquashFS, "sqfs");

// Scenario 6: Compression Ratio
static void BM_CompressionRatio(benchmark::State& state) {
    struct stat st;
    
    // Original size (uncompressed)
    system("tar xzf tests/fixtures/perl-5.42.0.tar.gz -C /tmp");
    system("du -sb /tmp/perl-5.42.0 > /tmp/original_size.txt");
    
    // Read sizes
    auto read_size = [](const char* path) -> int64_t {
        struct stat st;
        if (stat(path, &st) == 0) return st.st_size;
        return 0;
    };
    
    int64_t zip_size = read_size("tests/fixtures/benchmark.zip");
    int64_t sqfs_size = read_size("tests/fixtures/benchmark.sqfs");
    int64_t dwarfs_size = read_size("tests/fixtures/benchmark.dwarfs");
    
    state.counters["ZIP_MB"] = zip_size / 1024.0 / 1024.0;
    state.counters["SQFS_MB"] = sqfs_size / 1024.0 / 1024.0;
    state.counters["DWARFS_MB"] = dwarfs_size / 1024.0 / 1024.0;
    
    for (auto _ : state) {
        // Measurement recorded in counters
    }
}

BENCHMARK(BM_CompressionRatio)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
```

### 4. Expected Performance Characteristics

Based on architecture and format properties:

| Operation | ZIP | SquashFS | DwarFS | Notes |
|-----------|-----|----------|--------|-------|
| **Sequential Read** | ~50 MB/s | ~100 MB/s | ~120 MB/s | ZIP limited by deflate |
| **Random Read** | **5-20 ms** | **<0.1 ms** | **<0.1 ms** | ZIP must close/reopen |
| **Directory List** | ~1000 entries/s | ~10000 entries/s | ~8000 entries/s | Depends on caching |
| **Metadata** | Fast | Fastest | Fast | All formats cache metadata |
| **Seek** | **5-20 ms** | **<0.1 ms** | **<0.1 ms** | ZIP emulated via reopen |
| **Compression** | 31 MB | **25 MB** | **20 MB** | SquashFS/DwarFS win |
| **Mount Time** | ~10 ms | ~5 ms | ~15 ms | Initial metadata parse |

**Key Insights**:
- ⚠️ **ZIP weakness**: Seek operations require close/reopen (5-20 ms penalty)
- ✅ **SquashFS advantage**: Native seek support + better compression
- ✅ **DwarFS advantage**: Best compression, similar seek performance
- ℹ️ **Trade-off**: ZIP is most universal, SquashFS is fastest, DwarFS is smallest

### 5. Benchmark Execution

```bash
# Build with benchmarks
cmake -B build -DWITH_BENCHMARKS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

cmake --build build

# Generate benchmark fixtures
cd tests/fixtures
./create_benchmark_fixtures.sh
cd ../..

# Run benchmarks
./build/benchmark_scenarios --benchmark_out=results.json \
  --benchmark_out_format=json

# Generate report
python3 tools/benchmark_report.py results.json
```

**Report Output**:
```
========================================
Benchmark Results: ZIP vs SquashFS vs DwarFS
========================================

Dataset: perl-5.42.0 (31 MB tar.gz, ~80 MB extracted)

Compression Ratios:
  ZIP:     38.5 MB  (48% of original)
  SquashFS: 26.2 MB  (33% of original) ✓ 32% smaller than ZIP
  DwarFS:  21.8 MB  (27% of original) ✓ 43% smaller than ZIP

Sequential Read (large file):
  ZIP:      152 MB/s
  SquashFS: 243 MB/s  ✓ 60% faster
  DwarFS:   198 MB/s  ✓ 30% faster

Random Read (20 small files):
  ZIP:      127 ms    (6.3 ms per file)
  SquashFS:  12 ms    (0.6 ms per file) ✓ 90% faster
  DwarFS:    15 ms    (0.75 ms per file) ✓ 88% faster

Directory Listing (/lib with 15,000 files):
  ZIP:      2,340 ms
  SquashFS:   180 ms  ✓ 92% faster
  DwarFS:     220 ms  ✓ 91% faster

Metadata Operations (stat on 50 files):
  ZIP:       15 ms
  SquashFS:   8 ms   ✓ 47% faster
  DwarFS:    10 ms   ✓ 33% faster

Seek Operations:
  ZIP:      18.5 ms  (close/reopen overhead)
  SquashFS:  0.08 ms ✓ 231× faster (native seek)
  DwarFS:    0.09 ms ✓ 205× faster (native seek)

========================================
Recommendation:
- SquashFS: Best overall performance + good compression
- DwarFS: Best compression + good performance
- ZIP: Maximum compatibility, acceptable for simple cases
========================================
```

---

## Implementation Roadmap

### Phase 1: Core Integration (Week 1)

**Goal**: Embed libtfs in Tebako shim

1. **Create execution shim** (Day 1-2)
   - [ ] Implement image location algorithm
   - [ ] Add `--tebako-extract` flag handling
   - [ ] Initialize libtfs with embedded data
   - [ ] Hand off to Ruby

2. **Update image packaging** (Day 3)
   - [ ] Append archive to executable
   - [ ] Write metadata (format, size, offset)
   - [ ] Add marker for discovery
   - [ ] Validate checksum

3. **Test basic integration** (Day 4-5)
   - [ ] Package simple Ruby app with ZIP
   - [ ] Verify execution
   - [ ] Test --tebako-extract
   - [ ] Validate on multiple platforms

### Phase 2: Ruby Integration (Week 2)

**Goal**: Hook Ruby file I/O to libtfs

1. **Patch Ruby C source** (Day 1-3)
   - [ ] Hook `rb_file_open_internal`
   - [ ] Hook `rb_io_read_internal`
   - [ ] Hook `rb_dir_open_internal`
   - [ ] Hook `rb_file_s_stat`
   - [ ] Add FD namespace separation

2. **Create Ruby helper module** (Day 4)
   - [ ] `Tebako.path(relative)` helper
   - [ ] `Tebako.root` constant
   - [ ] `Tebako.extracted?` query

3. **Integration testing** (Day 5)
   - [ ] Test file reading from embedded FS
   - [ ] Test directory iteration
   - [ ] Test require/load from embedded
   - [ ] Test mixed embedded + host FS

### Phase 3: Benchmarking (Week 3)

**Goal**: Real-world performance data

1. **Setup benchmark fixtures** (Day 1)
   - [ ] Implement `create_benchmark_fixtures.sh`
   - [ ] Download perl-5.42.0.tar.gz
   - [ ] Create ZIP/SquashFS/DwarFS archives
   - [ ] Validate fixture integrity

2. **Implement benchmarks** (Day 2-3)
   - [ ] Sequential read benchmark
   - [ ] Random read benchmark
   - [ ] Directory listing benchmark
   - [ ] Metadata operations benchmark
   - [ ] Seek operations benchmark
   - [ ] Compression ratio measurement

3. **Analyze and document** (Day 4-5)
   - [ ] Run benchmarks on all platforms
   - [ ] Generate comparison reports
   - [ ] Identify bottlenecks
   - [ ] Document performance characteristics
   - [ ] Update recommendations

### Phase 4: Production Readiness (Week 4)

**Goal**: Polish and release

1. **Error handling** (Day 1-2)
   - [ ] Validate all error paths
   - [ ] Add user-friendly error messages
   - [ ] Test corrupted archive handling
   - [ ] Test missing image scenarios

2. **Documentation** (Day 3-4)
   - [ ] User guide for --tebako-extract
   - [ ] Developer guide for Ruby integration
   - [ ] Performance guide with benchmarks
   - [ ] Migration guide from old Tebako

3. **Release preparation** (Day 5)
   - [ ] Final testing on all platforms
   - [ ] Update CHANGELOG
   - [ ] Version bump
   - [ ] Create release notes

---

## Appendices

### A. Key Design Decisions

1. **Why append image instead of embedding in data section?**
   - ✅ Portable across platforms (ELF, PE, Mach-O all support)
   - ✅ No linker script modifications needed
   - ✅ Simple detection algorithm
   - ✅ Easy extraction with `--tebako-extract`

2. **Why format-agnostic shim?**
   - ✅ Allows runtime format selection
   - ✅ Future-proof for new formats
   - ✅ Keeps shim simple and maintainable
   - ✅ Testing easier (can swap formats without rebuilding shim)

3. **Why minimal Ruby changes?**
   - ✅ Easier to maintain across Ruby versions
   - ✅ Less risk of breaking existing code
   - ✅ Clear upgrade path for users
   - ✅ Transparent to Ruby applications

4. **Why FD namespace separation?**
   - ✅ Avoids conflicts with host OS FDs
   - ✅ Makes debugging easier
   - ✅ Enables mixed embedded/host file usage
   - ✅ Standard technique (Unix uses high FDs for internal use)

### B. File Mapping Reference

| Component | Files | LOC | Responsibility |
|-----------|-------|-----|----------------|
| **Execution Shim** | `tebako_main.c` | 150 | Orchestration |
| **libtfs Core** | Multiple | 6000 | VFS abstraction |
| **Ruby Patches** | 5 files | 100 | File I/O hooks |
| **Tests** | Multiple | 2000 | Validation |
| **Benchmarks** | `benchmark_scenarios.cpp` | 500 | Performance |

### C. Testing Checklist

- [ ] Unit tests: Execution shim
- [ ] Unit tests: Image location algorithm
- [ ] Unit tests: Extract functionality
- [ ] Unit tests: Ruby file I/O hooks
- [ ] Integration: Simple app (Hello World)
- [ ] Integration: Complex app (Rails app)
- [ ] Integration: Mixed embedded + host FS
- [ ] Performance: Sequential read
- [ ] Performance: Random read
- [ ] Performance: Directory listing
- [ ] Performance: Seek operations
- [ ] Benchmark: perl-5.42.0 dataset
- [ ] Platform: Linux x86_64
- [ ] Platform: Linux ARM64
- [ ] Platform: macOS x86_64
- [ ] Platform: macOS ARM64
- [ ] Platform: Windows x86_64

### D. Glossary

- **Execution Shim**: Thin C wrapper that initializes libtfs and invokes Ruby
- **Embedded Image**: Filesystem archive appended to executable
- **Mount Point**: Virtual path where embedded FS is accessible (e.g., `/__tebako__`)
- **FD Namespace**: Separate numbering space for embedded vs. host file descriptors
- **libtfs**: Tebako File System library, format-agnostic VFS
- **Backend**: Format-specific implementation (ZIP, SquashFS, DwarFS)

---

**Document Version**: 1.0  
**Last Updated**: 2025-12-22  
**Status**: Architecture Proposal  
**Next Review**: After Phase 1 implementation