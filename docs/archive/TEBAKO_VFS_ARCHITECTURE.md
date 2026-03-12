# Tebako Virtual Filesystem Architecture

## Executive Summary

**libdwarfs-wr is Tebako's pluggable virtual filesystem layer**, not merely a DwarFS wrapper. It provides a POSIX-compatible VFS abstraction that can support multiple filesystem backends simultaneously.

## Key Architectural Insight

The naming "libdwarfs-wr" is **misleading** - it suggests a simple DwarFS wrapper, but the architecture reveals it's actually:

```
Tebako VFS Layer (libdwarfs-wr)
├── POSIX API (open, read, stat, etc.)
├── FD table management
├── Mount table (supports multiple mount points)
├── Filesystem backend abstraction
│   ├── DwarFS backend (current)
│   ├── SquashFS backend (potential)
│   ├── ZIP backend (potential)
│   └── TAR backend (potential)
└── Path resolution and routing
```

## Evidence from Code

### 1. Mount Table Architecture

From [`include/tebako-mount-table.h`](include/tebako-mount-table.h:34-36):
```cpp
typedef std::pair<uint32_t, std::string> tebako_mount_point;
typedef std::variant<std::string, uint32_t> tebako_mount_target;
typedef std::map<tebako_mount_point, tebako_mount_target> tebako_mount_table;
```

This shows support for:
- **Multiple mount points** (map structure)
- **Multiple targets** (variant: path string OR filesystem index)
- **Hierarchical mounting** (mount one FS at a path in another FS)

### 2. Memory Filesystem Table

From [`include/tebako-memfs-table.h`](include/tebako-memfs-table.h:39):
```cpp
typedef std::map<uint32_t, std::shared_ptr<memfs>> tebako_memfs_table;
```

This shows:
- **Multiple filesystem instances** supported (indexed by uint32_t)
- **Dynamic filesystem registration**
- Not just one DwarFS - can have many

### 3. Filesystem Abstraction in Practice

From [`src/tebako-memfs.cpp`](src/tebako-memfs.cpp:200-210):
```cpp
else if (std::holds_alternative<uint32_t>(*mount_point)) {
    uint32_t index = std::get<uint32_t>(*mount_point);
    auto next_memfs = tebako::sync_tebako_memfs_table::get_tebako_memfs_table().get(index);
    if (next_memfs != nullptr) {
        auto next_inode = next_memfs->get_root_inode();
        // Switch to different filesystem
        return next_memfs->find_inode(next_inode, next_path, ...);
    }
}
```

This code **switches between different filesystem instances dynamically** during path resolution!

### 4. FD Table Abstraction

From [`include/tebako-fd.h`](include/tebako-fd.h:56):
```cpp
typedef std::map<int, std::shared_ptr<tebako_fd>> tebako_fdtable;
```

The FD table is **filesystem-agnostic** - it can hold file descriptors from any backend.

## What This Means

### Current State

```
Application
    ↓ (POSIX API: open, read, stat, etc.)
Tebako VFS Layer (libdwarfs-wr)
    ├── Mount Table: /app/data → DwarFS #0
    ├── Mount Table: /app/assets → DwarFS #1
    └── Mount Table: /app/cache → (potential: different FS)
        ↓
Backend: DwarFS Filesystem
    └── Uses: cereal/bitsery (header-only)
```

### Future Potential

```
Application
    ↓
Tebako VFS Layer
    ├── /app/data → DwarFS backend
    ├── /app/assets → SquashFS backend
    ├── /tmp/extract → ZIP backend
    └── /backup → TAR backend
```

## Design Patterns

### 1. Strategy Pattern
- Filesystem backend is pluggable
- Current: only DwarFS implemented
- Future: add more backends (SquashFS, ZIP, TAR, etc.)

### 2. Facade Pattern
- Provides unified POSIX interface
- Hides complexity of multiple backends
- Applications see standard file operations

### 3. Registry Pattern
- `tebako_memfs_table` - Registry of filesystem instances
- `tebako_mount_table` - Registry of mount points
- `tebako_fdtable` - Registry of open file descriptors

## Architecture Layers

