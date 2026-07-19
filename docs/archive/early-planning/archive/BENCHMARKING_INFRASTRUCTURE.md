# Benchmarking Infrastructure Guide

**Date**: 2025-12-22  
**Status**: Implementation Guide  
**Purpose**: Real-world performance benchmarking for libtfs backends

---

## Overview

This document specifies the benchmarking infrastructure for comparing ZIP, SquashFS, and DwarFS backend performance using realistic datasets.

## Benchmark Dataset

### Primary Dataset: perl-5.42.0

**Why Perl Source?**
- ✅ **Realistic**: Complex directory structure like real applications
- ✅ **Representative**: Mix of text files, scripts, documentation
- ✅ **Appropriate Size**: 31MB compressed, ~80MB extracted
- ✅ **Reproducible**: Publicly available from CPAN
- ✅ **Well-structured**: Nested directories, varied file sizes

**Statistics**:
```
Source: perl-5.42.0.tar.gz
URL: https://www.cpan.org/src/5.0/perl-5.42.0.tar.gz
Compressed: 31 MB
Extracted: ~80 MB
Files: ~18,500
Directories: ~700
```

**Directory Structure**:
```
perl-5.42.0/
├── lib/          # ~15,000 .pm modules (Perl libraries)
├── t/            # ~3,000 test files
├── dist/         # ~500 distribution directories
├── cpan/         # ~200 CPAN module directories
├── pod/          # ~100 documentation files
├── ext/          # Extensions
├── utils/        # Utility scripts
└── [Configure, README, etc.]
```

## Fixture Generation Script

### Script: `tests/fixtures/create_benchmark_fixtures.sh`

**Purpose**: Download perl source and create benchmark archives in all supported formats

