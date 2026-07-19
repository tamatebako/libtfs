# Multi-Language Architecture: Supporting Julia, Python, Node.js, etc.

## Vision

Transform libdwarfs-wr from a Ruby-specific library into a **generic VFS core** with **pluggable language adapters**, enabling Tebako packaging for Julia, Python, Node.js, and other languages.

---

## Current Architecture (Ruby-Only)

```
┌─────────────────────┐
│ Ruby Application    │
└────────┬────────────┘
         │ rb_w32_* macros
┌────────▼──────────────────────────────┐
│ libdwarfs-wr (MONOLITHIC)             │
│ ├─ Ruby shims (40%)                   │
│ ├─ Tebako features (30%)              │
│ └─ VFS core (30%)                     │
│    All Mixed Together                  │
└────────┬──────────────────────────────┘
         │
┌────────▼────────┐
│ DwarFS Backend  │
└─────────────────┘
```

**Problems:**
- Ruby code mixed with VFS core
- Cannot reuse for Julia/Python/Node.js
- Violates separation of concerns
- Hard to maintain and extend

---

## Target Architecture (Multi-Language)

```
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│  Ruby    │  │  Julia   │  │  Python  │  │  Node.js │
│   App    │  │   App    │  │   App    │  │   App    │
└─────┬────┘  └─────┬────┘  └─────┬────┘  └─────┬────┘
      │             │              │              │
      │ rb_w32_*    │ jl_*         │ Py_*         │ N-API
┌─────▼─────────────▼──────────────▼──────────────▼─────┐
│          Language Adapter Layer (Pluggable)            │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ │
│  │  Ruby    │ │  Julia   │ │  Python  │ │  Node.js │ │
│  │ Adapter  │ │ Adapter  │ │ Adapter  │ │ Adapter  │ │
│  └─────┬────┘ └─────┬────┘ └─────┬────┘ └─────┬────┘ │
└────────┼────────────┼─────────────┼─────────────┼──────┘
         └────────────┴─────────────┴─────────────┘
                          │
┌─────────────────────────▼──────────────────────────────┐
│         Tebako VFS Core (Language-Agnostic)            │
│  ┌───────────────────────────────────────────────┐    │
│  │ Stable C API                                   │    │
│  │  - tebako_vfs_mount(path, backend, options)   │    │
│  │  - tebako_vfs_open(path) → handle             │    │
│  │  - tebako_vfs_read(handle, buf, size)         │    │
│  │  - tebako_vfs_stat(path, stat)                │    │
│  └───────────────────────────────────────────────┘    │
│  ┌───────────────────────────────────────────────┐    │
│  │ VFS Core Implementation                        │    │
│  │  - Mount table                                 │    │
│  │  - FD table                                    │    │
│  │  - Filesystem registry                         │    │
│  │  - Path resolution                             │    │
│  └───────────────────────────────────────────────┘    │
└────────────┬──────────────────────────────────────────┘
             │ Backend Interface
┌────────────▼──────────────────────────────────────────┐
│         Filesystem Backend Layer (Pluggable)           │
│  ┌──────────┐ ┌───────────┐ ┌────────┐ ┌────────┐   │
│  │  DwarFS  │ │ SquashFS  │ │  ZIP   │ │  TAR   │   │
│  │ Backend  │ │  Backend  │ │ Backend│ │ Backend│   │
│  └──────────┘ └───────────┘ └────────┘ └────────┘   │
└────────────────────────────────────────────────────────┘
```

---

## Refactoring Strategy

### Phase 1: Extract VFS Core (Week 1)

#### 1.1 Create Clean Core API

**New file:** `include/tebako/vfs/core.h`

