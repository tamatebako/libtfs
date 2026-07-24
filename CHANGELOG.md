# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Multi-mount support in the modern C API: `FsContext` now keeps a mount table (`tebako_mount_t` handle → mount) instead of a single mount, with longest-mount-point-prefix path dispatch across mounts and per-mount fd/dir ownership. New additive symbols: `tebako_fs_mount_from_file`, `tebako_fs_mount_from_file_at`, `tebako_fs_mount_from_memory` (all returning a mount handle via out-param) and `tebako_fs_unmount_handle` (force-closes only that mount's fds/dirs). Compat shims are unchanged: `tebako_fs_init*` stays single-mount (`EEXIST` when any mount exists), `tebako_fs_unmount()` unmounts all, and `tebako_get_mount_point`/`tebako_get_archive_path`/`tebako_get_backend_name` keep reporting the `init*` mount. `tebako_fs_extract_all` extracts a single mount at the destination root as before; with multiple mounts each mount's tree goes into its own `<dest>/<mount-point-basename>` subtree.
- tebakofs package tooling for tebako three-part packages (bootstrap + image slots + `tpkg` manifest trailer): `bundle`, `unbundle`, `reassemble`, `insert-image`, `remove-image`, `set-runtime`, `mkimage` (mkdwarfs wrapper; dwarfs only — the zip backend is read-only), and `tebakofs info` now detects and dumps a `tpkg` trailer while keeping archive-info behavior for plain image files.
- Release pipeline: per-platform `libtfs-deps-<version>-<platform>.tar.gz` package carrying the exact transitive static libraries consumers link against (dwarfs reader set, flatbuffers, libzip, fmt, xxhash, zstd/lz4/lzma/brotli/z/bzip2, boost filesystem+chrono; plus OpenSSL on Linux/Windows — macOS consumers link brew/system OpenSSL) together with those ports' CMake package configs. A `libtfs` package plus the matching `libtfs-deps` package are fully self-contained: no vcpkg needed downstream.

## [0.12.6] - 2026-07-22

### Added
- libtfs-deps packages now include curated headers (brotli, zstd, lz4, lzma, fmt, flatbuffers, boost fs+chrono, …) so packaged native gem extensions build without vcpkg.

## [0.12.5] - 2026-07-22

### Added
- Self-contained `libtfs-deps-<ver>-<platform>` release packages (transitive static libs) — consumers need no vcpkg on the prebuilt path.

## [0.12.4] - 2026-07-22

### Fixed
- dwarfs-t tebako-v0.14.1-12: fix nondeterministic mount/read failures (file_extents_iterable UAF — the iterable now owns a copy of the extents).

## [0.12.3] - 2026-07-22

### Fixed
- Exit-time static destruction order: singleton tables (memfs/mount/fd/kfd/dir) are now constructed at mount time so packaged binaries no longer abort with "Unhandled exception" (SIGABRT) on process exit.

## [0.12.2] - 2026-07-21

### Fixed

- **dwarfs-t backend bumped to `tebako-v0.14.1-11`**: fixes corrupt reads for
  duplicate-content files (flatbuffers reader resolved deduplicated chunks to
  the wrong inode). No libtfs API changes.

## [0.12.1] - 2026-07-21

### Added

- **Legacy tebako shim quartet restored** (`src/file-io.cpp`, `src/file-ctl.cpp`,
  `src/dir-io.cpp`, `src/dir-ctl.cpp`): the tebako-facing POSIX surface
  (`tebako_open`, `tebako_mkdir`, `tebako_pread`, `tebako_unlink`, …) is built
  again where `WITH_LEGACY_TEBAKO_API` is ON. (`tebako_eaccess` remains
  glibc-only and is not provided on macOS.)

### Changed

- **Modern C API namespaced `tebako_fs_*`**: the 9 modern C API functions that
  collided with the restored legacy shims were renamed, so the legacy and
  modern APIs can now be linked into one binary.
- **Release artifacts are SquashFS-free**: packages are built with
  `WITH_SQUASHFS=OFF` (dwarfs + zip backends only) so consumers are not forced
  to link LGPL squashfs-tools-ng. POSIX source builds may still opt into the
  SquashFS backend with `-DWITH_SQUASHFS=ON`.

## [0.12.0] - 2026-07-21

First release as **libtfs** (Tebako File System), formerly `libdwarfs-wr`.
The changes below were originally drafted as a "v2.0.0" release; they are
released as v0.12.0 (no v2.0.0 tag was ever cut).

### Changed - BREAKING CHANGES

- **Project renamed from `libdwarfs-wr` to `libtfs` (Tebako File System)**
  - Repository moved to `github.com/tamatebako/libtfs`
  - CMake project name changed from `dwarfs` to `libtfs`
  - All library artifacts now use `libtfs` naming
  - No backward compatibility provided - clean break from v1.x

- **Header organization completely restructured**
  - Headers moved from flat `include/` to hierarchical `include/tebako/fs/`
  - Public API headers in `include/tebako/fs/`
  - Internal headers in `include/tebako/fs/internal/`
  - Utility headers in `include/tebako/fs/util/`
  - Old header paths no longer supported

- **Thrift-free dwarfs-t backend**
  - Backend is the `tamatebako/dwarfs-t` fork, pinned at `tebako-v0.14.1-5`
  - FlatBuffers multi-format metadata is now the only serialization format (header-only)
  - Legacy cereal and bitsery support removed
  - Thrift support removed (not static-link friendly)

