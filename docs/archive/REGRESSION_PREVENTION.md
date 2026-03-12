# Regression Prevention Strategy

## Overview

This document outlines the strategy to prevent regressions when removing the libfolly dependency from libdwarfs. It includes automated checks, code review guidelines, and monitoring strategies.

## Automated Checks

### 1. Pre-Commit Hooks

Create `.git/hooks/pre-commit`:

```bash
#!/bin/bash
# Pre-commit hook to prevent folly references

echo "Checking for folly references..."

# Check for folly includes in source code
FOLLY_INCLUDES=$(git diff --cached --name-only | \
  grep -E '\.(cpp|h|hpp)$' | \
  xargs grep -l '#include.*folly' 2>/dev/null || true)

if [ -n "$FOLLY_INCLUDES" ]; then
  echo "ERROR: Found folly includes in:"
  echo "$FOLLY_INCLUDES"
  echo "Please use tebako:: equivalents instead"
  exit 1
fi

# Check for folly namespace usage
FOLLY_USAGE=$(git diff --cached --name-only | \
  grep -E '\.(cpp|h|hpp)$' | \
  xargs grep -E 'folly::(Synchronized|to)<' 2>/dev/null || true)

if [ -n "$FOLLY_USAGE" ]; then
  echo "ERROR: Found folly namespace usage:"
  echo "$FOLLY_USAGE"
  echo "Please use tebako:: equivalents instead"
  exit 1
fi

echo "✓ No folly references found"
exit 0
```

### 2. CI/CD Pipeline

#### GitHub Actions Workflow

```yaml
name: Folly-Free Verification

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]

jobs:
  verify-no-folly:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Check for folly references
        run: |
          # Check source files
          if grep -r "folly::" src/ include/ --include="*.cpp" --include="*.h"; then
            echo "ERROR: Found folly:: references in code"
            exit 1
          fi

          # Check CMakeLists.txt
          if grep -i "folly" CMakeLists.txt | grep -v "#"; then
            echo "ERROR: Found folly references in CMakeLists.txt"
            exit 1
          fi

          echo "✓ No folly references found"

      - name: Build without folly
        run: |
          mkdir build && cd build
          cmake ..
          make -j$(nproc)

      - name: Run tests
        run: |
          cd build
          ctest --output-on-failure --timeout 300

      - name: Check binary for folly symbols
        run: |
          cd build
          # Check if any folly symbols are linked
          if nm wr-bin | grep -i folly; then
            echo "ERROR: Found folly symbols in binary"
            exit 1
          fi
          echo "✓ No folly symbols in binary"

  thread-safety:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install ThreadSanitizer
        run: |
          sudo apt-get update
          sudo apt-get install -y clang

      - name: Build with ThreadSanitizer
        run: |
          mkdir build && cd build
          CC=clang CXX=clang++ cmake \
            -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
            -DCMAKE_C_FLAGS="-fsanitize=thread -g -O1" \
            ..
          make -j$(nproc)

      - name: Run tests with ThreadSanitizer
        run: |
          cd build
          export TSAN_OPTIONS="halt_on_error=1 history_size=7"
          ctest --output-on-failure --timeout 600

  memory-safety:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install Valgrind
        run: |
          sudo apt-get update
          sudo apt-get install -y valgrind

      - name: Build for Valgrind
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Debug ..
          make -j$(nproc)

      - name: Run tests with Valgrind
        run: |
          cd build
          ctest -T memcheck --output-on-failure

  performance:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Build Release
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)

      - name: Run performance benchmarks
        run: |
          cd build
          # Add benchmark commands here
          echo "Running benchmarks..."
          # ./benchmark_suite --output=results.json

      - name: Check for regressions
        run: |
          # Compare with baseline
          echo "Checking for performance regressions..."
          # Add comparison logic
```

### 3. Code Quality Checks

#### Static Analysis

```bash
#!/bin/bash
# static-analysis.sh

echo "Running static analysis..."

# Clang-Tidy
clang-tidy src/*.cpp include/*.h \
  --checks='-*,concurrency-*,modernize-*,performance-*,readability-*' \
  --header-filter='include/tebako-.*\.h' \
  -- -std=c++17 -I include

# Cppcheck
cppcheck --enable=all \
  --inconclusive \
  --suppress=missingIncludeSystem \
  --suppress=unusedFunction \
  --error-exitcode=1 \
  src/ include/

# Include-what-you-use
iwyu_tool.py -p build/ src/*.cpp

echo "✓ Static analysis complete"
```

#### Code Coverage

```bash
#!/bin/bash
# check-coverage.sh

echo "Checking code coverage..."

# Build with coverage
mkdir -p build-coverage && cd build-coverage
cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_COVERAGE=ON ..
make -j$(nproc)

# Run tests
ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --remove coverage.info '*/tests/*' --output-file coverage.info

# Check coverage thresholds
COVERAGE=$(lcov --summary coverage.info 2>&1 | \
  grep "lines" | \
  awk '{print $2}' | \
  sed 's/%//')

echo "Line coverage: $COVERAGE%"

if (( $(echo "$COVERAGE < 80" | bc -l) )); then
  echo "ERROR: Coverage below 80%"
  exit 1
fi

echo "✓ Coverage acceptable"
```

## Code Review Checklist

### For Reviewers