```bash
#!/bin/bash
set -e

# ==============================================================================
# Benchmark Fixture Generation Script
# Creates real-world filesystem archives for performance benchmarking
# ==============================================================================

PERL_VERSION="5.42.0"
PERL_TARBALL="perl-${PERL_VERSION}.tar.gz"
PERL_URL="https://www.cpan.org/src/5.0/${PERL_TARBALL}"
PERL_DIR="perl-${PERL_VERSION}"

echo "========================================="
echo "Creating Benchmark Fixtures"
echo "Dataset: ${PERL_DIR} (realistic 31MB)"
echo "========================================="
echo ""

# Check for required tools
if ! command -v zip &> /dev/null; then
    echo "Error: 'zip' command not found. Please install zip."
    exit 1
fi

if ! command -v mksquashfs &> /dev/null; then
    echo "Warning: 'mksquashfs' not found. SquashFS benchmarks will be skipped."
    echo "Install: apt-get install squashfs-tools (Ubuntu) or brew install squashfs (macOS)"
    SKIP_SQUASHFS=1
fi

# Download perl source if not present
if [ ! -f "${PERL_TARBALL}" ]; then
    echo ">>> Downloading ${PERL_TARBALL}..."
    if command -v curl &> /dev/null; then
        curl -L -O "${PERL_URL}"
    elif command -v wget &> /dev/null; then
        wget "${PERL_URL}"
    else
        echo "Error: Neither curl nor wget found. Cannot download perl."
        exit 1
    fi
    echo "✅ Downloaded ${PERL_TARBALL}"
else
    echo "✅ Using existing ${PERL_TARBALL}"
fi

# Extract perl source
if [ ! -d "${PERL_DIR}" ]; then
    echo ""
    echo ">>> Extracting ${PERL_TARBALL}..."
    tar xzf "${PERL_TARBALL}"
    echo "✅ Extracted to ${PERL_DIR}"
else
    echo "✅ Using existing ${PERL_DIR} directory"
fi

# Get original size
ORIGINAL_SIZE=$(du -sb "${PERL_DIR}" | cut -f1)
ORIGINAL_MB=$(echo "scale=2; ${ORIGINAL_SIZE} / 1024 / 1024" | bc)

echo ""
echo ">>> Source tree statistics:"
echo "    Size: ${ORIGINAL_MB} MB (${ORIGINAL_SIZE} bytes)"
FILE_COUNT=$(find "${PERL_DIR}" -type f | wc -l | tr -d ' ')
DIR_COUNT=$(find "${PERL_DIR}" -type d | wc -l | tr -d ' ')
echo "    Files: ${FILE_COUNT}"
echo "    Directories: ${DIR_COUNT}"

# Create ZIP benchmark archive
echo ""
echo ">>> Creating benchmark.zip..."
START=$(date +%s)
(cd "${PERL_DIR}" && zip -q -r ../benchmark.zip .)
END=$(date +%s)
ZIP_TIME=$((END - START))
ZIP_SIZE=$(stat -f%z benchmark.zip 2>/dev/null || stat -c%s benchmark.zip)
ZIP_MB=$(echo "scale=2; ${ZIP_SIZE} / 1024 / 1024" | bc)
ZIP_RATIO=$(echo "scale=1; 100 * ${ZIP_SIZE} / ${ORIGINAL_SIZE}" | bc)
echo "✅ Created benchmark.zip"
echo "    Size: ${ZIP_MB} MB (${ZIP_SIZE} bytes)"
echo "    Compression: ${ZIP_RATIO}% of original"
echo "    Time: ${ZIP_TIME}s"

# Create SquashFS benchmark archive
if [ -z "${SKIP_SQUASHFS}" ]; then
    echo ""
    echo ">>> Creating benchmark.sqfs..."
    START=$(date +%s)
    mksquashfs "${PERL_DIR}" benchmark.sqfs -noappend -quiet -comp gzip
    END=$(date +%s)
    SQFS_TIME=$((END - START))
    SQFS_SIZE=$(stat -f%z benchmark.sqfs 2>/dev/null || stat -c%s benchmark.sqfs)
    SQFS_MB=$(echo "scale=2; ${SQFS_SIZE} / 1024 / 1024" | bc)
    SQFS_RATIO=$(echo "scale=1; 100 * ${SQFS_SIZE} / ${ORIGINAL_SIZE}" | bc)
    SQFS_VS_ZIP=$(echo "scale=1; 100 * ${SQFS_SIZE} / ${ZIP_SIZE}" | bc)
    echo "✅ Created benchmark.sqfs"
    echo "    Size: ${SQFS_MB} MB (${SQFS_SIZE} bytes)"
    echo "    Compression: ${SQFS_RATIO}% of original"
    echo "    vs ZIP: ${SQFS_VS_ZIP}%"
    echo "    Time: ${SQFS_TIME}s"
fi

# Create DwarFS benchmark archive (if mkdwarfs available)
if command -v mkdwarfs &> /dev/null; then
    echo ""
    echo ">>> Creating benchmark.dwarfs..."
    START=$(date +%s)
    mkdwarfs -i "${PERL_DIR}" -o benchmark.dwarfs -l7 -C zstd:level=19 2>/dev/null
    END=$(date +%s)
    DWARFS_TIME=$((END - START))
    DWARFS_SIZE=$(stat -f%z benchmark.dwarfs 2>/dev/null || stat -c%s benchmark.dwarfs)
    DWARFS_MB=$(echo "scale=2; ${DWARFS_SIZE} / 1024 / 1024" | bc)
    DWARFS_RATIO=$(echo "scale=1; 100 * ${DWARFS_SIZE} / ${ORIGINAL_SIZE}" | bc)
    DWARFS_VS_ZIP=$(echo "scale=1; 100 * ${DWARFS_SIZE} / ${ZIP_SIZE}" | bc)
    echo "✅ Created benchmark.dwarfs"
    echo "    Size: ${DWARFS_MB} MB (${DWARFS_SIZE} bytes)"
    echo "    Compression: ${DWARFS_RATIO}% of original"
    echo "    vs ZIP: ${DWARFS_VS_ZIP}%"
    echo "    Time: ${DWARFS_TIME}s"
else
    echo ""
    echo "⚠️  mkdwarfs not found, skipping benchmark.dwarfs"
    echo "    Install from: https://github.com/mhx/dwarfs"
fi

# Optionally cleanup extracted source
echo ""
read -p "Remove extracted source directory ${PERL_DIR}? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "${PERL_DIR}"
    echo "✅ Cleaned up ${PERL_DIR}"
else
    echo "ℹ️  Keeping ${PERL_DIR} for inspection"
fi

# Summary
echo ""
echo "========================================="
echo "Benchmark Fixtures Created Successfully"
echo "========================================="
echo ""
ls -lh benchmark.* 2>/dev/null || true
echo ""
echo "Compression Comparison:"
echo "  Original:  ${ORIGINAL_MB} MB (100%)"
echo "  ZIP:       ${ZIP_MB} MB (${ZIP_RATIO}%)"
if [ -z "${SKIP_SQUASHFS}" ]; then
    echo "  SquashFS:  ${SQFS_MB} MB (${SQFS_RATIO}%) ← ${SQFS_VS_ZIP}% of ZIP"
fi
if [ -f "benchmark.dwarfs" ]; then
    echo "  DwarFS:    ${DWARFS_MB} MB (${DWARFS_RATIO}%) ← ${DWARFS_VS_ZIP}% of ZIP"
fi
echo ""
echo "Next steps:"
echo "  1. Run: ctest -R benchmark"
echo "  2. Or manually: ./build/benchmark_scenarios"
echo "  3. View results with: python3 tools/analyze_benchmarks.py"
echo ""
```

