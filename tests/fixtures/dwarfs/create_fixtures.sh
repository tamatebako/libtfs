#!/bin/bash
set -e

# Check for mkdwarfs
if ! command -v mkdwarfs &> /dev/null; then
    echo "Error: mkdwarfs not found. Please install dwarfs."
    echo "On macOS: brew install dwarfs"
    echo "On Ubuntu: apt-get install dwarfs"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="../../test_data/dwarfs_source"

cd "$SCRIPT_DIR"

# 1. simple.dwarfs - Basic functionality
echo "Creating simple.dwarfs..."
if [ -d "$SOURCE_DIR/simple" ]; then
    mkdwarfs -i "$SOURCE_DIR/simple" \
             -o simple.dwarfs \
             -l 9 \
             --no-progress \
             --force
    echo "✓ simple.dwarfs created"
else
    echo "✗ Source directory $SOURCE_DIR/simple not found"
    exit 1
fi

# 2. nested.dwarfs - Deep directory structure
echo "Creating nested.dwarfs..."
if [ -d "$SOURCE_DIR/nested" ]; then
    mkdwarfs -i "$SOURCE_DIR/nested" \
             -o nested.dwarfs \
             -l 9 \
             --no-progress \
             --force
    echo "✓ nested.dwarfs created"
else
    echo "✗ Source directory $SOURCE_DIR/nested not found"
    exit 1
fi

# 3. permissions.dwarfs - POSIX permissions (DwarFS advantage)
echo "Creating permissions.dwarfs..."
if [ -d "$SOURCE_DIR/permissions" ]; then
    mkdwarfs -i "$SOURCE_DIR/permissions" \
             -o permissions.dwarfs \
             -l 9 \
             --no-progress \
             --force
    echo "✓ permissions.dwarfs created"
else
    echo "✗ Source directory $SOURCE_DIR/permissions not found"
    exit 1
fi

# 4. large.dwarfs - Performance testing
# The source blobs (*.bin, ~11M) are gitignored; generate them on the fly like
# the zip/squashfs fixture scripts do instead of failing on a clean checkout.
echo "Creating large.dwarfs..."
LARGE_SRC="$SOURCE_DIR/large"
LARGE_TMP=""
if [ ! -d "$LARGE_SRC" ]; then
    LARGE_TMP="$(mktemp -d)"
    dd if=/dev/urandom of="$LARGE_TMP/1mb.bin" bs=1M count=1 2>/dev/null
    dd if=/dev/urandom of="$LARGE_TMP/10mb.bin" bs=1M count=10 2>/dev/null
    LARGE_SRC="$LARGE_TMP"
fi
mkdwarfs -i "$LARGE_SRC" \
         -o large.dwarfs \
         -l 9 \
         --no-progress \
         --force
if [ -n "$LARGE_TMP" ]; then
    rm -rf "$LARGE_TMP"
fi
echo "✓ large.dwarfs created"

# 5. empty.dwarfs - Edge case testing
# git cannot track an empty directory; create it if it is missing.
echo "Creating empty.dwarfs..."
mkdir -p "$SOURCE_DIR/empty"
if [ -d "$SOURCE_DIR/empty" ]; then
    mkdwarfs -i "$SOURCE_DIR/empty" \
             -o empty.dwarfs \
             --no-progress \
             --force
    echo "✓ empty.dwarfs created"
else
    echo "✗ Source directory $SOURCE_DIR/empty not found"
    exit 1
fi

# 6. corrupted.dwarfs - Error handling (corrupt a copy of simple.dwarfs)
echo "Creating corrupted.dwarfs..."
if [ -f "simple.dwarfs" ]; then
    cp simple.dwarfs corrupted.dwarfs
    # Corrupt the magic bytes
    dd if=/dev/zero of=corrupted.dwarfs bs=1 count=100 seek=100 conv=notrunc 2>/dev/null
    echo "✓ corrupted.dwarfs created"
else
    echo "✗ simple.dwarfs not found for corruption"
    exit 1
fi

echo ""
echo "All DwarFS test fixtures created successfully!"
ls -lh ./*.dwarfs