```cpp
namespace tebako {
namespace vfs {

/**
 * VFS Core - Language-agnostic virtual filesystem
 */

// Opaque handle types
typedef struct vfs_handle_* VFSHandle;
typedef struct vfs_mount_* VFSMount;

// Core operations
int vfs_init(void);
void vfs_cleanup(void);

VFSMount vfs_mount(const char* mount_point,
                   const void* fs_data,
                   size_t fs_size,
                   cons char* fs_type);  // "dwarfs", "squashfs", etc.

int vfs_unmount(VFSMount mount);

VFSHandle vfs_open(const char* path, int flags);
ssize_t vfs_read(VFSHandle handle, void* buf, size_t size);
off_t vfs_seek(VFSHandle handle, off_t offset, int whence);
int vfs_stat(const char* path, struct stat* st);
int vfs_close(VFSHandle handle);

// Directory operations
typedef struct vfs_dir_* VFSDir;
VFSDir vfs_opendir(const char* path);
struct dirent* vfs_readdir(VFSDir dir);
int vfs_closedir(VFSDir dir);

}} // namespace tebako::vfs
```

#### 1.2 Separate VFS Core Implementation

**Move to:** `src/vfs/` directory

```
src/vfs/
├── core.cpp           # VFS initialization, cleanup
├── mount_table.cpp    # From tebako-mount-table.cpp
├── fd_table.cpp       # From tebako-fd.cpp
├── path_resolver.cpp  # Path resolution logic
└── filesystem_registry.cpp  # Backend registry
```

#### 1.3 Define Backend Interface

**New file:** `include/tebako/vfs/backend.h`

```cpp
namespace tebako {
namespace vfs {

/**
 * Filesystem backend interface
 * All backends (DwarFS, SquashFS, etc.) must implement this
 */
class IFilesystemBackend {
public:
    virtual ~IFilesystemBackend() = default;

    // Lifecycle
    virtual int init(const void* data, size_t size) = 0;
    virtual void cleanup() = 0;

    // Operations
    virtual int find_inode(const char* path, uint64_t* inode) = 0;
    virtual int stat(uint64_t inode, struct stat* st) = 0;
    virtual ssize_t read(uint64_t inode, void* buf, size_t size, off_t offset) = 0;
    virtual int readdir(uint64_t inode, /* ... */) = 0;
    virtual int readlink(uint64_t inode, char* buf, size_t size) = 0;

    // Metadata
    virtual const char* get_type() const = 0;  // "dwarfs", "squashfs", etc.
    virtual const char* get_version() const = 0;
};

// Backend registry
int register_backend(const char* type,
                     IFilesystemBackend* (*creator)(const void*, size_t));

}} // namespace tebako::vfs
```

---

### Phase 2: Create Language Adapter Layer (Week 2)

#### 2.1 Ruby Adapter

**Move to:** `src/adapters/ruby/`

```
src/adapters/ruby/
├── rb_adapter.h        # Ruby adapter interface
├── rb_adapter.cpp      # Implementation
├── rb_w32.h            # From tebako-io-rb-w32.h
└── rb_defines.h        # From tebako-defines.h (Ruby macros only)
```

**Interface example:**
```cpp
// include/tebako/adapters/ruby.h
namespace tebako {
namespace adapters {
namespace ruby {

// Initialize Ruby adapter
int rb_init(VFSHandle vfs);

// Ruby-specific functions
#ifdef RB_W32
struct direct* rb_readdir(DIR* dirp, void* enc);
int rb_stati128(const char* path, struct stati128* buf);
// ... other rb_w32_* functions
#endif

}}} // namespace
```

#### 2.2 Julia Adapter (NEW!)

**Create:** `src/adapters/julia/`

```
src/adapters/julia/
├── jl_adapter.h
├── jl_adapter.cpp
└── jl_defines.h
```