**Usage**:
```bash
cd tests/fixtures
chmod +x create_benchmark_fixtures.sh
./create_benchmark_fixtures.sh
```

## Benchmark Scenarios

### Scenario 1: Sequential Read
**Purpose**: Measure sustained read throughput

**Method**: Read a large file (Configure script, ~25KB) sequentially

**Expected Results**:
- ZIP: ~50 MB/s (deflate decompression)
- SquashFS: ~100 MB/s (gzip, less overhead)
- DwarFS: ~120 MB/s (zstd, optimized)

### Scenario 2: Random Read
**Purpose**: Measure small file access performance

**Method**: Read 20 random small files (.pm modules)

**Expected Results**:
- ZIP: 6-7 ms per file (close/reopen overhead)
- SquashFS: 0.5-0.8 ms per file (native seek)
- DwarFS: 0.6-0.9 ms per file (native seek)

**Key Insight**: ZIP's lack of native seek causes **10× slower** random access

### Scenario 3: Directory Listing
**Purpose**: Measure directory traversal performance

**Method**: List `/lib` directory (~15,000 .pm files)

**Expected Results**:
- ZIP: ~2-3 seconds (iterate central directory)
- SquashFS: ~150-200 ms (optimized inode table)
- DwarFS: ~200-250 ms (compressed metadata)

**Key Insight**: SquashFS **10× faster** for large directory listings

### Scenario 4: Metadata Operations
**Purpose**: Measure stat/permissions query performance

**Method**: Call `stat()` on 50 random files

**Expected Results**:
- ZIP: ~10-15 ms total (cached central directory)
- SquashFS: ~5-8 ms total (native POSIX metadata)
- DwarFS: ~8-12 ms total (FlatBuffers lookup)

### Scenario 5: Seek Operations
**Purpose**: Measure random access within a file

**Method**: Seek to 3 random positions in Configure file, read 1KB each

**Expected Results**:
- ZIP: **15-20 ms** (must close and reopen file for each seek)
- SquashFS: **<0.1 ms** (native lseek support)
- DwarFS: **<0.1 ms** (native lseek support)

**Key Insight**: ZIP seek is **200× slower** than SquashFS/DwarFS

### Scenario 6: Compression Ratio
**Purpose**: Measure storage efficiency

**Expected Results**:
```
Original:  80 MB (extracted)
ZIP:      38-40 MB (48-50% of original)
SquashFS: 25-28 MB (31-35% of original) ← 30% smaller than ZIP
DwarFS:   20-23 MB (25-29% of original) ← 45% smaller than ZIP
```

## Benchmark Implementation

### CMake Integration

Add to `CMakeLists.txt`:
```cmake
if(WITH_BENCHMARKS)
  find_package(benchmark REQUIRED)
  
  add_executable(benchmark_scenarios
    tests/benchmark_scenarios.cpp
  )
  
  target_link_libraries(benchmark_scenarios PRIVATE
    tfs
    benchmark::benchmark
  )
  
  # Copy benchmark fixtures
  file(COPY tests/fixtures/benchmark.zip
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures)
  file(COPY tests/fixtures/benchmark.sqfs
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures)
  file(COPY tests/fixtures/benchmark.dwarfs
       DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures)
endif()
```

### Running Benchmarks

