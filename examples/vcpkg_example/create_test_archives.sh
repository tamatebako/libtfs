#!/bin/bash
set -e

# Create test archives directory
mkdir -p test_archives
cd test_archives

# Create sample content
mkdir -p sample_content
echo "Hello from libtfs example!" > sample_content/README.txt
echo "This is a test file." > sample_content/test.txt
mkdir -p sample_content/subdir
echo "Nested file content" > sample_content/subdir/nested.txt

# Create DwarFS archive
if command -v mkdwarfs &> /dev/null; then
    mkdwarfs -i sample_content -o sample.dwarfs --no-progress
    echo "Created sample.dwarfs"
else
    echo "mkdwarfs not found, skipping DwarFS archive creation"
fi

# Create ZIP archive
zip -r sample.zip sample_content/
echo "Created sample.zip"

# Create SquashFS archive (if available)
if command -v mksquashfs &> /dev/null; then
    mksquashfs sample_content sample.squashfs -noappend
    echo "Created sample.squashfs"
else
    echo "mksquashfs not found, skipping SquashFS archive creation"
fi

# Cleanup
rm -rf sample_content

echo ""
echo "Test archives created successfully!"
ls -lh