### Layer 1: Application Interface (C API)
**Files:** `include/tebako-io.h`, `include/tebako-dirent.h`
```c
int tebako_open(int n, const char* path, int flags, ...);
ssize_t tebako_read(int fd, void* buf, size_t count);
DIR* tebako_opendir(const char* path);
```

### Layer 2: VFS Core (C++ Implementation)
**Files:** Most of `src/` directory
- Path resolution and routing
- Mount point management
- FD table management
- Cross-filesystem operations

### Layer 3: Backend Abstraction
**Files:** `include/tebako-memfs.h`, `src/tebako-memfs.cpp`
```cpp
class memfs {
    // Backend interface that could be abstracted further
    int find_inode(...);
    int readlink(...);
    ssize_t inode_read(...);
};
```

### Layer 4: Filesystem Implementation
**Currently:** DwarFS only
**Future:** Could have `class squashfs`, `class zipfs`, etc.

## Implications

### 1. Naming Should Reflect Reality

**Current:** `libdwarfs-wr` (implies DwarFS-specific wrapper)
**Reality:** Tebako VFS layer (supports multiple backends)
**Better names:**
- `libtebako-vfs`
- `libtebako-fs`
-`tebako-filesystem`

**Recommendation:** Consider renaming in major version bump, or at least clarify in documentation

### 2. Architecture is Well-Designed for Extension

The code already supports:
- ✅ Multiple filesystem instances
- ✅ Hierarchical mounting
- ✅ Dynamic filesystem switching
- ✅ Backend abstraction (though not formally defined as interface)

**To add a new filesystem backend:**
1. Create `class newfs` similar to `class memfs`
2. Implement backend operations (find_inode, read, etc.)
3. Register in `tebako_memfs_table` (despite name, it's FS-agnostic)
4. Mount at desired path

### 3. "memfs" naming is also misleading

**Current:** `tebako_memfs_table`, `class memfs`
**Reality:** Filesystem table (not memory-specific)
**Better:** `tebako_fs_table`, `class filesystem_backend`

But renaming would be disruptive, so document clearly instead.

## Recommendations

### Short Term: Documentation

1. **Update README.md to clarify:**
   ```markdown
   ## libdwarfs-wr: Tebako Virtual Filesystem Layer

   libdwarfs-wr is Tebako's pluggable VFS layer providing:
   - POSIX-compatible file operations
   - Support for multiple filesystem backends
   - Currently implements DwarFS backend
   - Designed for future SquashFS, ZIP, TAR support
   ```

2. **Add to ARCHITECTURE.md:**
   - Explain VFS design
   - Document extensibility
   - Show how to add new backends

### Medium Term: API Formalization

1. **Define filesystem backend interface:**
   ```cpp
   class tebako_filesystem {
   public:
       virtual ~tebako_filesystem() = default;
       virtual int find_inode(...) = 0;
       virtual int read(...) = 0;
       virtual int readlink(...) = 0;
       // ... etc
   };

   class dwarfs_backend : public tebako_filesystem {
       // DwarFS-specific implementation
   };
   ```

2. **Rename for clarity:**
   - `tebako_memfs_table` → `tebako_filesystem_table`
   - But preserve aliases for compatibility

### Long Term: Additional Backends

Potential filesystem backends to support:
1. **SquashFS** - Popular in embedded systems
2. **ZIP** - Universal archive format
3. **TAR** - Simple, widespread
4. **ROMFS** - Minimal overhead
5. **Host folder** - Direct passthrough for development

## Conclusion

**libdwarfs-wr is fundamentally misnamed** - it's actually:

> **Tebako Virtual Filesystem Layer**
>
> A pluggable VFS providing POSIX-compatible file operations over multiple filesystem backends, with DwarFS as the primary (currently only) implementation.

The architecture is well-designed for extensibility. The naming is historical but misleading. Documentation should clarify the true scope and purpose.

## References

- [`include/tebako-mount-table.h`](include/tebako-mount-table.h:34) - Multi-mount support
- [`include/tebako-memfs-table.h`](include/tebako-memfs-table.h:39) - Filesystem registry
- [`src/tebako-memfs.cpp`](src/tebako-memfs.cpp:200) - Cross-filesystem path resolution
- [`docs/FINAL_SOLUTION.md`](docs/FINAL_SOLUTION.md:458) - Architecture diagrams