When reviewing code changes related to folly removal:

- [ ] **No Folly References**
  - Check that no `#include <folly/*>` remains
  - Verify no `folly::` namespace usage
  - Confirm CMakeLists.txt has no folly dependencies

- [ ] **Correct Replacements**
  - `folly::Synchronized<T>` → `tebako::Synchronized<T>`
  - `folly::to<T>()` → `tebako::util::string_to<T>()`
  - Verify API compatibility maintained

- [ ] **Thread Safety**
  - Lock acquisition order correct
  - No potential deadlocks
  - Proper exception safety
  - RAII principles followed

- [ ] **Error Handling**
  - Exceptions handled correctly
  - Error messages clear and helpful
  - No resource leaks on error paths

- [ ] **Performance**
  - No obvious performance issues
  - Lock contention minimized
  - No unnecessary allocations

- [ ] **Tests**
  - Existing tests still pass
  - New code covered by tests
  - Edge cases tested

- [ ] **Documentation**
  - Code well-commented
  - API documentation updated
  - README reflects changes

## Monitoring Strategy

### 1. Build Monitoring

Track build metrics:
- **Build time** - Should not increase significantly
- **Binary size** - Should decrease (no folly overhead)
- **Dependency count** - Should decrease by 1 (folly)

### 2. Runtime Monitoring

Monitor in production (if applicable):
- **Performance metrics**
  - File operation latency
  - Lock contention
  - Memory usage

- **Error rates**
  - Conversion errors
  - Lock timeout errors
  - File operation failures

### 3. Crash Reporting

Set up crash reporting to catch:
- Segmentation faults
- Assertion failures
- Unhandled exceptions
- Thread deadlocks

## Backward Compatibility

### API Compatibility

Ensure the following remain stable:
- Public function signatures
- Return types
- Exception types
- Thread safety guarantees

### ABI Compatibility

Check for ABI changes:
```bash
# Before folly removal
nm -C libdwarfs-wr.a > symbols_before.txt

# After folly removal
nm -C libdwarfs-wr.a > symbols_after.txt

# Compare
diff symbols_before.txt symbols_after.txt
```

Expected changes:
- Removal of folly symbols ✓
- No changes to public API symbols ✓

## Testing Matrix

Test on all supported platforms:

| Platform | Compiler | C++ Std | Thread Sanitizer | Memory Check |
|----------|----------|---------|------------------|--------------|
| Ubuntu 20.04 | GCC 9 | C++17 | ✓ | ✓ |
| Ubuntu 22.04 | GCC 11 | C++20 | ✓ | ✓ |
| Ubuntu 22.04 | Clang 14 | C++17 | ✓ | ✓ |
| macOS 12 | AppleClang | C++17 | ✓ | ✓ |
| macOS 13 | AppleClang | C++20 | ✓ | ✓ |
| Windows | MSVC 2019 | C++17 | - | ✓ |
| Windows | MSVC 2022 | C++20 | - | ✓ |
| Alpine Linux | GCC | C++17 | ✓ | ✓ |

## Rollback Triggers

Automatically rollback if:
1. Build fails on any platform
2. More than 5% of tests fail
3. Memory leaks detected
4. Thread sanitizer errors
5. Performance regression > 20%
6. Critical bug reported

## Long-Term Maintenance

### 1. Prevent Reintroduction

Add to CI:
```bash
# Fail if folly ever gets added back
if grep -r "folly" CMakeLists.txt vcpkg.json; then
  echo "ERROR: folly dependency detected"
  exit 1
fi
```

### 2. Keep Replacements Updated

Monitor for:
- C++ standard library improvements
- Better mutex implementations
- Performance optimizations

### 3. Documentation

Maintain:
- Why folly was removed
- How replacements work
- Migration guide for similar projects

## Success Metrics

Track over time:
- **Build success rate** - Should be ≥99%
- **Test pass rate** - Should be 100%
- **Performance** - Should be within ±5% of baseline
- **Memory usage** - Should decrease or stay same
- **Bug reports** - Should not increase

## Review Checklist Summary

Before declaring success:

**Code Quality**
- [ ] No compiler warnings
- [ ] No static analysis issues
- [ ] Code coverage ≥80%
- [ ] All tests passing

**Performance**
- [ ] Build time acceptable
- [ ] Runtime performance within 5%
- [ ] Memory usage acceptable
- [ ] No lock contention issues

**Safety**
- [ ] No thread safety issues
- [ ] No memory leaks
- [ ] Proper error handling
- [ ] Exception safety verified

**Documentation**
- [ ] Code documented
- [ ] README updated
- [ ] Migration guide complete
- [ ] Testing plan executed

**Testing**
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Thread safety verified
- [ ] Platform compatibility confirmed

## Escalation Path

If issues found:

1. **Minor Issues** (< 3 test failures)
   - Fix immediately
   - Add regression test
   - Continue

2. **Moderate Issues** (thread safety warnings)
   - Pause rollout
   - Investigate thoroughly
   - Fix and retest
   - Continue if resolved

3. **Major Issues** (crashes, data corruption)
   - Stop immediately
   - Revert changes
   - Root cause analysis
   - Fix in development
   - Full retest required

## Sign-Off

**Prevention Strategy Reviewed By:** _____________
**Date:** _____________
**Approved:** _____________