**Julia-specific intercepts:**
```cpp
// include/tebako/adapters/julia.h
namespace tebako {
namespace adapters {
namespace julia {

// Initialize Julia adapter
int jl_init(VFSHandle vfs);

// Julia file system functions to intercept
// Based on Julia's libuv integration
int jl_fs_open(const char* path, int flags, int mode);
ssize_t jl_fs_read(int fd, void* buf, size_t size);
int jl_fs_close(int fd);
int jl_fs_stat(const char* path, uv_stat_t* stat);
int jl_fs_scandir(const char* path, uv_fs_t* req);

// Map Julia's uv_* types to our API
struct uv_stat_t_adapter {
    // Convert between uv_stat_t and struct stat
};

}}} // namespace
```

**Julia Integration Points:**
- Julia uses libuv for I/O
- Intercept `uv_fs_*` functions
- Provide `uv_stat_t` compatibility
- Handle Julia's async I/O model

#### 2.3 Python Adapter (Future)

```cpp
// include/tebako/adapters/python.h
namespace tebako {
namespace adapters {
namespace python {

int py_init(VFSHandle vfs);

// Python C API intercepts
PyObject* py_file_open(const char* path, const char* mode);
Py_ssize_t py_file_read(PyObject* file, char* buf, Py_ssize_t size);
// ... etc
}}}
```

#### 2.4 Node.js Adapter (Future)

```cpp
// include/tebako/adapters/nodejs.h
namespace tebako {
namespace adapters {
namespace nodejs {

int node_init(VFSHandle vfs);

// Node.js N-API integrations
// Intercept fs module calls
napi_value node_fs_open(napi_env env, napi_callback_info info);
// ... etc
}}}
```

---

### Phase 3: Refactor Current Code (Week 3)

#### 3.1 Directory Structure

**Proposed reorganization:**

```
libtebako-fs/
├── include/
│   └── tebako/
│       ├── vfs/
│       │   ├── core.h           # Core VFS API (language-agnostic)
│       │   ├── backend.h        # Backend interface
│       │   ├── types.h          # Common types
│       │   └── config.h         # Configuration
│       ├── adapters/
│       │   ├── ruby.h           # Ruby adapter
│       │   ├── julia.h          # Julia adapter
│       │   ├── python.h         # Python adapter (future)
│       │   └── nodejs.h         # Node.js adapter (future)
│       └── backends/
│           ├── dwarfs.h         # DwarFS backend
│           ├── squashfs.h       # SquashFS (future)
│           └── zip.h            # ZIP (future)
├── src/
│   ├── vfs/                     # VFS core (language-agnostic)
│   │   ├── core.cpp
│   │   ├── mount_table.cpp
│   │   ├── fd_table.cpp
│   │   ├── path_resolver.cpp
│   │   └── backend_registry.cpp
│   ├── adapters/                # Language adapters
│   │   ├── ruby/
│   │   │   ├── rb_adapter.cpp
│   │   │   ├── rb_w32.cpp
│   │   │   └── rb_defines.cpp
│   │   └── julia/               # NEW!
│   │       ├── jl_adapter.cpp
│   │       ├── jl_uv.cpp        # libuv integration
│   │       └── jl_defines.cpp
│   └── backends/                # Filesystem backends
│       └── dwarfs/
│           ├── dwarfs_backend.cpp
│           └── dwarfs_wrapper.cpp
└── CMakeLists.txt
```

#### 3.2 Configuration Options

```cmake
# Language adapters (can enable multiple!)
option(TEBAKO_ADAPTER_RUBY "Build Ruby adapter" ON)
option(TEBAKO_ADAPTER_JULIA "Build Julia adapter" OFF)
option(TEBAKO_ADAPTER_PYTHON "Build Python adapter" OFF)
option(TEBAKO_ADAPTER_NODEJS "Build Node.js adapter" OFF)

# Filesystem backends (can enable multiple!)
option(TEBAKO_BACKEND_DWARFS "Build DwarFS backend" ON)
option(TEBAKO_BACKEND_SQUASHFS "Build SquashFS backend" OFF)
option(TEBAKO_BACKEND_ZIP "Build ZIP backend" OFF)
```

