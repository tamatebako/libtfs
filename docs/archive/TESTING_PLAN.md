# Testing Plan: libfolly Removal

## Overview

This document outlines the comprehensive testing strategy to ensure the removal of libfolly dependency does not introduce regressions or break existing functionality.

## Pre-Testing Checklist

- [x] All source code changes completed
- [x] Build system updated (CMakeLists.txt)
- [x] Documentation created
- [ ] Code compiles successfully
- [ ] All unit tests pass
- [ ] No performance regressions
- [ ] Thread safety verified

## Phase 1: Compilation Testing

### 1.1 Clean Build Test

**Purpose:** Verify the code compiles without folly

**Steps:**
```bash
# Clean any previous builds
rm -rf build

# Create fresh build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Check for any folly-related errors in CMake output
# Expected: No references to folly in output

# Compile
make -j$(nproc)

# Check compilation status
echo $?  # Should be 0 (success)
```

**Success Criteria:**
- ✅ CMake configuration succeeds without errors
- ✅ No folly-related include errors
- ✅ All source files compile successfully
- ✅ No linker errors related to missing folly symbols
- ✅ All test executables build successfully

**What to Look For:**
- ❌ `error: 'folly' has not been declared`
- ❌ `undefined reference to 'folly::`
- ❌ Missing header file errors for folly includes
- ❌ CMake errors about missing folly package

### 1.2 Verify No Folly References

**Purpose:** Ensure no folly code remains in the build

**Steps:**
```bash
# Search for folly in build output
grep -r "folly" build/ 2>/dev/null | grep -v "Binary file"

# Expected: Only references in comments or documentation
# Should NOT see: actual folly library files or symbols
```

**Success Criteria:**
- ✅ No folly libraries in build artifacts
- ✅ No folly symbols in object files
- ✅ No folly includes in generated files

## Phase 2: Unit Testing

### 2.1 Run Existing Test Suite

**Purpose:** Verify all existing tests still pass

**Steps:**
```bash
cd build

# Run all tests with verbose output
ctest --output-on-failure --verbose

# Run tests with specific patterns if needed
ctest -R "memfs" --output-on-failure
ctest -R "sync" --output-on-failure
ctest -R "conversion" --output-on-failure
```

**Success Criteria:**
- ✅ All tests pass (100% pass rate)
- ✅ No new failures compared to pre-change baseline
- ✅ No timeout issues
- ✅ No segmentation faults or crashes

**Critical Test Areas:**
1. **Synchronization Tests**
   - Multi-threaded file operations
   - Concurrent directory traversal
   - Lock contention scenarios

2. **Conversion Tests**
   - String to double conversion
   - String to size_t conversion
   - String to file_off_t conversion
   - Edge cases (overflow, underflow, invalid input)

3. **File Operations**
   - Open/close operations
   - Read/write operations
   - Directory operations

### 2.2 Specific Feature Tests

**Test 1: Synchronized<T> Functionality**
```cpp
// Test multi-threaded access
// File: tests/test-synchronized.cpp (if exists)
// Verify:
// - Multiple readers can access simultaneously
// - Writers get exclusive access
// - No deadlocks
// - No race conditions
```

**Test 2: String Conversions**
```cpp
// Test edge cases
// - Empty strings
// - Invalid formats
// - Overflow/underflow values
// - Negative numbers
// - Scientific notation (for double)
```

**Test 3: File Descriptor Table**
```cpp
// Test concurrent operations on fd table
// - Multiple threads opening files
// - Simultaneous read operations
// - Concurrent close operations
```

## Phase 3: Thread Safety Testing

### 3.1 Race Condition Detection

**Purpose:** Ensure no race conditions introduced

**Tools:**
- ThreadSanitizer (if available)
- Valgrind --tool=helgrind
- Custom multi-threaded stress tests

**Steps:**
```bash
# Build with ThreadSanitizer
cd build
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..
make clean && make -j$(nproc)

# Run tests with thread sanitizer
export TSAN_OPTIONS="halt_on_error=1"
ctest --output-on-failure

# Check for any thread sanitizer warnings
```

**Success Criteria:**
- ✅ No data races detected
- ✅ No deadlock warnings
- ✅ No memory ordering issues
- ✅ Proper lock acquisition order

### 3.2 Stress Testing

**Purpose:** Verify system under heavy concurrent load

**Test Scenarios:**
1. **High Concurrency File Access**
   - 100+ threads reading different files
   - Mixed read/write operations
   - Rapid open/close cycles

2. **Lock Contention**
   - Multiple threads accessing same synchronized objects
   - Verify no performance degradation
   - Check lock fairness

3. **Long-Running Operations**
   - Keep system running for extended period
   - Monitor for memory leaks
   - Check for lock starvation

## Phase 4: Performance Testing

### 4.1 Benchmark Critical Paths

**Purpose:** Ensure no performance regression

**Metrics to Measure:**
1. **File Open/Close Performance**
   ```bash
   # Measure time for 10000 file operations
   time ./benchmark_file_ops 10000
   ```

