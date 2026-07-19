# Stage 2 Implementation: ZIP Backend

**Duration**: 1 week
**Status**: Ready to execute
**Goal**: Add ZIP backend to demonstrate multi-backend architecture
**Prerequisites**: Stage 1 completed

---

## Overview

Add ZIP archive support as a second filesystem backend, demonstrating libtfs's multi-backend capability. This validates the pluggable architecture and provides a lightweight alternative to DwarFS.

**Key Decision**: Use **miniz** (header-only, public domain) instead of libzip.

---

## Motivation

### Why ZIP Backend?

1. **Validation** - Proves multi-backend architecture works
2. **Lightweight** - Simpler than DwarFS for basic needs
3. **Universal** - ZIP is ubiquitous and well-understood
4. **Testing** - Easy to create test archives
5. **Complementary** - Different trade-offs than DwarFS

### Use Cases

- **Development**: Fast iteration without DwarFS overhead
- **Simple packaging**: When compression isn't critical
- **Debugging**: Human-readable with standard tools
- **Cross-platform**: ZIP tools everywhere

---

## Architecture

### Backend Abstraction

```cpp
// include/tebako/fs/backend.h
namespace tebako::fs {

class Backend {
public:
    virtual ~Backend() = default;

    // Lifecycle
    virtual bool mount(const std::string& path, const std::string& mountpoint) = 0;
    virtual bool unmount() = 0;

    // File operations
    virtual int open(const char* path, int flags) = 0;
    virtual ssize_t read(int fd, void* buf, size_t count) = 0;
    virtual int close(int fd) = 0;

    // Directory operations
    virtual bool opendir(const char* path) = 0;
    virtual const char* readdir() = 0;
    virtual void closedir() = 0;

    // Metadata
    virtual bool stat(const char* path, struct stat* st) = 0;
    virtual std::string type() const = 0;
};

class DwarfsBackend : public Backend { /* existing */ };
class ZipBackend : public Backend { /* new */ };

} // namespace tebako::fs
```

### Backend Registry

```cpp
// include/tebako/fs/backend_registry.h
namespace tebako::fs {

class BackendRegistry {
public:
    static BackendRegistry& instance();

    void register_backend(const std::string& type,
                         std::function<std::unique_ptr<Backend>()> factory);

    std::unique_ptr<Backend> create(const std::string& type);

    std::vector<std::string> available_backends() const;
};

} // namespace tebako::fs
```

---

## Week Plan

### Day 1: Setup & Design

#### Morning: Dependency Integration

**Add miniz**:
```cmake
# CMakeLists.txt
FetchContent_Declare(
  miniz
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG        3.0.2
)
FetchContent_MakeAvailable(miniz)
```

**Verify**:
```bash
cd build
cmake ..
# Check miniz available
```

#### Afternoon: Backend Interface

**File**: `include/tebako/fs/backend.h`

Create abstract backend interface (see Architecture section above).

**Test**:
```bash
# Compile check
mkdir -p build && cd build
cmake -DTEBAKO_BUILD=ON ..
make -j$(nproc)
```

### Day 2: ZIP Implementation

#### Morning: Basic ZIP Backend

**File**: `include/tebako/fs/backends/zip_backend.h`

```cpp
#pragma once
#include <tebako/fs/backend.h>
#include <miniz.h>
#include <unordered_map>
#include <vector>

namespace tebako::fs {

class ZipBackend : public Backend {
public:
    ZipBackend();
    ~ZipBackend() override;

    bool mount(const std::string& path, const std::string& mountpoint) override;
    bool unmount() override;

    int open(const char* path, int flags) override;
    ssize_t read(int fd, void* buf, size_t count) override;
    int close(int fd) override;

    bool opendir(const char* path) override;
    const char* readdir() override;
    void closedir() override;

    bool stat(const char* path, struct stat* st) override;
    std::string type() const override { return "zip"; }

private:
    struct FileEntry {
        size_t index;
        size_t size;
        size_t compressed_size;
        time_t mtime;
        bool is_dir;
    };

    mz_zip_archive archive_;
    std::unordered_map<std::string, FileEntry> entries_;
    std::unordered_map<int, std::vector<char>> open_files_;
    int next_fd_ = 1000;  // Start high to avoid conflicts

    bool build_file_index();
};

} // namespace tebako::fs
```

#### Afternoon: Implementation

**File**: `src/zip_backend.cpp`