---

## Clean Separation of Concerns

### Layer 1: VFS Core (Language-Agnostic)

**Responsibility:** Generic virtual filesystem operations

**Files:**
- `include/tebako/vfs/core.h`
- `src/vfs/*.cpp`

**Depends on:** Nothing language-specific

**Provides:**
- Mount/unmount operations
- File handle management
- Path resolution
- Backend routing

**API Design Principle:**
```cpp
// GOOD: Language-agnostic
int vfs_open(const char* path, int flags);

// BAD: Ruby-specific
int vfs_rb_w32_open(const char* path, int flags);
```

### Layer 2: Language Adapters (Pluggable)

**Responsibility:** Language-specific integration

**Ruby Adapter:**
- `include/tebako/adapters/ruby.h`
- `src/adapters/ruby/*.cpp`
- Provides: `rb_w32_*` macros, `struct direct`, `stati128`
- Depends on: VFS Core API only

**Julia Adapter:**
- `include/tebako/adapters/julia.h`
- `src/adapters/julia/*.cpp`
- Provides: `jl_fs_*` functions, `uv_*` compatibility
- Depends on: VFS Core API only

**Key Principle:** Adapters are **isolated** - they don't know about each other

### Layer 3: Filesystem Backends (Pluggable)

**Responsibility:** Read from specific filesystem formats

**DwarFS Backend:**
- `include/tebako/backends/dwarfs.h`
- `src/backends/dwarfs/*.cpp`
- Implements: `IFilesystemBackend`
- Depends on: dwarfs library

**SquashFS Backend (Future):**
- `include/tebako/backends/squashfs.h`
- Implements: `IFilesystemBackend`
- Depends on: squashfs-tools library

---

## Stable Interfaces

### Interface 1: VFS Core API

**Contract:** Never changes (or only additions)

**Version:** Use semantic versioning
- 2.0.0 - Refactored multi-language architecture
- 2.x.x - Add features (backward compatible)
- 3.0.0 - Breaking changes (rare)

**Stability Promise:**
```cpp
// include/tebako/vfs/core.h
#define TEBAKO_VFS_VERSION_MAJOR 2
#define TEBAKO_VFS_VERSION_MINOR 0
#define TEBAKO_VFS_VERSION_PATCH 0

// API compatibility check
#if TEBAKO_VFS_VERSION_MAJOR != 2
#error "Incompatible VFS core version"
#endif
```

### Interface 2: Adapter Interface

**Contract:** Adapters implement standard hooks

**Hook points:**
```cpp
// include/tebako/vfs/adapter.h
namespace tebako {
namespace vfs {

struct AdapterHooks {
    // Lifecycle
    int (*init)(void* language_runtime);
    void (*cleanup)(void);

    // File operations
    void* (*open_wrapper)(const char* path, int flags);
    ssize_t (*read_wrapper)(void* handle, void* buf, size_t size);
    int (*close_wrapper)(void* handle);

    // Type conversions
    void (*stat_to_language)(const struct stat* vfs_stat, void* language_stat);
    void (*dirent_to_language)(const struct dirent* vfs_dirent, void* language_dirent);
};

// Register adapter
int register_adapter(const char* name, const AdapterHooks* hooks);

}} // namespace
```

### Interface 3: Backend Interface

**Contract:** All backends implement `IFilesystemBackend`

```cpp
// include/tebako/vfs/backend.h
class IFilesystemBackend {
public:
    virtual ~IFilesystemBackend() = default;

    // Required operations
    virtual int init(const void* data, size_t size) = 0;
    virtual int find(const char* path, uint64_t* inode) = 0;
    virtual ssize_t read(uint64_t inode, void* buf, size_t size, off_t off) = 0;
    virtual int stat(uint64_t inode, struct stat* st) = 0;
    virtual int readdir(uint64_t inode, /* ... */) = 0;

    // Metadata
    virtual const char* type() const = 0;
    virtual uint32_t version() const = 0;
};
```

