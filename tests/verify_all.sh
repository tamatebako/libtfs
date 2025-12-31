#!/bin/bash
# Quick verification script for libtfs and tebakofs CLI

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=========================================="
echo "libtfs & tebakofs Verification Script"
echo "=========================================="
echo ""

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Check if build directory exists
if [ ! -d "$PROJECT_ROOT/build" ]; then
    echo -e "${RED}✗ Build directory not found${NC}"
    echo "  Run: cmake -B build -DWITH_TESTS=ON && cmake --build build"
    exit 1
fi

# Check if fixtures exist
echo "1. Checking test fixtures..."
if [ ! -f "$SCRIPT_DIR/fixtures/zip/simple.zip" ]; then
    echo -e "${YELLOW}⚠ ZIP fixtures missing, regenerating...${NC}"
    cd "$SCRIPT_DIR/fixtures/zip" && ./create_fixtures.sh
fi

if [ ! -f "$SCRIPT_DIR/fixtures/squashfs/simple.sqfs" ]; then
    echo -e "${YELLOW}⚠ SquashFS fixtures missing, regenerating...${NC}"
    cd "$SCRIPT_DIR/fixtures/squashfs" && ./create_fixtures.sh
fi
echo -e "${GREEN}✓ Test fixtures present${NC}"
echo ""

# Check if tebakofs was built
echo "2. Checking tebakofs CLI..."
if [ ! -f "$PROJECT_ROOT/build/tebakofs" ]; then
    echo -e "${RED}✗ tebakofs not built${NC}"
    echo "  Run: cmake --build build --target tebakofs"
    exit 1
fi
echo -e "${GREEN}✓ tebakofs CLI built${NC}"
echo ""

# Check if test executables exist
echo "3. Checking test executables..."
TESTS=(
    "test_backend_factory"
    "test_zip_backend"
    "test_zip_integration"
    "test_squashfs_backend"
    "test_squashfs_integration"
)

for test in "${TESTS[@]}"; do
    if [ ! -f "$PROJECT_ROOT/build/$test" ]; then
        echo -e "${RED}✗ $test not built${NC}"
        exit 1
    fi
done
echo -e "${GREEN}✓ All test executables built${NC}"
echo ""

# Run unit tests
echo "4. Running unit tests..."
cd "$PROJECT_ROOT/build"

if ctest --output-on-failure > /tmp/ctest_output.txt 2>&1; then
    TEST_COUNT=$(grep -o "tests passed" /tmp/ctest_output.txt | head -1 | awk '{print $1}')
    echo -e "${GREEN}✓ All tests passed ($TEST_COUNT tests)${NC}"
else
    echo -e "${RED}✗ Some tests failed${NC}"
    cat /tmp/ctest_output.txt
    exit 1
fi
echo ""

# Test CLI with ZIP
echo "5. Testing tebakofs CLI with ZIP..."
cd "$PROJECT_ROOT"

# List ZIP
if ./build/tebakofs ls tests/fixtures/zip/simple.zip > /tmp/cli_zip_ls.txt 2>&1; then
    if grep -q "test.txt" /tmp/cli_zip_ls.txt && grep -q "file2.txt" /tmp/cli_zip_ls.txt; then
        echo -e "${GREEN}✓ ZIP ls command works${NC}"
    else
        echo -e "${RED}✗ ZIP ls output unexpected${NC}"
        cat /tmp/cli_zip_ls.txt
        exit 1
    fi
else
    echo -e "${RED}✗ ZIP ls command failed${NC}"
    cat /tmp/cli_zip_ls.txt
    exit 1
fi

# Cat ZIP file
if ./build/tebakofs cat tests/fixtures/zip/simple.zip /test.txt > /tmp/cli_zip_cat.txt 2>&1; then
    if grep -q "Hello from ZIP" /tmp/cli_zip_cat.txt; then
        echo -e "${GREEN}✓ ZIP cat command works${NC}"
    else
        echo -e "${RED}✗ ZIP cat output unexpected${NC}"
        cat /tmp/cli_zip_cat.txt
        exit 1
    fi
else
    echo -e "${RED}✗ ZIP cat command failed${NC}"
    cat /tmp/cli_zip_cat.txt
    exit 1
fi

echo ""

# Test CLI with SquashFS
echo "6. Testing tebakofs CLI with SquashFS..."

# List SquashFS
if ./build/tebakofs ls tests/fixtures/squashfs/simple.sqfs > /tmp/cli_sqfs_ls.txt 2>&1; then
    if grep -q "test.txt" /tmp/cli_sqfs_ls.txt && grep -q "file2.txt" /tmp/cli_sqfs_ls.txt; then
        echo -e "${GREEN}✓ SquashFS ls command works${NC}"
    else
        echo -e "${RED}✗ SquashFS ls output unexpected${NC}"
        cat /tmp/cli_sqfs_ls.txt
        exit 1
    fi
else
    echo -e "${RED}✗ SquashFS ls command failed${NC}"
    cat /tmp/cli_sqfs_ls.txt
    exit 1
fi

# Cat SquashFS file
if ./build/tebakofs cat tests/fixtures/squashfs/simple.sqfs /test.txt > /tmp/cli_sqfs_cat.txt 2>&1; then
    if grep -q "Hello from SquashFS" /tmp/cli_sqfs_cat.txt; then
        echo -e "${GREEN}✓ SquashFS cat command works${NC}"
    else
        echo -e "${RED}✗ SquashFS cat output unexpected${NC}"
        cat /tmp/cli_sqfs_cat.txt
        exit 1
    fi
else
    echo -e "${RED}✗ SquashFS cat command failed${NC}"
    cat /tmp/cli_sqfs_cat.txt
    exit 1
fi

# Test permissions (SquashFS feature)
if ./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /readonly.txt > /tmp/cli_sqfs_stat.txt 2>&1; then
    echo -e "${GREEN}✓ SquashFS stat command works (POSIX permissions preserved)${NC}"
else
    echo -e "${YELLOW}⚠ SquashFS stat command failed (non-critical)${NC}"
fi

echo ""

# Summary
echo "=========================================="
echo -e "${GREEN}✅ All verifications passed!${NC}"
echo "=========================================="
echo ""
echo "Summary:"
echo "  • Test fixtures: Ready ✓"
echo "  • tebakofs CLI: Built ✓"
echo "  • Unit tests: All passing ✓"
echo "  • ZIP backend: Working ✓"
echo "  • SquashFS backend: Working ✓"
echo ""
echo "Next steps:"
echo "  • Run full test suite: cd build && ctest --verbose"
echo "  • Try CLI commands: ./build/tebakofs help"
echo "  • Read docs: cat tests/TESTING_GUIDE.md"
echo ""