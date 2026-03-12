#!/bin/bash
set -e

# 1. simple.zip - Basic functionality
echo "Creating simple.zip..."
mkdir -p simple
printf 'Hello from ZIP\x21\n' > simple/test.txt
echo "Second file" > simple/file2.txt
(cd simple && zip -q ../simple.zip test.txt file2.txt)
rm -rf simple

# 2. nested.zip - Directory structure
echo "Creating nested.zip..."
mkdir -p nested/dir1/subdir nested/dir2
echo "File 1" > nested/dir1/file1.txt
echo "File 2" > nested/dir1/subdir/file2.txt
echo "File 3" > nested/dir2/file3.txt
(cd nested && zip -q -r ../nested.zip *)
rm -rf nested

# 3. empty.zip - Edge cases
echo "Creating empty.zip..."
mkdir -p empty/empty_dir
touch empty/empty_file.txt
(cd empty && zip -q -r ../empty.zip *)
rm -rf empty

# 4. large.zip - Performance testing
echo "Creating large.zip..."
mkdir -p large/many_files
dd if=/dev/urandom of=large/large.txt bs=1M count=10 2>/dev/null
for i in {1..100}; do echo "File $i" > large/many_files/file$i.txt; done
(cd large && zip -q -r ../large.zip *)
rm -rf large

# 5. corrupted.zip - Error handling
echo "Creating corrupted.zip..."
cp simple.zip corrupted.zip
dd if=/dev/zero of=corrupted.zip bs=1 count=100 seek=100 conv=notrunc 2>/dev/null

echo "All test fixtures created successfully!"
ls -lh *.zip