---

## Julia Support Implementation

### Step 1: Understand Julia's Filesystem Interface

**Julia uses libuv for I/O:**
```julia
# Julia code
file = open("/path/to/file")
data = read(file, String)
close(file)
```

**Translates to libuv calls:**
```c
// C level
uv_fs_t req;
uv_fs_open(loop, &req, path, flags, mode, callback);
uv_fs_read(loop, &req, file, &buf, 1, offset, callback);
uv_fs_close(loop, &req, file, callback);
```

### Step 2: Create Julia Adapter

**File:** `src/adapters/julia/jl_adapter.cpp`

```cpp
#include <tebako/vfs/core.h>
#include <tebako/adapters/julia.h>
#include <uv.h>

namespace tebako {
namespace adapters {
namespace julia {

static VFSHandle g_vfs = nullptr;

int jl_init(VFSHandle vfs) {
    g_vfs = vfs;
    return 0;
}

// Intercept Julia's libuv filesystem calls
int jl_fs_open(uv_loop_t* loop, uv_fs_t* req,
               const char* path, int flags, int mode,
               uv_fs_cb cb) {

    // Check if path is in Tebako VFS
    if (is_tebako_path(path)) {
        VFSHandle handle = vfs_open(path, flags);
        if (handle) {
            req->result = (ssize_t)handle;
            if (cb) cb(req);
            return 0;
        }
        return -1;
    }

    // Not in VFS, use real libuv
    return uv_fs_open(loop, req, path, flags, mode, cb);
}

// Similar for read, write, stat, etc.

}}} // namespace
```

### Step 3: Julia Integration Macros

**File:** `include/tebako/adapters/julia/defines.h`

```cpp
#ifdef TEBAKO_JULIA_ADAPTER

// Replace Julia's filesystem calls
#define uv_fs_open(...) tebako::adapters::julia::jl_fs_open(__VA_ARGS__)
#define uv_fs_read(...) tebako::adapters::julia::jl_fs_read(__VA_ARGS__)
#define uv_fs_close(...) tebako::adapters::julia::jl_fs_close(__VA_ARGS__)
#define uv_fs_stat(...) tebako::adapters::julia::jl_fs_stat(__VA_ARGS__)
#define uv_fs_scandir(...) tebako::adapters::julia::jl_fs_scandir(__VA_ARGS__)

#endif // TEBAKO_JULIA_ADAPTER
```

### Step 4: Julia Package Integration

**In Tebako-Julia build:**
```cmake
# Build libtebako-fs with Julia adapter
add_subdirectory(libtebako-fs)

# Configure for Julia
target_compile_definitions(tebako-fs PUBLIC
    TEBAKO_ADAPTER_JULIA=1
    TEBAKO_MOUNT_POINT="/tebako_vfs"
)

# Link Julia runtime with our adapter
target_link_libraries(julia-runtime
    tebako-fs
    tebako-julia-adapter
)
```

---

## Implementation Timeline

### Week 1: Extract VFS Core
- Day 1-2: Define core API
- Day 3-4: Refactor existing code into VFS core
- Day 5: Unit tests for VFS core

### Week 2: Create Adapter Layer
- Day 1-2: Extract Ruby adapter from current code
- Day 3: Define adapter interface
- Day 4-5: Test Ruby adapter works identically

### Week 3: Julia Adapter
- Day 1: Research Julia's libuv integration
- Day 2-3: Implement Julia adapter
- Day 4: Julia integration tests
- Day 5: Documentation

### Week 4: Integration & Testing
- Day 1-2: Integration testing
- Day 3: Performance testing
- Day 4: Documentation updates
- Day 5: Release preparation

**Total: 4 weeks (160 hours)**

---

## Benefits

### For Ruby (Current Users)
- ✅ No breaking changes (compatibility layer)
- ✅ Same performance
- ✅ Cleaner architecture

