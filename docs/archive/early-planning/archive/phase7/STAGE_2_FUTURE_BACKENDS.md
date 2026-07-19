# Future Backend Support Analysis

**Date**: 2025-12-22
**Status**: Architectural Planning
**Purpose**: Evaluate additional archive format backends for libtfs

---

## Overview

The static factory architecture supports easy addition of new backends. This document analyzes potential archive formats for future implementation, ranked by priority for Tebako's use case (Ruby application packaging).

---

## Critical Requirement: Random Access Architecture

### Why Random Access Matters

libtfs provides a **virtual filesystem interface** where applications expect to:
- Open any file directly without scanning the entire archive
- Seek within files without decompressing from the beginning
- List directory contents efficiently
- Query file metadata instantly

### Random Access Requirements

✅ **Supported formats MUST have**:
1. **Central index/directory** - File locations stored in metadata
2. **Direct file access** - Can jump to file data by offset
3. **Independent file compression** - Each file compressed separately (or uncompressed)
4. **Metadata table** - File sizes, locations, timestamps in index

❌ **Incompatible formats** (Sequential):
- TAR (no index, must scan sequentially)
- CPIO (no index, sequential format)
- tar.gz, tar.bz2, tar.xz (solid compression, must decompress from start)
- Any format with solid compression blocks

### Architecture Pattern

```
Application: "Open /app/config.json"
           ↓
Backend:   1. Lookup in index → File at offset X, size Y
           2. Seek to offset X
           3. Read/decompress Y bytes
           4. Return file handle
```

**NOT this**:
```
Application: "Open /app/config.json"
           ↓
Backend:   1. Scan archive from beginning ❌
           2. Decompress everything before target ❌
           3. Find target file
           4. Return data
```

---

## Priority 1: High-Value Random-Access Backends

### 1. SquashFS

**Priority**: ⭐⭐⭐⭐⭐ **HIGHEST** (Now that TAR is excluded)

**Why**: Excellent compression, read-only, Linux standard, **has inode table and directory index**

**Random Access**: ✅ **YES**
- Inode table with file locations
- Directory structure with offsets
- Per-file compression blocks
- Can jump directly to any file

**Formats**:
- `.sqsh` / `.squashfs`
- `.sfs`
- `.appimage` (AppImage contains SquashFS)

**Library**: **squashfs-tools-ng**
- LGPL license (safe for static linking)
- Cross-platform support
- Read-only API available

**Magic Numbers**:
```cpp
// SquashFS magic (little-endian)
offset 0: 68 73 71 73  // "hsqs"

// SquashFS magic (big-endian)
offset 0: 73 71 73 68  // "sqsh"
```

**Index Structure**:
```
Superblock → Inode Table → Directory Table → Fragment Table
    ↓
Can directly lookup any file without scanning
```

**Implementation Complexity**: Medium-High
- Need to parse superblock and inode table
- Block-based compression handling
- Directory tree navigation

**Effort Estimate**: 5-7 days

---

### 2. 7z (7-Zip)

**Priority**: ⭐⭐⭐⭐ **HIGH**

**Why**: Excellent compression ratio, **has header with file index**

**Random Access**: ✅ **YES**
- Header contains file index
- Archive header lists all files with offsets
- Can seek directly to any file
- Per-file compression (not solid by default)

**Formats**:
- `.7z`

**Library**: **lzma-sdk** or **p7zip**
- Public domain (lzma-sdk) ✅
- LGPL (p7zip) ✅
- Cross-platform ✅

**Magic Numbers**:
```cpp
// 7z signature
offset 0: 37 7A BC AF 27 1C  // "7z¼¯'."
```

**Index Structure**:
```
Signature → Header → Files Header → Packed Streams
    ↓
Header contains offset to each file's data
```

**Note**: Must avoid "solid" compression mode which defeats random access

**Implementation Complexity**: Medium-High
- Complex format with multiple compression methods
- Need to parse header structure
- Handle various compression algorithms

**Effort Estimate**: 4-5 days

---

### 3. ISO 9660 (CD/DVD Images)

**Priority**: ⭐⭐⭐⭐ **HIGH**

**Why**: Simple, **has directory records**, no compression, **perfect random access**

**Random Access**: ✅ **YES** (Best random access of all formats)
- Primary Volume Descriptor contains directory structure
- Each directory entry has direct pointer to file data
- No compression = instant access
- Designed for optical media random access

