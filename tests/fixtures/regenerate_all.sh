#!/bin/bash
set -e

echo "=================================================="
echo "Regenerating All Test Fixtures"
echo "=================================================="
echo ""

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Track success/failure
FAILED=0

# Regenerate ZIP fixtures
echo ">>> Regenerating ZIP fixtures..."
if cd "$SCRIPT_DIR/zip" && ./create_fixtures.sh; then
    echo "✅ ZIP fixtures regenerated successfully"
else
    echo "❌ ZIP fixtures regeneration failed"
    FAILED=1
fi
echo ""

# Regenerate SquashFS fixtures
echo ">>> Regenerating SquashFS fixtures..."
if cd "$SCRIPT_DIR/squashfs" && ./create_fixtures.sh; then
    echo "✅ SquashFS fixtures regenerated successfully"
else
    echo "❌ SquashFS fixtures regeneration failed"
    FAILED=1
fi
echo ""

# Summary
echo "=================================================="
if [ $FAILED -eq 0 ]; then
    echo "✅ All fixtures regenerated successfully!"
    echo ""
    echo "Fixture summary:"
    echo "  ZIP fixtures:      $(find "$SCRIPT_DIR/zip" -maxdepth 1 -name '*.zip' -print | wc -l) archives"
    echo "  SquashFS fixtures: $(find "$SCRIPT_DIR/squashfs" -maxdepth 1 -name '*.sqfs' -print | wc -l) archives"
    echo ""
    echo "Total size:"
    du -sh "$SCRIPT_DIR/zip" "$SCRIPT_DIR/squashfs" 2>/dev/null | awk '{print "  "$2": "$1}'
    echo ""
    echo "Next steps:"
    echo "  1. Build the project: cmake --build build"
    echo "  2. Run tests: cd build && ctest --verbose"
    echo "  3. Test CLI: ./build/tebakofs ls tests/fixtures/zip/simple.zip"
else
    echo "❌ Some fixtures failed to regenerate"
    echo ""
    echo "Check error messages above for details."
    echo "Ensure required tools are installed:"
    echo "  - zip (for ZIP fixtures)"
    echo "  - mksquashfs (for SquashFS fixtures)"
    exit 1
fi
echo "=================================================="