### For Julia (New Users)
- ✅ Full Tebako packaging support
- ✅ Works with Julia's libuv model
- ✅ Same VFS features as Ruby

### For Future Languages
- ✅ Clear template (copy Julia adapter)
- ✅ Minimal effort (just adapter layer)
- ✅ Shared VFS benefits

### For Maintenance
- ✅ Clean separation
- ✅ Language changes isolated
- ✅ VFS core is stable
- ✅ Easy to add backends

---

## Example: Building for Julia

```bash
# Build libtebako-fs with Julia adapter
cmake \
  -DTEBAKO_ADAPTER_JULIA=ON \
  -DTEBAKO_BACKEND_DWARFS=ON \
  -DTEBAKO_BUILD=ON \
  -DDWARFS_WITH_CEREAL=ON \
  -DDWARFS_WITH_BITSERY=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  ..

make tebako-fs tebako-julia-adapter
```

**In Julia packaging:**
```julia
# Julia code
using TebakoFS

# TebakoFS.jl wraps our C adapter
file = open("/tebako_vfs/mypackage/data.json")
data = read(file, String)
close(file)
```

---

## Migration Path (Backward Compatibility)

### Version 2.0: Refactored Architecture
- Extract VFS core
- Create Ruby adapter
- Provide compatibility layer
- **Ruby users see no difference**

### Version 2.1: Add Julia Support
- Add Julia adapter
- No changes to VFS core
- No impact on Ruby adapter

### Version 2.2+: Add More Languages
- Python adapter
- Node.js adapter
- etc.

### Version 3.0: Remove Compatibility (Future)
- Clean up old header names
- Remove deprecated APIs
- Fully modular architecture

---

## Example Usage: Multi-Language Tebako

**Imagine:** Tebako package with Ruby + Julia

```cpp
// Initialize VFS
tebako::vfs::vfs_init();

// Mount DwarFS
auto mount = tebako::vfs::vfs_mount(
    "/tebako_vfs",
    dwarfs_data,
    dwarfs_size,
    "dwarfs"
);

// Initialize Ruby adapter
tebako::adapters::ruby::rb_init();

// Initialize Julia adapter
tebako::adapters::julia::jl_init();

// Now BOTH Ruby and Julia can access /tebako_vfs/!

// Ruby code:
File.read("/tebako_vfs/ruby/lib.rb")

// Julia code:
read("/tebako_vfs/julia/module.jl", String)
```

---

## Action Plan

### Immediate (This Project)
1. Document current architecture
2. Complete folly/thrift removal
3. Test and validate

### Next (Refactoring Project)
1. Design stable interfaces
2. Extract VFS core
3. Create Ruby adapter
4. Ensure backward compatibility

### Future (Expansion Projects)
1. Julia adapter
2. Python adapter
3. Additional backends (SquashFS, ZIP)

**Estimated Total Effort:** 6-8 weeks for complete multi-language support

---

## Success Criteria

**For Julia Support:**
- ✅ Julia can read from Tebako VFS
- ✅ Julia package loading works from VFS
- ✅ No impact on Ruby support
- ✅ Clean, documented interfaces
- ✅ <5% performance overhead

**For Architecture:**
- ✅ VFS core has zero language-specific code
- ✅ Adapters are independent modules
- ✅ Backends are plug-and-play
- ✅ Comprehensive documentation
- ✅ Working examples for each language

---

## References

- [`docs/PURPOSE_AND_SCOPE.md`](docs/PURPOSE_AND_SCOPE.md:1) - Current architecture
- [`docs/TEBAKO_VFS_ARCHITECTURE.md`](docs/TEBAKO_VFS_ARCHITECTURE.md:1) - VFS design
- [`docs/RENAMING_PROPOSAL.md`](docs/RENAMING_PROPOSAL.md:1) - Naming strategy