**Formats**:
- `.iso`
- `.img` (sometimes)

**Library**:
- **Custom implementation** (format is simple, can avoid GPL libiso9660)
- Or: libiso9660 (part of libcdio) if GPL acceptable

**Magic Numbers**:
```cpp
// ISO 9660 signature
offset 32768: "CD001"  // Primary Volume Descriptor
offset 32776: 0x01     // VD type (Primary)
```

**Index Structure**:
```
Primary Volume Descriptor (offset 32768)
    ↓
Root Directory Record
    ↓
File/Directory entries with data offsets
```

**Implementation Complexity**: Low-Medium
- Simple format specification
- No compression to handle
- Can implement from scratch easily

**Effort Estimate**: 2-3 days

---

## Priority 2: Specialized Random-Access Formats

### 4. AppImage

**Priority**: ⭐⭐⭐ **MEDIUM**

**Why**: Linux application packaging standard, **backed by SquashFS (has index)**

**Random Access**: ✅ **YES** (via SquashFS)
- AppImage = ELF header + SquashFS filesystem
- SquashFS provides the random access
- Parse ELF to find SquashFS offset
- Use SquashFS backend for actual filesystem

**Format**:
- `.appimage`
- Structure: ELF executable + embedded SquashFS

**Implementation**:
- Requires SquashFS backend first
- Add ELF header parser
- Mount SquashFS portion at discovered offset

**Magic Numbers**:
```cpp
// ELF header
offset 0: 7F 45 4C 46  // ELF magic

// Scan for SquashFS magic after ELF sections
// at SquashFS offset: 68 73 71 73 or 73 71 73 68
```

**Implementation Complexity**: Low (after SquashFS)
- Simple ELF parsing
- Reuse SquashFS backend
- Just need offset detection

**Effort Estimate**: 1-2 days (after SquashFS)

---

## ❌ Excluded: Sequential Formats (No Random Access)

### TAR (All Variants)

**Random Access**: ❌ **NO**

**Why Excluded**:
- Sequential format with no central index
- Must scan from beginning to find files
- tar.gz/tar.bz2/tar.xz: Solid compression requiring decompression from start
- Incompatible with VFS random access requirements

**Problem Example**:
```
To access file at position 10GB in .tar.gz:
1. Start decompression from byte 0 ❌
2. Decompress and skip 10GB of data ❌
3. Finally reach target file ❌
4. Performance: O(archive_size) per file access ❌
```

**Note**: While Ruby gems are `.tar.gz`, they must be **extracted** before use, not mounted as VFS.

---

### CPIO

**Random Access**: ❌ **NO**

**Why Excluded**:
- Sequential format like TAR
- No index or directory table
- Must scan entire archive to find files
- Same performance issues as TAR

---

## Implementation Strategy (Revised)

### Phase 1 (Stage 2): Core Backends
1. ✅ DwarFS (already implemented)
2. 🚧 ZIP (Week 2)

### Phase 2 (Stage 3): High-Performance Random-Access
3. **SquashFS** (excellent compression + random access)
   - Priority for Linux deployment
   - AppImage support

### Phase 3 (Stage 4): Additional Formats
4. **7z** (best compression ratio with random access)
5. **ISO 9660** (simple, perfect random access, no compression)

### Phase 4 (Future): Specialized
6. **AppImage** (leverages SquashFS)

---

## Library Dependencies Summary (Updated)

| Backend | Random Access | Library | License | Static Link | Priority |
|---------|---------------|---------|---------|-------------|----------|
| DwarFS | ✅ Yes | libdwarfs | Apache-2.0 | ✅ | Done |
| ZIP | ✅ Yes | libzip | BSD-3 | ✅ | Stage 2 |
| SquashFS | ✅ Yes | squashfs-tools-ng | LGPL | ✅ | Stage 3 |
| 7z | ✅ Yes | lzma-sdk | Public Domain | ✅ | Stage 4 |
| ISO 9660 | ✅ Yes | Custom/libiso9660 | N/A or GPL | ✅ | Stage 4 |
| AppImage | ✅ Yes (via SquashFS) | - | - | ✅ | Future |
| ~~TAR~~ | ❌ No | ~~libarchive~~ | - | - | ❌ Excluded |
| ~~CPIO~~ | ❌ No | - | - | - | ❌ Excluded |

---