```bash
# Build with benchmarks
cmake -B build -DWITH_BENCHMARKS=ON -DWITH_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build -j$(nproc)

# Generate fixtures (one-time)
cd tests/fixtures
./create_benchmark_fixtures.sh
cd ../..

# Run benchmarks
./build/benchmark_scenarios \
  --benchmark_out=results.json \
  --benchmark_out_format=json \
  --benchmark_repetitions=10

# Analyze results
python3 tools/analyze_benchmarks.py results.json
```

## Expected Benchmark Report

```
========================================
Benchmark Results: ZIP vs SquashFS vs DwarFS
========================================

Dataset: perl-5.42.0 (31 MB tar.gz, 80 MB extracted)
Date: 2025-12-22
Platform: Linux x86_64, 16 GB RAM, SSD storage

----------------------------------------
Compression Ratios
----------------------------------------
Original:  80.0 MB  (100%)
ZIP:       38.5 MB  (48.1%) 
SquashFS:  26.2 MB  (32.8%)  ✓ 32% smaller than ZIP
DwarFS:    21.8 MB  (27.3%)  ✓ 43% smaller than ZIP

Winner: DwarFS (best compression)

----------------------------------------
Sequential Read (Configure file, 25 KB)
----------------------------------------
ZIP:       156 MB/s  (± 12 MB/s)
SquashFS:  243 MB/s  (± 18 MB/s)  ✓ 56% faster
DwarFS:    198 MB/s  (± 15 MB/s)  ✓ 27% faster

Winner: SquashFS (best throughput)

----------------------------------------
Random Read (20 small .pm files)
----------------------------------------
ZIP:       127 ms    (6.4 ms/file)
SquashFS:   12 ms    (0.6 ms/file)  ✓ 91% faster
DwarFS:     15 ms    (0.8 ms/file)  ✓ 88% faster

Winner: SquashFS (10× faster random access)

----------------------------------------
Directory Listing (/lib, 15,000 files)
----------------------------------------
ZIP:      2,340 ms
SquashFS:   180 ms   ✓ 92% faster
DwarFS:     220 ms   ✓ 91% faster

Winner: SquashFS (13× faster directory listing)

----------------------------------------
Metadata Operations (stat 50 files)
----------------------------------------
ZIP:       15 ms     (0.30 ms/file)
SquashFS:   8 ms     (0.16 ms/file)  ✓ 47% faster
DwarFS:    10 ms     (0.20 ms/file)  ✓ 33% faster

Winner: SquashFS (fastest metadata access)

----------------------------------------
Seek Operations (3 seeks in 1 file)
----------------------------------------
ZIP:      18.5 ms    (close/reopen overhead)
SquashFS:  0.08 ms   ✓ 231× faster
DwarFS:    0.09 ms   ✓ 206× faster

Winner: SquashFS (native seek support)

----------------------------------------
Mount Time (initial open)
----------------------------------------
ZIP:       12 ms     (read central directory)
SquashFS:   5 ms     (read superblock + inodes)
DwarFS:    18 ms     (decompress metadata)

Winner: SquashFS (fastest mount)

========================================
Overall Recommendations
========================================

Use SquashFS when:
  ✓ Performance is critical
  ✓ Random access patterns
  ✓ Large directory listings
  ✓ POSIX permissions needed
  → Best for production Tebako applications

Use DwarFS when:
  ✓ Minimizing package size is priority
  ✓ Sequential read patterns
  ✓ Storage/bandwidth costs matter
  → Best for distribution

Use ZIP when:
  ✓ Maximum compatibility needed
  ✓ Simple read-once patterns
  ✓ Standard tooling required
  → Best for initial prototyping

========================================
Performance Summary
========================================

                    ZIP      SquashFS  DwarFS
Compression:        ★★☆      ★★★       ★★★★★
Sequential:         ★★★      ★★★★★     ★★★★
Random Access:      ★☆☆      ★★★★★     ★★★★★
Dir Listing:        ★☆☆      ★★★★★     ★★★★
Metadata:           ★★★      ★★★★★     ★★★★
Seek:               ★☆☆      ★★★★★     ★★★★★
Mount Time:         ★★★★     ★★★★★     ★★★

Overall Winner: SquashFS
  - Best all-around performance
  - Good compression
  - Native POSIX support
  - Recommended default for Tebako

========================================
```

