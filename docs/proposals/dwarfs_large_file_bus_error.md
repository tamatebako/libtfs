# Issue Report: Bus Error When Reading Large Files from DwarFS Archives

## Summary

When reading large binary files (10MB) from DwarFS archives using the `filesystem_v2_lite::read()` method, the application crashes with a **Bus Error (SIGBUS)** on macOS ARM64. The same test code works correctly with the ZIP backend, suggesting the issue is specific to the DwarFS library.

## Environment

- **Platform**: macOS 15.x (Darwin 24.x), ARM64 (Apple Silicon)
- **DwarFS Version**: v0.14.1 (built via vcpkg overlay from `github.com/mhx/dwarfs`)
- **Archive Format**: DwarFS v2.5 (created with mkdwarfs -l 9)
- **Compiler**: AppleClang 17.0.0

## Reproduction

### Test Case
```cpp
// ReadLargeFilePerformance test
std::string archive = "tests/fixtures/dwarfs/large.dwarfs";
auto mount_result = backend->mount(archive, "/mnt/test");

auto handle_result = backend->open("/mnt/test/10mb.bin", O_RDONLY);
auto handle = std::move(handle_result).unwrap();

char buffer[4096];
while (true) {
    ssize_t bytes_read = handle->read(buffer, sizeof(buffer));  // <-- CRASH HERE
    if (bytes_read <= 0) break;
}
```

### Archive Details
```
$ dwarfsck -i large.dwarfs
DwarFS version 2.5 [2]
block size: 64 MiB
block count: 1
inode count: 3
original filesystem size: 11 MiB
compressed block size: 104 B (1.25%)
```

### Crash Output
```
$ ./test_dwarfs_backend --gtest_filter="*ReadLargeFile*"
[ RUN      ] DwarfsBackendTest.ReadLargeFilePerformance
Bus error: 10
```

## Code Path

The crash occurs in the DwarFS reader's `read()` method:

```cpp
// In our DwarfsFileHandle::read()
std::error_code ec;
size_t bytes_read = fs_.read(inode_.inode_num(),   // filesystem_v2_lite&
                             static_cast<char*>(buffer),
                             to_read,
                             current_pos_,
                             ec);
```

## Test File Characteristics

The test file (`10mb.bin`) is a 10MB sparse file filled with zeros:
```
$ xxd 10mb.bin | head -1
00000000: 0000 0000 0000 0000 0000 0000 0000 0000  ................
```

This highly compressible content compresses to ~104 bytes in the archive.

## Backtrace (Updated with Diagnostic)

```
* thread #1, queue = 'com.apple.main-thread', stop reason = EXC_BAD_ACCESS (code=257, address=0xf85b83a8941537b2)
  * frame #0: 0x000003a8941537b2  <- Invalid address (corrupted function pointer?)
    frame #1: dwarfs::internal::worker_group::worker_group(...) + 1588
    frame #2: std::__1::__call_once_proxy<...block_cache_::enqueue_job...> + 156
    frame #3: std::__1::__call_once + 196
    frame #4: dwarfs::reader::block_cache_::enqueue_job + 100
    frame #5: dwarfs::reader::block_cache_::create_cached_block + 604
    frame #6: dwarfs::reader::block_cache_::get + 956
    frame #7: dwarfs::reader::inode_reader_::read_internal + 1300
    frame #8: dwarfs::reader::inode_reader_::read_internal<lambda> + 52
    frame #9: dwarfs::reader::inode_reader_::read + 240
    frame #10: dwarfs::reader::filesystem_::read + 268
    frame #11: DwarfsFileHandle::read + 136  <- Our code calling dwarfs
    frame #12: TestBody (ReadLargeFilePerformance)
```

**Key Observation**: The crash occurs in `worker_group::worker_group` constructor during block cache initialization. The address `0xf85b83a8941537b2` looks like a corrupted function pointer or vtable issue.

## Observations

1. **Small files work**: Reading small files (< 1MB) works correctly
2. **Sequential read crash**: The crash happens during sequential reading
3. **Random access crash**: The crash also happens with random seek+read patterns
4. **ZIP backend works**: The same test with ZIP archives passes
5. **Archive is valid**: `dwarfsextract` successfully extracts the file
6. **Mount succeeds**: The archive mounts without error

## Hypothesis

The bus error suggests a memory alignment issue or invalid memory access. Based on the backtrace:

1. **Worker group initialization issue**: The crash occurs in `worker_group::worker_group` constructor, suggesting a problem with thread pool initialization
2. **Corrupted function pointer**: The invalid address `0xf85b83a8941537b2` suggests memory corruption
3. **Block cache threading**: The issue appears during `enqueue_job` when creating cached blocks for highly compressible data

## Request

Could the DwarFS team investigate:

1. Is there a known issue with reading highly compressible (sparse) files on ARM64?
2. Are there any memory alignment requirements for the `read()` buffer on ARM64?
3. Is there a maximum recommended file size or compression ratio for the reader?
4. Are there any debug flags or additional logging that could help pinpoint the issue?

## Workaround

Currently, we have disabled the performance tests for DwarFS large file reading. The functionality works correctly for typical use cases with smaller files.

## Files for Reproduction

- Archive: `large.dwarfs` (contains `10mb.bin` and `1mb.bin`)
- Test file: 10MB sparse file (all zeros)
- Test code: Sequential and random access read patterns

---

**Contact**: libtfs/tamatebako project
**Date**: 2026-02-20
