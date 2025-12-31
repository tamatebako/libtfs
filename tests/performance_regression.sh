#!/bin/bash
set -e

echo "=== Performance Regression Test Suite ==="
cd /Users/mulgogi/src/tamatebako/libdwarfs/build

FAILURES=0

# Test 1: Mount Time Regression
echo "Test 1: Mount Time (Target: < 10ms)"
# Run test_zip_backend mount tests and measure time
START=$(date +%s%N)
./test_zip_backend --gtest_filter=*Mount* > /dev/null 2>&1 || true
END=$(date +%s%N)
DURATION=$(( (END - START) / 1000000 ))
if [ $DURATION -gt 15 ]; then
  echo "  ❌ REGRESSION: Mount time ${DURATION}ms exceeds 15ms threshold"
  FAILURES=$((FAILURES + 1))
else
  echo "  ✅ PASS: Mount time ${DURATION}ms"
fi

# Test 2: Test Suite Time Regression
echo "Test 2: Full Test Suite (Target: < 30s)"
START=$(date +%s)
ctest --output-on-failure > /dev/null 2>&1 || true
END=$(date +%s)
DURATION=$((END - START))
if [ $DURATION -gt 40 ]; then
  echo "  ❌ REGRESSION: Test suite ${DURATION}s exceeds 40s threshold"
  FAILURES=$((FAILURES + 1))
else
  echo "  ✅ PASS: Test suite ${DURATION}s"
fi

# Test 3: Binary Size Regression
echo "Test 3: Binary Sizes (Target: < 1MB each)"
for binary in test_backend_factory test_zip_backend test_zip_integration test_c_api; do
  if [ -f ./$binary ]; then
    SIZE=$(stat -f%z ./$binary 2>/dev/null || stat -c%s ./$binary 2>/dev/null)
    SIZE_KB=$((SIZE / 1024))
    if [ $SIZE_KB -gt 1024 ]; then
      echo "  ❌ REGRESSION: $binary ${SIZE_KB}KB exceeds 1MB"
      FAILURES=$((FAILURES + 1))
    else
      echo "  ✅ PASS: $binary ${SIZE_KB}KB"
    fi
  else
    echo "  ⚠️  WARNING: $binary not found, skipping"
  fi
done

# Summary
echo ""
echo "=== Performance Regression Summary ==="
if [ $FAILURES -eq 0 ]; then
  echo "✅ All performance tests PASSED"
  exit 0
else
  echo "❌ $FAILURES performance test(s) FAILED"
  exit 1
fi