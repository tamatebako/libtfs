# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

## [2.0.0] - 2025-01-17

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

- **Serialization format migration**
  - FlatBuffers is now the primary serialization format (header-only)
  - Upstream dwarfs updated to use FlatBuffers internally
  - Legacy cereal and bitsery support deprecated
  - Thrift support removed (not static-link friendly)

- **Build system updates**
  - CMake option `DWARFS_WITH_FLATBUFFERS` now recommended (replaces cereal/bitsery)
  - CMake option `DWARFS_WITH_THRIFT=OFF` now strongly recommended
  - Build configuration simplified for static linking
  - Tebako build mode (`TEBAKO_BUILD=ON`) optimized for new structure

### Removed

- **Folly dependency completely removed**
  - All folly types replaced with pure C++17 standard library
  - `folly::Synchronized` → `std::shared_mutex` + custom wrapper
  - `folly::Conv` → standard C library conversion functions
  - Zero folly symbols in final binary

- **Thrift dependency removed**
  - Not compatible with static linking requirements
  - Replaced by FlatBuffers in upstream dwarfs
  - Significantly reduces dependency complexity

- **Legacy serialization options**
  - Cereal support deprecated (FlatBuffers preferred)
  - Bitsery support deprecated (FlatBuffers preferred)

### Added

- **Comprehensive documentation**
  - Implementation plan for three-stage transformation
  - Stage 1: Rename & Solidify (this release)
  - Stage 2: ZIP backend addition (planned)
  - Stage 3: Multi-language support (planned)
  - FlatBuffers migration guide
  - Testing strategy documentation

- **Improved static linking support**
  - Zero problematic dependencies for static builds
  - Fully compatible with Tebako packaging
  - Header-only dependencies (FlatBuffers)
  - No ABI compatibility issues

### Fixed

- **Static linking compatibility**
  - Resolved all shared library symbol conflicts
  - Eliminated problematic dependencies (folly, thrift)
  - Improved build system for static-only builds

- **Header organization**
  - Clear separation between public API and internal implementation
  - Proper include guard consistency
  - Better namespace organization

### Documentation

- Updated README.md with libtfs branding and v2.0.0 migration guide
- Added CHANGELOG.md (this file) for version tracking
- Reorganized documentation structure:
  - Current implementation plans in `docs/`
  - Historical documentation archived in `docs/archive/`
- Updated all examples to use new header paths
- Added comprehensive API examples

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
   ```bash
   # Old
   cmake -DTEBAKO_BUILD=ON \
         -DDWARFS_WITH_THRIFT=OFF \
         -DDWARFS_WITH_CEREAL=ON \
         -DDWARFS_WITH_BITSERY=ON \
         ..

   # New (recommended)
   cmake -DTEBAKO_BUILD=ON \
         -DDWARFS_WITH_THRIFT=OFF \
         -DDWARFS_WITH_FLATBUFFERS=ON \
         ..
   ```

5. **Review API changes**
   - All public APIs remain functionally compatible
   - Only header paths and project names changed
   - Binary compatibility not maintained (recompile required)

## [1.x.x] - Historical

Previous versions released as `libdwarfs-wr`. See git history for details:
- v1.x series: Original libdwarfs wrapper implementation
- Folly-dependent versions
- Thrift-dependent versions

For historical changes, see git commit history prior to v2.0.0.

---

## Release Notes

### v2.0.0 Release Highlights

This is a major release that modernizes the project with a clean break from the past:

✅ **Pure C++17** - No more folly/thrift dependencies
✅ **Static linking ready** - Perfect for Tebako packaging
✅ **Modern organization** - Clean header hierarchy
✅ **FlatBuffers** - Header-only serialization
✅ **Better documentation** - Comprehensive guides and examples

### Known Issues

None reported for v2.0.0 at time of release.

### Acknowledgments

- Upstream dwarfs project (https://github.com/mhx/dwarfs) for the core filesystem implementation
- Tebako project (https://github.com/tamatebako) for packaging requirements and use cases
- All contributors who provided feedback and testing

---

[Unreleased]: https://github.com/tamatebako/libtfs/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/tamatebako/libtfs/releases/tag/v2.0.0