```cpp
#include <tebako/fs/backends/zip_backend.h>
#include <algorithm>

namespace tebako::fs {

ZipBackend::ZipBackend() {
    memset(&archive_, 0, sizeof(archive_));
}

ZipBackend::~ZipBackend() {
    unmount();
}

bool ZipBackend::mount(const std::string& path, const std::string& mountpoint) {
    if (!mz_zip_reader_init_file(&archive_, path.c_str(), 0)) {
        return false;
    }
    return build_file_index();
}

bool ZipBackend::unmount() {
    open_files_.clear();
    entries_.clear();
    mz_zip_reader_end(&archive_);
    return true;
}

bool ZipBackend::build_file_index() {
    size_t num_files = mz_zip_reader_get_num_files(&archive_);

    for (size_t i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&archive_, i, &file_stat)) {
            continue;
        }

        std::string filename = file_stat.m_filename;

        FileEntry entry;
        entry.index = i;
        entry.size = file_stat.m_uncomp_size;
        entry.compressed_size = file_stat.m_comp_size;
        entry.mtime = file_stat.m_time;
        entry.is_dir = mz_zip_reader_is_file_a_directory(&archive_, i);

        entries_[filename] = entry;
    }

    return true;
}

int ZipBackend::open(const char* path, int flags) {
    // Remove leading slash
    std::string clean_path = path;
    if (!clean_path.empty() && clean_path[0] == '/') {
        clean_path = clean_path.substr(1);
    }

    auto it = entries_.find(clean_path);
    if (it == entries_.end()) {
        errno = ENOENT;
        return -1;
    }

    if (it->second.is_dir) {
        errno = EISDIR;
        return -1;
    }

    // Extract file data
    size_t size = it->second.size;
    std::vector<char> data(size);

    if (!mz_zip_reader_extract_to_mem(&archive_, it->second.index,
                                       data.data(), size, 0)) {
        errno = EIO;
        return -1;
    }

    int fd = next_fd_++;
    open_files_[fd] = std::move(data);

    return fd;
}

ssize_t ZipBackend::read(int fd, void* buf, size_t count) {
    auto it = open_files_.find(fd);
    if (it == open_files_.end()) {
        errno = EBADF;
        return -1;
    }

    // For simplicity, read all at once on first read
    size_t to_copy = std::min(count, it->second.size());
    memcpy(buf, it->second.data(), to_copy);

    return to_copy;
}

int ZipBackend::close(int fd) {
    auto it = open_files_.find(fd);
    if (it == open_files_.end()) {
        errno = EBADF;
        return -1;
    }

    open_files_.erase(it);
    return 0;
}

bool ZipBackend::stat(const char* path, struct stat* st) {
    std::string clean_path = path;
    if (!clean_path.empty() && clean_path[0] == '/') {
        clean_path = clean_path.substr(1);
    }

    auto it = entries_.find(clean_path);
    if (it == entries_.end()) {
        errno = ENOENT;
        return false;
    }

    memset(st, 0, sizeof(*st));
    st->st_size = it->second.size;
    st->st_mtime = it->second.mtime;
    st->st_mode = it->second.is_dir ? S_IFDIR | 0755 : S_IFREG | 0644;

    return true;
}

// Directory operations omitted for brevity - similar pattern

} // namespace tebako::fs
```

### Day 3: Integration

#### Morning: Backend Registry

**File**: `src/backend_registry.cpp`

```cpp
#include <tebako/fs/backend_registry.h>
#include <tebako/fs/backends/dwarfs_backend.h>
#include <tebako/fs/backends/zip_backend.h>

namespace tebako::fs {

BackendRegistry& BackendRegistry::instance() {
    static BackendRegistry registry;
    return registry;
}

// Constructor registers built-in backends
BackendRegistry::BackendRegistry() {
    register_backend("dwarfs", []() {
        return std::make_unique<DwarfsBackend>();
    });

    register_backend("zip", []() {
        return std::make_unique<ZipBackend>();
    });
}

std::unique_ptr<Backend> BackendRegistry::create(const std::string& type) {
    auto it = factories_.find(type);
    if (it == factories_.end()) {
        return nullptr;
    }
    return it->second();
}

} // namespace tebako::fs
```

#### Afternoon: Mount Table Updates

**File**: `src/tebako-mount-table.cpp`

Update to support backend selection:

```cpp
bool mount_filesystem(const char* path, const char* mountpoint,
                     const char* backend_type) {
    auto& registry = BackendRegistry::instance();

    // Auto-detect if not specified
    std::string type = backend_type ? backend_type : detect_type(path);

    auto backend = registry.create(type);
    if (!backend) {
        return false;
    }

    if (!backend->mount(path, mountpoint)) {
        return false;
    }

    // Store in mount table
    mount_table_[mountpoint] = std::move(backend);
    return true;
}

std::string detect_type(const char* path) {
    std::string p = path;

    // Check extension
    if (ends_with(p, ".dwarfs")) return "dwarfs";
    if (ends_with(p, ".zip")) return "zip";

    // Check magic number
    // TODO: Implement magic number detection

    return "dwarfs";  // Default
}
```

### Day 4: Testing

#### Morning: Unit Tests

**File**: `tests/tests-zip-backend.cpp`

