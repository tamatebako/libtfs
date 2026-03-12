# Testing Strategy

**Project**: libtfs (Tebako File System)
**Purpose**: Ensure quality throughout transformation
**Scope**: All stages (Stage 1-3)
**Status**: Active

---

## Testing Philosophy

### Core Principles

1. **Test First, Change Second** - Establish baseline before modifications
2. **Continuous Validation** - Test after every significant change
3. **Automated Regression** - Catch regressions immediately
4. **Cross-Platform Coverage** - Linux, macOS, Windows behaviors
5. **Real-World Integration** - Test with actual Tebako workloads

---

## Testing Pyramid

```
        ┌─────────────────┐
        │  Integration    │  10% - End-to-end Tebako
        │   (Tebako)      │
        ├─────────────────┤
        │   Functional    │  30% - API contracts
        │   (C++ Tests)   │
        ├─────────────────┤
        │     Unit        │  60% - Individual components
        │  (Component)    │
        └─────────────────┘
```

---

## Test Categories

### 1. Unit Tests (Component Level)

**Framework**: Google Test (via CTest)
**Location**: `tests/tests-*.cpp`
**Coverage**: Individual classes and functions

**Current Tests**:
- `tests-cmdline.cpp` - Command-line parsing
- `tests-defines.h` - Macro definitions
- `tests-dir-ctl.cpp` - Directory control
- `tests-dir-io.cpp` - Directory I/O operations
- `tests-dl-ctl.cpp` - Dynamic loading control
- `tests-file-ctl.cpp` - File control
- `tests-file-io.cpp` - File I/O operations
- `tests-fs-load.cpp` - Filesystem loading
- `tests-fs-load2.cpp` - Advanced filesystem loading
- `tests-kfd.cpp` - Kernel file descriptor management
- `tests-ln.cpp` - Symlink handling
- `tests-memfs-table.cpp` - In-memory filesystem table
- `tests-mfs.cpp` - Memory filesystem operations
- `tests-mount-dwarfs.cpp` - DwarFS mounting

**Run**:
```bash
cd build
ctest --output-on-failure
```

### 2. Functional Tests (API Level)

**Framework**: Examples as functional tests
**Location**: `examples/`
**Coverage**: Public API contracts

**Tests**:
- `basic_usage.cpp` - Core API usage patterns
- `api_example.cpp` - Advanced API scenarios

**Run**:
```bash
cd build
./examples/basic_usage
./examples/api_example
```

### 3. Integration Tests (System Level)

**Framework**: Tebako packaging
**Location**: Separate Tebako repository
**Coverage**: Real-world Ruby application packaging

**Test Cases**:
- Simple Ruby script
- Rails application
- Gems with native extensions
- Cross-platform binaries

**Run**:
```bash
# In tebako repository
rake test:integration
```

---

## Stage-Specific Testing

### Stage 1: Rename & Solidify

#### Pre-Rename Baseline
```bash
# Before any changes
cd build
cmake -DTEBAKO_BUILD=ON -DWITH_TESTS=ON ..
make -j$(nproc)
ctest --output-on-failure > baseline-tests.log 2>&1

# Verify all tests pass
grep -c "tests passed" baseline-tests.log
```

#### During Rename
After each major change:
```bash
# Quick sanity check
make -j$(nproc) && ctest --output-on-failure

# If failures, rollback and fix
```

#### Post-Rename Validation
```bash
# Full test suite
rm -rf build
mkdir build && cd build
cmake -DTEBAKO_BUILD=ON \
      -DDWARFS_WITH_FLATBUFFERS=ON \
      -DWITH_TESTS=ON ..
make -j$(nproc)
ctest --output-on-failure --verbose

# Compare with baseline
diff baseline-tests.log current-tests.log
```

### Stage 2: ZIP Backend

#### New Test Suite
Create `tests/tests-zip-backend.cpp`:
- ZIP file mounting
- File enumeration in ZIP
- Read operations from ZIP
- Error handling (corrupted ZIP)
- Performance comparison with DwarFS

#### Regression Testing
```bash
# Ensure DwarFS still works
ctest -R "dwarfs" --output-on-failure

# Test ZIP functionality
ctest -R "zip" --output-on-failure
```

### Stage 3: Multi-Language

#### Per-Language Testing

**Julia**:
```julia
# tests/julia/test_libtfs.jl
using Test
using LibTFS

@testset "LibTFS Julia Bindings" begin
    @test mount_filesystem("test.dwarfs", "/mnt/test")
    @test isfile("/mnt/test/file.txt")
    @test read("/mnt/test/file.txt") == "expected content"
end
```

**Python**:
```python
# tests/python/test_libtfs.py
import unittest
from libtfs import mount, read_file

class TestLibTFS(unittest.TestCase):
    def test_mount_dwarfs(self):
        mount("test.dwarfs", "/mnt/test")
        self.assertTrue(os.path.exists("/mnt/test/file.txt"))
```

**Node.js**:
```javascript
// tests/nodejs/test_libtfs.js
const assert = require('assert');
const libtfs = require('libtfs');

describe('LibTFS Node.js Bindings', function() {
  it('should mount DwarFS filesystem', function() {
    libtfs.mount('test.dwarfs', '/mnt/test');
    assert(fs.existsSync('/mnt/test/file.txt'));
  });
});
```

---

## Continuous Integration

### GitHub Actions Workflow

**File**: `.github/workflows/test.yml`