2. **Directory Traversal**
   ```bash
   # Measure directory listing performance
   time ./benchmark_directory_scan
   ```

3. **Concurrent Access**
   ```bash
   # Measure multi-threaded file access
   time ./benchmark_concurrent_access
   ```

**Success Criteria:**
- ✅ Performance within 5% of baseline (folly version)
- ✅ No significant outliers
- ✅ Consistent performance across runs

**Baseline Comparison:**
| Operation | With Folly | Without Folly | Delta |
|-----------|------------|---------------|-------|
| File Open (1000x) | X ms | Y ms | Z% |
| Dir Scan (large) | X ms | Y ms | Z% |
| Concurrent R/W | X ms | Y ms | Z% |

### 4.2 Memory Usage

**Purpose:** Verify no memory leaks or excessive usage

**Steps:**
```bash
# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./wr-tests

# Check for leaks
# Expected: No leaks or only known/acceptable leaks
```

**Success Criteria:**
- ✅ No new memory leaks
- ✅ Memory usage similar to folly version
- ✅ No excessive allocations

## Phase 5: Integration Testing

### 5.1 Real-World Scenarios

**Test 1: Mount DwarFS Filesystem**
```bash
# Test mounting actual DwarFS archive
# Verify:
# - Successful mount
# - File access works
# - Directory listing correct
# - Unmount clean
```

**Test 2: Large File Operations**
```bash
# Test with large files
# Verify:
# - Reading large files works
# - No buffer overflows
# - Proper error handling
```

**Test 3: Symbolic Links**
```bash
# Test symbolic link handling
# Verify:
# - Link resolution works
# - Circular link detection
# - Cross-filesystem links
```

### 5.2 Error Handling

**Test Error Paths:**
1. **Invalid Conversions**
   - Non-numeric strings
   - Out of range values
   - Null pointers

2. **Lock Failures**
   - Verify proper exception handling
   - Check resource cleanup

3. **File System Errors**
   - Missing files
   - Permission errors
   - Corrupted archives

## Phase 6: Regression Prevention

### 6.1 Continuous Integration

**Add CI Tests:**
```yaml
# .github/workflows/test-no-folly.yml
name: Test Without Folly

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build
        run: |
          mkdir build && cd build
          cmake ..
          make -j$(nproc)
      - name: Test
        run: |
          cd build
          ctest --output-on-failure
```

### 6.2 Static Analysis

**Run Static Analyzers:**
```bash
# Clang-tidy
clang-tidy src/*.cpp include/*.h \
  --checks='-*,modernize-*,performance-*,concurrency-*'

# Cppcheck
cppcheck --enable=all --inconclusive \
  --suppress=missingIncludeSystem \
  src/ include/
```

**Success Criteria:**
- ✅ No new warnings
- ✅ No threading issues detected
- ✅ No performance anti-patterns

### 6.3 Code Coverage

**Ensure Test Coverage:**
```bash
# Build with coverage
cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_COVERAGE=ON ..
make -j$(nproc)

# Run tests
ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

**Success Criteria:**
- ✅ Coverage maintained or improved
- ✅ New code (tebako-synchronized.h, tebako-conversions.h) covered
- ✅ Critical paths tested

## Phase 7: Documentation

### 7.1 Update Documentation

**Files to Update:**
- [ ] README.md - Update build instructions if needed
- [ ] CHANGELOG.md - Document the change
- [ ] Migration guide (if needed for external users)

### 7.2 Code Comments

**Verify:**
- [ ] New headers have adequate documentation
- [ ] Complex logic explained
- [ ] Thread safety guarantees documented

## Test Execution Checklist

### Before Merging

- [ ] All compilation tests pass
- [ ] All unit tests pass (100%)
- [ ] Thread sanitizer clean
- [ ] No memory leaks
- [ ] Performance within acceptable range
- [ ] Static analysis clean
- [ ] Code coverage maintained
- [ ] Documentation updated
- [ ] CI tests pass

### Post-Merge Monitoring

- [ ] Monitor for bug reports
- [ ] Check production metrics
- [ ] Verify no performance degradation
- [ ] Review any crash reports

## Known Acceptable Changes

These changes are expected and acceptable:
1. Different lock implementation (folly → std::shared_mutex)
2. Different conversion error messages
3. Minor performance variations (±5%)

## Red Flags to Watch For

These would indicate a problem:
1. ❌ Segmentation faults
2. ❌ Deadlocks
3. ❌ Data races
4. ❌ Memory leaks
5. ❌ Performance regression >10%
6. ❌ Test failures
7. ❌ Compiler warnings

## Rollback Plan

If critical issues are found:
1. Revert all commits related to folly removal
2. Document the issue
3. Fix the problem
4. Re-test thoroughly
5. Re-apply the changes

## Sign-Off

**Testing Complete When:**
- [ ] All tests pass
- [ ] No regressions found
- [ ] Performance acceptable
- [ ] Documentation complete
- [ ] Code reviewed
- [ ] CI/CD updated

**Tested By:** _____________
**Date:** _____________
**Results:** _____________