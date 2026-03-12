# Library Renaming Proposal

## Proposed Name: `libtebako-fs`

**Current:** `libdwarfs-wr` (misleading - implies DwarFS-only wrapper)
**Proposed:** `libtebako-fs` (accurate - Tebako filesystem layer)

## Rationale

The library is **Tebako's VFS layer** supporting multiple backends, not just a DwarFS wrapper:
- 70% new functionality (POSIX abstraction, mount tables, FD management)
- Supports multiple filesystem instances simultaneously
- Designed for extensibility (future: SquashFS, ZIP, TAR backends)

## Name Options Evaluated

| Name | Pros | Cons | Score |
|------|------|------|-------|
| **libtebako-fs** | Clear, short, accurate | Migration cost | 9/10 ⭐ |
| libtebako-vfs | Explicit "virtual FS" | Slightly longer | 8/10 |
| tebako-filesystem | Most explicit | Long, no lib prefix | 7/10 |
| libdwarfs-wr | Established | Misleading | 3/10 |

## Recommendation

**Rename to `libtebako-fs`** because:
- Accurately reflects purpose
- Standard `lib{project}-{function}` convention
- Short and memorable
- Enables future: `libtebako-net`, `libtebako-io`, etc.

## jemalloc Clarification

**Question:** Do we need jemalloc?

**Answer:** YES, but not for folly (folly is gone). jemalloc is required for:
- **Ruby memory management** when statically embedded in Tebako
- Memory allocation consistency across embedded interpreters
- Performance optimization for dynamic languages

**Update documentation to clarify:**
```markdown
### jemalloc Requirement

jemalloc is required when embedding Ruby or other dynamic languages
that have specific memory management requirements. It provides:
- Consistent memory allocation across interpreter boundaries
- GC integration support
- Performance optimization
```

## Migration Plan

### Phase 1: Compatibility Layer (Immediate)
```cpp
// include/tebako-io.h (old name - deprecated)
#pragma once
#warning "Use <tebako/fs.h> instead of <tebako-io.h>"
#include <tebako/fs.h>
```

### Phase 2: Gradual Migration (6-12 months)
- New projects use `libtebako-fs`
- Old projects continue with compatibility headers
- Deprecation warnings guide migration

### Phase 3: Complete Transition (After deprecation period)
- Remove compatibility layer
- Version 3.0.0
- Clean break

## File Reorganization

```
include/
├── tebako/              # New organized structure
│   └── fs/
│       ├── core.h       # Main API
│       ├── mount.h      # Mount management
│       ├── descriptor.h # FD management
│       └── backends/
│           └── dwarfs.h # DwarFS backend
└── tebako-*.h          # Old headers (deprecated, compatibility)
```

## Approval Required

**Decision needed on:**
1. Approve rename to `libtebako-fs`? (Recommended: YES)
2. Timeline for migration? (Recommended: Start with v2.0.0)
3. Deprecation period? (Recommended: 6-12 months)

**Next steps if approved:**
1. Create compatibility headers
2. Update CMakeLists.txt project name
3. Reorganize header structure
4. Update all documentation
5. Release as v2.0.0