## CI/CD Integration

### GitHub Actions Workflow

```yaml
name: Benchmarks

on:
  push:
    branches: [main]
  pull_request:
  schedule:
    - cron: '0 0 * * 0'  # Weekly

jobs:
  benchmark:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest]
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          if [ "$RUNNER_OS" == "Linux" ]; then
            sudo apt-get update
            sudo apt-get install -y squashfs-tools
          else
            brew install squashfs
          fi
      
      - name: Setup vcpkg
        run: |
          git clone https://github.com/Microsoft/vcpkg.git
          ./vcpkg/bootstrap-vcpkg.sh
      
      - name: Generate fixtures
        run: |
          cd tests/fixtures
          ./create_benchmark_fixtures.sh <<< "n"
      
      - name: Build with benchmarks
        run: |
          cmake -B build -DWITH_BENCHMARKS=ON \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
          cmake --build build -j$(nproc)
      
      - name: Run benchmarks
        run: |
          ./build/benchmark_scenarios \
            --benchmark_out=results-${{ matrix.os }}.json \
            --benchmark_out_format=json
      
      - name: Upload results
        uses: actions/upload-artifact@v3
        with:
          name: benchmark-results-${{ matrix.os }}
          path: results-${{ matrix.os }}.json
```

## Maintenance

### Updating Benchmark Dataset

When updating to a newer perl version:

1. Update `PERL_VERSION` in `create_benchmark_fixtures.sh`
2. Verify URL is correct
3. Regenerate fixtures
4. Update expected results in documentation
5. Commit new baselines

### Verifying Benchmark Integrity

```bash
# Check fixture checksums
sha256sum benchmark.zip benchmark.sqfs benchmark.dwarfs > checksums.txt

# Verify contents match
mkdir -p verify/zip verify/sqfs verify/dwarfs
unzip -q benchmark.zip -d verify/zip
unsquashfs -d verify/sqfs benchmark.sqfs
dwarfsextract -i benchmark.dwarfs -o verify/dwarfs

# Compare (should be identical)
diff -r verify/zip verify/sqfs
diff -r verify/zip verify/dwarfs
```

---

## Appendix A: benchmark_scenarios.cpp Template

Complete C++ benchmark implementation with Google Benchmark:

```cpp
#include <benchmark/benchmark.h>
#include <tebako/fs/backend_factory.h>
#include <vector>
#include <random>

using namespace tebako::fs;

// Test file paths
const std::vector<std::string> SMALL_FILES = {
    "/lib/strict.pm",
    "/lib/warnings.pm", 
    "/lib/Config.pm",
    "/lib/Carp.pm",
    "/lib/Exporter.pm",
    "/lib/File/Path.pm",
    "/lib/File/Spec.pm",
    "/lib/Getopt/Std.pm",
    "/lib/Text/Wrap.pm",
    "/lib/constant.pm",
    // ... 10 more files
};

const std::string LARGE_FILE = "/Configure";
const std::string DEEP_DIR = "/lib";

// Scenario 1: Sequential Read
static void BM_SequentialRead(benchmark::State& state,
                              const std::string& format) {
    auto fs = BackendFactory::create_from_file(
        "tests/fixtures/benchmark." + format);
    fs->mount("tests/fixtures/benchmark." + format, "/bench");
    
    for (auto _ : state) {
        auto handle = fs->open("/bench" + LARGE_FILE, O_RDONLY);
        char buffer[4096];
        int64_t total_read = 0;
        ssize_t n;
        while ((n = handle->read(buffer, sizeof(buffer))) > 0) {
            total_read += n;
            benchmark::DoNotOptimize(buffer);
        }
        state.counters["bytes"] = total_read;
    }
    
    fs->unmount();
    state.SetBytesProcessed(state.iterations() * 
                           state.counters["bytes"]);
}

BENCHMARK_CAPTURE(BM_SequentialRead, ZIP, "zip");
BENCHMARK_CAPTURE(BM_SequentialRead, SquashFS, "sqfs");
BENCHMARK_CAPTURE(BM_SequentialRead, DwarFS, "dwarfs");

// [Additional scenarios follow same pattern...]

BENCHMARK_MAIN();
```

---

**Document Version**: 1.0  
**Last Updated**: 2025-12-22  
**Next Review**: After benchmark implementation