```cpp
#include <gtest/gtest.h>
#include <tebako/fs/backends/zip_backend.h>

class ZipBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        backend = std::make_unique<tebako::fs::ZipBackend>();

        // Create test ZIP
        create_test_zip();
    }

    void create_test_zip() {
        // Use system zip command
        system("cd tests/test_filesystem && zip -r ../test.zip .");
    }

    std::unique_ptr<tebako::fs::ZipBackend> backend;
};

TEST_F(ZipBackendTest, MountSuccess) {
    EXPECT_TRUE(backend->mount("tests/test.zip", "/mnt/test"));
}

TEST_F(ZipBackendTest, ReadFile) {
    backend->mount("tests/test.zip", "/mnt/test");

    int fd = backend->open("/file.txt", O_RDONLY);
    ASSERT_GT(fd, 0);

    char buf[1024];
    ssize_t n = backend->read(fd, buf, sizeof(buf));
    ASSERT_GT(n, 0);

    backend->close(fd);
}

TEST_F(ZipBackendTest, StatFile) {
    backend->mount("tests/test.zip", "/mnt/test");

    struct stat st;
    EXPECT_TRUE(backend->stat("/file.txt", &st));
    EXPECT_GT(st.st_size, 0);
    EXPECT_TRUE(S_ISREG(st.st_mode));
}
```

#### Afternoon: Integration Tests

**File**: `tests/tests-multi-backend.cpp`

```cpp
TEST(MultiBackendTest, DwarfsAndZip) {
    // Mount both backends
    EXPECT_TRUE(mount_filesystem("tests/test.dwarfs", "/mnt/dwarfs", "dwarfs"));
    EXPECT_TRUE(mount_filesystem("tests/test.zip", "/mnt/zip", "zip"));

    // Verify both work
    EXPECT_TRUE(file_exists("/mnt/dwarfs/file.txt"));
    EXPECT_TRUE(file_exists("/mnt/zip/file.txt"));

    // Read from both
    auto dwarfs_content = read_file("/mnt/dwarfs/file.txt");
    auto zip_content = read_file("/mnt/zip/file.txt");

    EXPECT_EQ(dwarfs_content, zip_content);
}
```

### Day 5: Documentation & Polish

#### Morning: Documentation

**Update README.md**:

```markdown
## Supported Backends

LibTFS supports multiple archive formats as virtual filesystems:

### DwarFS (Recommended)
- **Compression**: Excellent
- **Speed**: Fast
- **Use Case**: Production deployments

```bash
mount_filesystem("app.dwarfs", "/app", "dwarfs");
```

### ZIP
- **Compression**: Good
- **Speed**: Fast
- **Use Case**: Development, simple packaging

```bash
mount_filesystem("app.zip", "/app", "zip");
```

### Auto-Detection

Backend is auto-detected from file extension:

```cpp
mount_filesystem("app.dwarfs", "/app");  // Uses DwarFS
mount_filesystem("app.zip", "/app");     // Uses ZIP
```
```

**Add**: `docs/BACKEND_COMPARISON.md`

#### Afternoon: Performance Testing

**File**: `tests/benchmarks/backend_comparison.cpp`

```cpp
void benchmark_mount_time() {
    auto start = now();
    mount_filesystem("large.dwarfs", "/mnt/test1", "dwarfs");
    auto dwarfs_time = elapsed(start);

    start = now();
    mount_filesystem("large.zip", "/mnt/test2", "zip");
    auto zip_time = elapsed(start);

    std::cout << "Mount time:\n";
    std::cout << "  DwarFS: " << dwarfs_time << "ms\n";
    std::cout << "  ZIP: " << zip_time << "ms\n";
}
```

---

## Success Criteria

- [ ] ZIP backend mounts archives
- [ ] File read operations work
- [ ] Directory traversal works
- [ ] Auto-detection functional
- [ ] All tests passing
- [ ] Performance acceptable
- [ ] Documentation complete
- [ ] No DwarFS regression

---

## API Examples

### Basic Usage

```cpp
#include <tebako/fs/io.h>

// Mount ZIP archive
if (mount_filesystem("app.zip", "/app")) {
    // Read file
    int fd = open("/app/config.json", O_RDONLY);
    // ... use file ...
    close(fd);
}
```

### Backend Selection

```cpp
// Explicit backend
mount_filesystem("archive.bin", "/mnt", "dwarfs");

// Auto-detect
mount_filesystem("archive.zip", "/mnt");  // Detects ZIP

// List available backends
auto backends = BackendRegistry::instance().available_backends();
for (const auto& backend : backends) {
    std::cout << backend << "\n";
}
// Output: dwarfs, zip
```

---

## Future Backends (Stage 3+)

Potential additions:
- **SquashFS**: Linux-native format
- **TAR**: Simple, streaming
- **ISO 9660**: CD-ROM images
- **Custom**: Application-specific formats

---

## Rollback Plan

If issues arise:
```bash
# Disable ZIP backend
cmake -DWITH_ZIP_BACKEND=OFF ..

# Remove files
git checkout HEAD -- include/tebako/fs/backends/zip_backend.h
git checkout HEAD -- src/zip_backend.cpp
```

---

## Next Stage

After completion: [STAGE_3_IMPLEMENTATION.md](STAGE_3_IMPLEMENTATION.md)

---

**Last Updated**: 2025-01-17