#!/bin/bash
set -e

# Check for mksquashfs
if ! command -v mksquashfs &> /dev/null; then
    echo "Error: mksquashfs not found. Please install squashfs-tools."
    exit 1
fi

# 1. simple.sqfs - Basic functionality
echo "Creating simple.sqfs..."
mkdir -p simple
echo "Hello from SquashFS!" > simple/test.txt
echo "Second file" > simple/file2.txt
mksquashfs simple simple.sqfs -noappend -quiet
rm -rf simple

# 2. nested.sqfs - Directory structure
echo "Creating nested.sqfs..."
mkdir -p nested/dir1/subdir nested/dir2
echo "File 1" > nested/dir1/file1.txt
echo "File 2" > nested/dir1/subdir/file2.txt
echo "File 3" > nested/dir2/file3.txt
mksquashfs nested nested.sqfs -noappend -quiet
rm -rf nested

# 3. empty.sqfs - Edge cases
echo "Creating empty.sqfs..."
mkdir -p empty/empty_dir
touch empty/empty_file.txt
mksquashfs empty empty.sqfs -noappend -quiet
rm -rf empty

# 4. permissions.sqfs - POSIX permissions (SquashFS advantage)
echo "Creating permissions.sqfs..."
mkdir -p perms
echo "Read-only file" > perms/readonly.txt
chmod 444 perms/readonly.txt
echo "Executable script" > perms/script.sh
chmod 755 perms/script.sh
echo "Private file" > perms/private.txt
chmod 600 perms/private.txt
mkdir -p perms/restricted_dir
chmod 700 perms/restricted_dir
mksquashfs perms permissions.sqfs -noappend -quiet
rm -rf perms

# 5. large.sqfs - Performance testing
echo "Creating large.sqfs..."
mkdir -p large/many_files
dd if=/dev/urandom of=large/large.txt bs=1M count=10 2>/dev/null
for i in {1..100}; do echo "File $i" > "large/many_files/file$i.txt"; done
mksquashfs large large.sqfs -noappend -quiet
rm -rf large

# 6. corrupted.sqfs - Error handling
echo "Creating corrupted.sqfs..."
cp simple.sqfs corrupted.sqfs
dd if=/dev/zero of=corrupted.sqfs bs=1 count=100 seek=100 conv=notrunc 2>/dev/null

echo "All SquashFS test fixtures created successfully!"
ls -lh ./*.sqfs