## Backend Factory Extension Example

Adding TAR backend is straightforward:

```cpp
// In backend_factory.h
class BackendFactory {
public:
    // Existing methods
    static std::unique_ptr<FileSystem> create_dwarfs();
    static std::unique_ptr<FileSystem> create_zip();

    // Add new method
    static std::unique_ptr<FileSystem> create_tar();

    // Add detection
    static bool is_tar_format(const std::string& path);
};

// In backend_factory.cpp
std::unique_ptr<FileSystem> BackendFactory::create_from_file(
    const std::string& archive_path) {

    // Existing detection
    if (is_dwarfs_format(archive_path)) return create_dwarfs();
    if (is_zip_format(archive_path)) return create_zip();

    // Add new detection
    if (is_tar_format(archive_path)) return create_tar();

    // Extension fallback
    if (has_extension(archive_path, ".tar") ||
        has_extension(archive_path, ".tar.gz") ||
        has_extension(archive_path, ".tgz") ||
        has_extension(archive_path, ".tar.bz2") ||
        has_extension(archive_path, ".tar.xz")) {
        return create_tar();
    }

    // ... rest of detection
}
```

---

## Recommendation (Revised for Random Access)

### Immediate Priority (Stage 2-3)
1. **ZIP** ✅ - Universal format, central directory index, perfect random access
2. **SquashFS** ✅ - Linux standard, inode table, excellent for AppImage

### Near-term (Stage 4)
3. **7z** ✅ - Best compression with header index
4. **ISO 9660** ✅ - Perfect random access, no compression overhead

### Future (As needed)
5. **AppImage** ✅ - After SquashFS (uses SquashFS internally)

### ❌ Explicitly Excluded
- **TAR** (all variants) - Sequential format, no index
- **CPIO** - Sequential format, no index
- **RAR** - Proprietary, licensing issues
- **StuffIt** - Legacy, declining use
- **tar.gz, tar.bz2, tar.xz** - Solid compression, no random access

---

## Critical Design Principle

> **"Only formats with a central index/directory that enables O(1) or O(log n) file lookup are acceptable for VFS mounting."**

Sequential formats requiring O(n) scanning are incompatible with the FileSystem interface expectations.

---

## Architecture Validation

Each candidate backend must answer:

1. ✅ **Does it have a central directory/index?**
   - ZIP: Yes (Central Directory)
   - DwarFS: Yes (Metadata blocks)
   - SquashFS: Yes (Inode table)
   - 7z: Yes (Header with file list)
   - ISO 9660: Yes (Volume descriptor + directory records)
   - TAR: ❌ NO (sequential)

2. ✅ **Can we seek directly to any file?**
   - All above: Yes (except TAR)

3. ✅ **Is each file independently accessible?**
   - All above: Yes (except solid archives)

---

## Testing Implications

Each backend needs performance tests:
```cpp
TEST(BackendPerformance, RandomFileAccess) {
    auto fs = BackendFactory::create_squashfs();
    fs->mount("test.sqsh", "/mnt");

    // Should be O(1) or O(log n), not O(n)
    auto start = chrono::steady_clock::now();

    // Access file near end without scanning entire archive
    auto handle = fs->open("/mnt/file_at_end.txt", O_RDONLY);

    auto duration = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - start);

    // Should be sub-millisecond for indexed access
    EXPECT_LT(duration.count(), 10);  // 10ms max
}
```

---

## Summary: Random-Access Formats Only

| Format | Index Type | Random Access | Compression | Status |
|--------|-----------|---------------|-------------|---------|
| **DwarFS** | Metadata blocks | ✅ Yes | Excellent | ✅ Implemented |
| **ZIP** | Central directory | ✅ Yes | Good | 🚧 Stage 2 |
| **SquashFS** | Inode table | ✅ Yes | Excellent | 📋 Stage 3 |
| **7z** | Header index | ✅ Yes | Best | 📋 Stage 4 |
| **ISO 9660** | Directory records | ✅ Yes | None | 📋 Stage 4 |
| **AppImage** | SquashFS | ✅ Yes | Excellent | 📋 Future |
| ~~TAR~~ | None | ❌ No | Varies | ❌ Excluded |
| ~~CPIO~~ | None | ❌ No | N/A | ❌ Excluded |

---

**Document Version**: 2.0
**Last Updated**: 2025-12-22
**Status**: Random-Access Architectures Only