```yaml
name: Test Suite

on: [push, pull_request]

jobs:
  test:
    strategy:
      matrix:
        os: [ubuntu-22.04, macos-13, macos-14]
        build_type: [Debug, Release]

    runs-on: ${{ matrix.os }}

    steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Install Dependencies
      run: |
        if [ "$RUNNER_OS" == "Linux" ]; then
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build
        elif [ "$RUNNER_OS" == "macOS" ]; then
          brew install cmake ninja
        fi

    - name: Configure
      run: |
        cmake -B build -G Ninja \
          -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
          -DTEBAKO_BUILD=ON \
          -DDWARFS_WITH_FLATBUFFERS=ON \
          -DWITH_TESTS=ON

    - name: Build
      run: cmake --build build --parallel

    - name: Test
      run: |
        cd build
        ctest --output-on-failure --verbose

    - name: Upload Test Results
      if: failure()
      uses: actions/upload-artifact@v4
      with:
        name: test-results-${{ matrix.os }}-${{ matrix.build_type }}
        path: build/Testing/Temporary/
```

---

## Performance Testing

### Benchmarks

**File**: `tests/benchmarks/performance.cpp`

```cpp
// Mount/unmount timing
void benchmark_mount() {
    auto start = std::chrono::high_resolution_clock::now();
    mount_filesystem("large.dwarfs", "/mnt/test");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Mount time: " << duration.count() << "ms\n";
}

// File read performance
void benchmark_read() {
    // Read 10000 small files
    // Read 10 large files (100MB each)
    // Measure throughput
}
```

**Run**:
```bash
cd build
./tests/benchmarks/performance > perf-results.txt
```

**Track Over Time**:
- Mount/unmount latency
- File read throughput
- Memory usage
- Binary size

---

## Test Data

### Test Filesystems

**Location**: `tests/test_filesystem/`

**Contents**:
- Simple files (text, binary)
- Nested directories (5 levels deep)
- Large directory (90 files)
- Symlinks
- Empty directories

**Creation**:
```bash
# Build test DwarFS image
mkdwarfs -i tests/test_filesystem -o tests/test.dwarfs

# Build test ZIP
cd tests/test_filesystem && zip -r ../test.zip .
```

---

## Regression Prevention

### Baseline Capture
```bash
# Before Stage 1
cd build
ctest --output-on-failure > baseline-stage0.log 2>&1

# After Stage 1
ctest --output-on-failure > baseline-stage1.log 2>&1

# Compare
diff baseline-stage0.log baseline-stage1.log
# Expected: Only project name changes, no functionality changes
```

### Automated Regression Detection
```bash
# Script: detect-regression.sh
#!/bin/bash

BASELINE="$1"
CURRENT="$2"

# Extract test results
grep -oP '\d+/\d+ Test' "$BASELINE" > baseline-summary.txt
grep -oP '\d+/\d+ Test' "$CURRENT" > current-summary.txt

# Compare
if diff baseline-summary.txt current-summary.txt; then
    echo "✓ No regressions detected"
    exit 0
else
    echo "✗ Regression detected!"
    diff baseline-summary.txt current-summary.txt
    exit 1
fi
```

---

## Test Coverage

### Coverage Measurement
```bash
# Configure with coverage
cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage" \
    -DWITH_TESTS=ON

# Build and test
cmake --build build
cd build && ctest

# Generate report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --list coverage.info
```

### Coverage Goals
- **Unit Tests**: 80%+ line coverage
- **Functional Tests**: 90%+ API coverage
- **Integration Tests**: 100% critical paths

---

## Manual Testing Checklist

### Stage 1 (Rename)
- [ ] All unit tests pass
- [ ] Examples compile and run
- [ ] Headers found at new locations
- [ ] CMake project name correct
- [ ] No folly/thrift symbols in binary

### Stage 2 (ZIP Backend)
- [ ] DwarFS tests still pass
- [ ] ZIP mounting works
- [ ] ZIP file reading works
- [ ] Error handling correct
- [ ] No performance regression

### Stage 3 (Multi-Language)
- [ ] Ruby bindings work (existing)
- [ ] Julia bindings work (new)
- [ ] Python bindings work (new)
- [ ] Node.js bindings work (new)
- [ ] Cross-language consistency

---

## Test Maintenance

### When to Update Tests

1. **API Changes**: Update functional tests immediately
2. **New Features**: Add tests before feature code
3. **Bug Fixes**: Add regression test first
4. **Refactoring**: Ensure tests still pass

### Test Debt Management

- Review test coverage monthly
- Remove obsolete tests
- Refactor brittle tests
- Document test purposes

---

## Troubleshooting

### Common Test Failures

#### "Cannot find header"
```bash
# Solution: Update include paths
grep -r "include <tebako-" src/
# Replace with new paths
```

#### "Symbol not found"
```bash
# Solution: Check for folly/thrift remnants
nm -g build/libtfs.a | grep -iE "folly|thrift"
```

#### "Test timeout"
```bash
# Solution: Increase timeout in CTest
ctest --timeout 300
```

---

## Success Metrics

### Stage 1
- [ ] 100% tests passing
- [ ] Zero new warnings
- [ ] Same or better performance
- [ ] Clean static analysis

### Stage 2
- [ ] ZIP tests passing
- [ ] DwarFS tests still passing
- [ ] Performance acceptable
- [ ] Error paths covered

### Stage 3
- [ ] All language tests passing
- [ ] Consistent API across languages
- [ ] Documentation complete
- [ ] Integration stable

---

**Continuous testing ensures quality transformation!**

Last Updated: 2025-01-17