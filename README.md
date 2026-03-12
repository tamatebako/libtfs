##  CI status

[![Ubuntu](https://github.com/tamatebako/libtfs/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/tamatebako/libtfs/actions/workflows/ubuntu.yml)   [![MacOS](https://github.com/tamatebako/libtfs/actions/workflows/macos.yml/badge.svg)](https://github.com/tamatebako/libtfs/actions/workflows/macos.yml) [![Alpine](https://github.com/tamatebako/libtfs/actions/workflows/alpine.yml/badge.svg)](https://github.com/tamatebako/libtfs/actions/workflows/alpine.yml)
[![Windows-MSys](https://github.com/tamatebako/libtfs/actions/workflows/windows-msys.yml/badge.svg)](https://github.com/tamatebako/libtfs/actions/workflows/windows-msys.yml)

[![Build Status](https://api.cirrus-ci.com/github/tamatebako/libtfs.svg?task=ubuntu-aarch64)](https://cirrus-ci.com/github/tamatebako/libtfs)

[![lint](https://github.com/tamatebako/libtfs/actions/workflows/lint.yml/badge.svg)](https://github.com/tamatebako/libtfs/actions/workflows/lint.yml) [![codecov](https://codecov.io/gh/tamatebako/libtfs/branch/main/graph/badge.svg?token=FMMPK27XU7)](https://codecov.io/gh/tamatebako/libtfs) [![coverity](https://scan.coverity.com/projects/27408/badge.svg)](https://scan.coverity.com/projects/tamatebako-libtfs) [![codeql](https://github.com/tamatebako/libtfs/actions/workflows/codeql.yml/badge.svg)](https://github.com/tamatebako/libtfs/actions/workflows/codeql.yml)

##  libtfs - Tebako File System

**libtfs** (Tebako File System) is a modern C++17 virtual filesystem library for Tebako, providing a unified interface for multiple archive formats.

### Features
* **Multi-backend architecture**: Support for DwarFS and ZIP (with more formats coming)
* **DwarFS backend**: Full support for DwarFS archives with legacy Thrift/Frozen2 format compatibility
* **ZIP backend**: Complete ZIP archive support with directory traversal
* **C interface**: C API alongside C++ for maximum compatibility
* **FlatBuffers serialization**: Header-only, static-link friendly (no Thrift/folly)
* **File descriptor addressing**: POSIX-like fd interface above filesystem implementation
* **Zero problematic dependencies**: Pure C++17, no libfolly in our code

### v2.0.0 Status

**Stage 1: FlatBuffers Migration** - Complete ✅ (2025-12-21)

All objectives achieved:
- ✅ Dwarfs libraries configured with FlatBuffers-only serialization
- ✅ Thrift dependencies removed
- ✅ All 6 dwarfs libraries building successfully
- ✅ Headers reorganized to `include/tebako/fs/` structure

**Stage 2: Multi-Backend Implementation** - Complete ✅ (2025-02-18)

- ✅ **ZIP Backend fully functional**
  - ZipBackend class implementing all FileSystem methods
  - ZipFileHandle for file reading with seek support
  - ZipDirectoryIterator for directory traversal
  - Thread-safe concurrent read operations
  - 140 tests passing

- ✅ **DwarFS Backend fully functional** (NEW!)
  - DwarfsBackend class using filesystem_v2_lite API
  - Support for legacy Thrift/Frozen2 format archives
  - Compact names (FSST compression) support
  - Directory iteration and file reading
  - 47 tests passing

**Total: 187 tests passing** (140 ZIP + 47 DwarFS)

### Testing

```bash
cd build
ctest --output-on-failure
```

**Test Coverage** (as of 2025-02-18):
- ✅ BackendFactory tests passing
- ✅ ZIP backend: 140 tests passing
- ✅ DwarFS backend: 47 tests passing
- ✅ C API tests passing
- ✅ Integration tests passing

### Performance Benchmarks

Run the benchmark suite to compare backend performance:

```bash
./benchmarks/quick_benchmark.sh
```

Results are saved to `benchmarks/results/`.

### Examples

The `examples/` directory contains comprehensive example programs demonstrating how to use the libtfs API:

* **basic_usage.cpp** - Basic DwarFS operations (mount, read, unmount)
* **api_example.cpp** - Comprehensive API demonstration (file/directory operations, stat, navigation)

To build the examples:

```bash
mkdir build && cd build
cmake -DBUILD_EXAMPLES=ON ..
make -j$(nproc)

# Run examples (requires a DwarFS image)
./examples/basic_usage filesystem.dwarfs /path/to/file.txt
./examples/api_example filesystem.dwarfs /path/to/file.txt /path/to/directory
```

For detailed information about the examples, including API usage patterns and troubleshooting, see [examples/README.md](examples/README.md).

### Documentation

#### Current Development
* **[Continuation Plan](docs/CONTINUATION_PLAN.md)** - Next stages roadmap ⭐
* **[Stage 1 Final Status](docs/STAGE_1_FINAL_STATUS.md)** - FlatBuffers migration complete ⭐
* [Implementation Plan](docs/IMPLEMENTATION_PLAN.md) - Overall LibTFS transformation roadmap
* [Architecture](docs/ARCHITECTURE.md) - System architecture and design
* [FlatBuffers Migration](docs/FLATBUFFERS_MIGRATION.md) - Serialization update details
* [Testing Strategy](docs/TESTING_STRATEGY.md) - Comprehensive testing approach

#### Archived Documentation
* [Final Solution](docs/archive/FINAL_SOLUTION.md) - Historical static linking solution
* [Dependency Strategy](docs/archive/DEPENDENCY_STRATEGY.md) - Historical dependency management
* [Folly Removal Summary](docs/archive/FOLLY_REMOVAL_SUMMARY.md) - Historical wrapper changes
* [Stage 1 Planning](docs/archive/STAGE_1_PLAN.md) - Stage 1 planning documents

### Architecture

libtfs uses a three-layer architecture with multi-backend support:

```
Application → libtfs (C++17) → Backends → Dependencies
                              ├── DwarFS (FlatBuffers)
                              └── ZIP (libzip)
```

* **libtfs**: Pure C++17, no folly/thrift in our code or API
* **Backends**: Pluggable architecture for different archive formats
* **Dependencies**: Fully isolated, not exposed in public API

Key architectural principles:
- **Backend abstraction**: Unified VFS interface for all formats
- **Header-only serialization**: FlatBuffers eliminates complex dependencies
- **Static-link friendly**: All dependencies carefully chosen for static linking

See [Architecture](docs/ARCHITECTURE.md) for complete details.

### Project Status

| Feature | Status |
|---------|--------|
| DwarFS Backend | ✅ Complete (FlatBuffers) |
| ZIP Backend | ✅ Complete (Day 2) - Unit tests pending |
| SquashFS Backend | 📋 Planned (Day 5-6) |
| TAR Backend | 📋 Future |
| Multi-language Support | 📋 Future (Stage 3) |

### License

This project is licensed under the same terms as DwarFS.

### Contributing

Contributions are welcome! Please see our contribution guidelines and code of conduct.

### Support

For issues, questions, or contributions, please visit our [GitHub repository](https://github.com/tamatebako/libtfs).
