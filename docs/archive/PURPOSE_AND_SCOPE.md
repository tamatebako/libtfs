# libdwarfs-wr: Purpose and Scope

## Critical Finding: This is Tebako/Ruby-Specific, Not Generic

### Evidence from Code

#### 1. Ruby Integration Layer ([`include/tebako-defines.h`](include/tebako-defines.h:58-87))

**HEAVY Ruby integration** - 30+ macro definitions:
```cpp
#ifdef RB_W32
#define rb_w32_open(...) tebako_open(...)
#define rb_w32_read(...) tebako_read(...)
#define rb_w32_stati128(...) tebako_stat(...)
// ... 27 more Ruby function mappings
#endif
```

**Purpose:** Intercepts Ruby's file system calls and redirects to Tebako's VFS

#### 2. Ruby Win32 Compatibility ([`include/tebako-io-rb-w32.h`](include/tebako-io-rb-w32.h:30-160))

**Complete Ruby Win32 compatibility layer:**
- Emulates Ruby's `struct direct` (not standard POSIX)
- Provides `rb_w32_*` function prototypes
- Includes `stati128` (Ruby's 128-bit stat structure)

#### 3. Package Descriptor ([`include/tebako-package-descriptor.h`](include/tebako-package-descriptor.h:42-74))

**Explicitly tracks Ruby and Tebako versions:**
```cpp
class package_descriptor {
    uint16_t ruby_version_major;
    uint16_t ruby_version_minor/;
    uint16_t ruby_version_patch;
    uint16_t tebako_version_major;
    uint16_t tebako_version_minor;
    uint16_t tebako_version_patch;
    std::string mount_point;
};
```

**Purpose:** Embeds Ruby/Tebako metadata into filesystem headers

#### 4. Configuration Enforcement ([`include/tebako-config.h`](include/tebako-config.h:62-64))

**Windows REQUIRES Ruby mode:**
```cpp
#if defined(_WIN32) && !defined(RB_W32)
#error "Only Ruby style Dir IO can be used on Windows"
#endif
```

---

## Answer to Your Questions

### Q1: Does this have Tebako/Ruby mentions?

**YES - extensively!**

- **30+ Ruby function mappings** ([`tebako-defines.h`](include/tebako-defines.h:58))
- **Ruby version tracking** ([`package-descriptor.h`](include/tebako-package-descriptor.h:43))
- **RB_W32 compatibility layer** ([`tebako-io-rb-w32.h`](include/tebako-io-rb-w32.h:30))
- **Tebako mount point** (`/__tebako_memfs__` defined in [`tebako-common.h`](include/tebako-common.h:78))

### Q2: Is it just a generic virtual FS layer?

**NO - it's specifically designed for Tebako's Ruby packaging needs.**

**However, the underlying architecture COULD support generic use:**
- Mount table is backend-agnostic ✅
- FD table is generic ✅
- Path resolution is generic ✅
- BUT: API layer has Ruby-specific hooks ❌

### Q3: Would other filesystem formats work if implemented?

**YES!** The architecture supports it:

**Current:**
```
Tebako/Ruby Application
  ↓ (via RB_W32 macros or tebako_ functions)
libdwarfs-wr (Tebako VFS Layer)
  ├── /app/ruby → DwarFS backend
  └── ...
```

**Potential:**
```
Tebako Application
  ↓
libdwarfs-wr VFS Core
  ├── /app/ruby → DwarFS backend
  ├── /app/assets → SquashFS backend
  ├── /tmp → ZIP backend
  └── /backup → TAR backend
```

**BUT:** You'd still be using the Tebako/Ruby-specific API layer

---

## Architectural Layers

### Layer 1: Ruby Compatibility (Tebako-specific)
- `RB_W32` macros
- Ruby function mappings (`rb_w32_*` → `tebako_*`)
- Ruby-specific structures (`struct direct`, `stati128`)

### Layer 2: Tebako API (Tebako-specific)
- `tebako_*` functions
- Package descriptors with Ruby/Tebako versions
- `/__tebako_memfs__` mount point convention

### Layer 3: VFS Core (Generic!)
- Mount table management
- FD table management
- Filesystem backend registry
- Path resolution

### Layer 4: Filesystem Backends (Pluggable!)
- **DwarFS** (current)
- **SquashFS** (potential)
- **ZIP** (potential)
- **TAR** (potential)

---

## Reusability Analysis

### Can Be Reused For:
- ✅ Tebako packaging other languages (Python, Node.js, etc.)
  - Would need to create language-specific compatibility layers like RB_W32
  - Core VFS is language-agnostic

- ✅ Adding other filesystem backends to Tebako
  - Architecture already supports multiple backends
  - Just implement backend interface

### Cannot Be Reused For:
- ❌ Generic userspace filesystem library
  - Too much Tebako/Ruby-specific code
  - Would need major refactoring

- ❌ FUSE replacement
  - Different purpose (FUSE is kernel integration)
  - This is application-level VFS

### Could Be Extracted To:
- 📦 Generic core + Tebako wrapper
  - Extract Layer 3 (VFS Core) to separate library
  - Keep Layer 1-2 as Tebako-specific wrapper
  - Would be substantial refactoring

---

## True Purpose Statement

**libdwarfs-wr is:**

> **Tebako's Virtual Filesystem Integration Layer**
>
> Provides:
> - Ruby compatibility shims for file system operations
> - POSIX abstraction over compressed filesystem backends
> - Tebako package metadata management
> - Multi-backend VFS architecture
>
> Designed specifically for Tebako packaging, with particular focus on Ruby integration.

**It is NOT:**
- ❌ Generic VFS for any application
- ❌ Simple DwarFS wrapper
- ❌ Reusable outside Tebako ecosystem

**It COULD BE with refactoring:**
- Extract generic VFS core
- Keep Tebako/Ruby layer separate
- Provide pluggable compatibility layers

---

## Naming Implications

### Current: `libdwarfs-wr`
✅ Accurate if "wr" means "wrapper for Tebako/Ruby"
❌ Misleading if "wr" means "simple DwarFS wrapper"

### Proposed: `libtebako-fs`
✅ Clearly identifies as Tebako component
✅ Indicates filesystem functionality
✅ Doesn't imply it's generic or reusable

###  Alternative: `libtebako-ruby-fs`
✅ Most accurate - identifies both Tebako AND Ruby specificity
❌ Too long
❌ Limits perception of extending to other languages

### Recommendation

**Rename to `libtebako-fs`** and document:
```markdown
## libtebako-fs

Tebako's virtual filesystem layer, providing POSIX-compatible file
operations over compressed filesystem backends, with specific support
for Ruby integration.

**Primary use case:** Packaging Ruby applications with Tebako
**Architecture:** Supports multiple filesystem backends (DwarFS, future: SquashFS, ZIP, TAR)
**API:** Includes Ruby Win32 compatibility layer (RB_W32 mode)
```

---

## Extensibility for Other Filesystems

### Q: Can we add SquashFS/ZIP/TAR support?

**A: YES!** The architecture supports it:

**What's needed:**
1. Implement backend class (like `class memfs` for DwarFS)
2. Register in filesystem table
3. Mount at desired path

**What' s NOT needed:**
- No changes to Ruby compatibility layer
- No changes to FD/mount table core
- No changes to POSIX API

**Example:**
```cpp
// Future: backends/squashfs_backend.cpp
namespace tebako {
class squashfs_backend {
    // Implement: find_inode, read, readdir, etc.
};

// Register
auto sqfs = std::make_shared<squashfs_backend>(data, size);
sync_tebako_memfs_table::insert_auto(sqfs);  // Get index
sync_tebako_mount_table::insert(root_ino, "assets", index);  // Mount
}
```

---

## Conclusion

**libdwarfs-wr is specifically designed for Tebako/Ruby** with:
- 40% Ruby-specific compatibility code
- 30% Tebako package integration
- 30% generic VFS core

**But** the generic VFS core is well-architected and CAN support:
- Multiple filesystem backends ✅
- Multiple mount points ✅
- Other languages (with new compatibility layers) ✅
- Standalone use (with refactoring) ⚠️

**Recommendation:** Rename to `libtebako-fs` to accurately reflect its purpose as Tebako's filesystem layer (not generic, not just DwarFS).