- **Legacy tebako API reduced in scope**
  - Limited to the POSIX API and ruby-build-context support (`WITH_LEGACY_TEBAKO_API`)
  - All other consumers use the modern C/C++ API

- **Build system replaced with a self-contained vcpkg build**
  - Manifest-mode vcpkg with overlay ports in `vcpkg-overlay/`:
    `dwarfs` (dwarfs-t fork), `jemalloc`, `squashfs-tools-ng`
  - Build configuration simplified for static linking

### Added

- **Multi-backend VFS**
  - Unified VFS interface with backend auto-detection (magic bytes + extensions)
  - DwarFS backend (dwarfs-t, FlatBuffers metadata)
  - ZIP backend (libzip) with directory traversal and seek support
  - SquashFS backend (squashfs-tools-ng) - POSIX platforms only (not built on Windows)

- **`tebakofs` CLI tool**
  - Docker-style commands: `ls`, `cat`, `tree`, `extract`, `find`, `info`, `stat`
  - Automatic archive format detection

- **Improved static linking support**
  - Zero problematic dependencies for static builds
  - Fully compatible with Tebako packaging
  - Header-only dependencies (FlatBuffers)
  - No ABI compatibility issues

- **All-platform CI green**
  - Ubuntu (glibc) x64 and ARM64
  - Alpine (musl)
  - macOS arm64 and x86_64
  - Windows MSYS2 UCRT64

### Removed

- **Folly dependency completely removed**
  - All folly types replaced with pure C++17 standard library
  - Zero folly symbols in final binary

- **Thrift dependency removed**
  - Not compatible with static linking requirements
  - Replaced by FlatBuffers multi-format metadata in dwarfs-t

- **Legacy serialization options**
  - Cereal and bitsery support removed (FlatBuffers preferred)

### Fixed

- **Static linking compatibility**
  - Resolved all shared library symbol conflicts
  - Eliminated problematic dependencies (folly, thrift)
  - Improved build system for static-only builds

### Migration Guide from v1.x (libdwarfs-wr)

If you are upgrading from libdwarfs-wr v1.x:

1. **Update repository references**
   ```bash
   # Old
   git clone https://github.com/tamatebako/libdwarfs.git

   # New
   git clone https://github.com/tamatebako/libtfs.git
   ```

2. **Update CMake project name**
   ```cmake
   # Old
   project(dwarfs)

   # New
   project(libtfs)
   ```

3. **Update include paths**
   ```cpp
   // Old
   #include <tebako-io.h>
   #include <tebako-memfs.h>

   // New
   #include <tebako/fs/io.h>
   #include <tebako/fs/memfs.h>
   ```

4. **Update build configuration**

   Dependencies are provided by vcpkg (manifest mode, using the overlay
   ports in `vcpkg-overlay/`):

   ```bash
   cmake -S . -B build \
         -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
         -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

5. **Review API changes**
   - Header paths and project/artifact names changed
   - Legacy tebako API limited to POSIX + ruby-build-context (`WITH_LEGACY_TEBAKO_API`)
   - Binary compatibility not maintained (recompile required)

## [0.11.0] - 2025-12-24

### Added
- 100% test pass rate (140/140 tests)
- Production-ready C API with thread-local errno
- Integration test automation script
- Performance baseline documentation
- Comprehensive release notes
- Performance regression test infrastructure
- Tebako integration guide with Ruby FFI examples
- Documentation index for easy navigation

### Fixed
- File existence check in backend factory
- Version string formatting in ZIP backend
- String lifetime in tebako_get_backend_name()
- Test fixture corrupted.zip recreation
- Function signature for tebako_init_cwd()

### Changed
- Removed legacy C API implementation (~22 KB)
- Validated SOLID architecture principles
- Memory safety audit completed (RAII enforced)
- Archived Phase 3 and Phase 4 documentation

### Documentation
- Phase 4 completion documented in README.adoc
- RELEASE_NOTES.md for v0.11.0
- PERFORMANCE_BASELINE.md established
- ARCHITECTURE.md updated with design decisions
- TEBAKO_INTEGRATION.md with Ruby FFI bindings
- Performance regression testing guide

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for complete details.

## [1.x.x] - Historical

Previous versions released as `libdwarfs-wr`. See git history for details:
- v1.x series: Original libdwarfs wrapper implementation
- Folly-dependent versions
- Thrift-dependent versions

For historical changes, see git commit history prior to v0.12.0.

---

## Release Notes

### v0.12.0 Release Highlights

This release modernizes the project with a clean break from the past:

✅ **Pure C++17** - No more folly/thrift dependencies
✅ **Static linking ready** - Perfect for Tebako packaging
✅ **Multi-backend VFS** - DwarFS, ZIP, and SquashFS behind one interface
✅ **FlatBuffers** - Header-only, multi-format metadata
✅ **Self-contained vcpkg build** - Overlay ports for dwarfs-t, jemalloc, squashfs-tools-ng

### Known Issues

None reported for v0.12.0 at time of release.

### Acknowledgments

- Upstream dwarfs project (https://github.com/mhx/dwarfs) for the core filesystem implementation
- Tebako project (https://github.com/tamatebako) for packaging requirements and use cases
- All contributors who provided feedback and testing

---

[Unreleased]: https://github.com/tamatebako/libtfs/compare/v0.12.1...HEAD
[0.12.1]: https://github.com/tamatebako/libtfs/compare/v0.12.0...v0.12.1
[0.12.0]: https://github.com/tamatebako/libtfs/releases/tag/v0.12.0
[0.11.0]: https://github.com/tamatebako/libtfs/releases/tag/v0.11.0
