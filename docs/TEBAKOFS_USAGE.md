# tebakofs CLI - Usage Guide

## Quick Start

### Building tebakofs

```bash
# Set vcpkg environment
export VCPKG_ROOT=/path/to/vcpkg

# Clean build (recommended first time)
./build_tebakofs.sh clean

# Or incremental build
./build_tebakofs.sh
```

The script will:
1. Check vcpkg environment
2. Configure with CMake
3. Build tebakofs binary
4. Output location: `build/tebakofs`

### Running tebakofs

```bash
cd build
./tebakofs help
```

## Complete Command Reference

### 1. List Directory Contents (`ls`)

```bash
# List root directory
./tebakofs ls archive.zip

# List specific directory
./tebakofs ls archive.dwarfs /subdir

# Recursive listing
./tebakofs ls -r archive.squashfs

# Long format (permissions, size, date)
./tebakofs ls -l archive.zip

# Recursive + long format
./tebakofs ls -rl archive.zip /path
```

**Options:**
- `-r, --recursive` - List recursively
- `-l, --long` - Show permissions, size, modification time
- `-v, --verbose` - Verbose output
- `-q, --quiet` - Quiet output

### 2. Show Archive Information (`info`)

```bash
# Show archive statistics
./tebakofs info archive.zip

# Verbose info
./tebakofs info -v archive.dwarfs
```

**Output:**
- Archive path and type
- File count
- Directory count
- Total uncompressed size

### 3. Display File Contents (`cat`)

```bash
# Print file to stdout
./tebakofs cat archive.zip /README.txt

# Pipe to other commands
./tebakofs cat archive.zip /data.json | jq .
```

### 4. Show Directory Tree (`tree`)

```bash
# Show entire tree
./tebakofs tree archive.zip

# Show subtree
./tebakofs tree archive.dwarfs /subdir
```

**Output:** Visual tree structure with indentation

### 5. Show File Metadata (`stat`)

```bash
# Show file metadata
./tebakofs stat archive.zip /file.txt

# Show directory metadata
./tebakofs stat archive.squashfs /
```

**Output:**
- File type (file/directory)
- Size (for files)
- Modification time

### 6. Extract Files (`extract`)

```bash
# Extract entire archive to current directory
./tebakofs extract archive.zip

# Extract to specific directory
./tebakofs extract -d /tmp/output archive.zip

# Extract specific files
./tebakofs extract archive.zip file1.txt dir/file2.txt

# Extract with destination
./tebakofs extract -d /tmp/out archive.dwarfs /path/to/file.txt

# Verbose extraction
./tebakofs extract -v archive.zip
```

**Options:**
- `-d, --dest <dir>` - Destination directory (default: current)
- `-v, --verbose` - Show extraction progress
- `-q, --quiet` - Suppress output

### 7. Search for Files (`find`)

```bash
# Find all .txt files
./tebakofs find archive.zip "*.txt"

# Find by exact name
./tebakofs find archive.dwarfs "README.md"

# Complex patterns
./tebakofs find archive.zip "test_*.cpp"
```

**Pattern:** Uses glob patterns (*, ?, [])

### 8. Help (`help`)

```bash
# General help
./tebakofs help

# Command-specific help
./tebakofs help ls
./tebakofs help extract
```

## Supported Archive Formats

tebakofs auto-detects format from magic bytes and works with:

| Format | Extensions | Status |
|--------|-----------|--------|
| ZIP | `.zip`, `.jar`, `.apk`, `.war`, `.ear` | ✅ Full support |
| DwarFS | `.dwarfs`, `.dfs` | ✅ Full support |
| SquashFS | `.squashfs`, `.sqfs` | ✅ Full support |

## Examples

### Example 1: Inspect Archive

```bash
# Get archive info
./tebakofs info my-app.zip

# List root contents
./tebakofs ls my-app.zip

# List with details
./tebakofs ls -l my-app.zip

# Show as tree
./tebakofs tree my-app.zip
```

### Example 2: Extract Specific Files

```bash
# Extract just documentation
./tebakofs extract -d /tmp/docs my-app.zip README.md LICENSE

# Extract a directory
./tebakofs extract -d /tmp/src my-app.zip /src/
```

### Example 3: Search and Extract

```bash
# Find all JSON files
./tebakofs find config.zip "*.json"

# Extract them
./tebakofs extract -d /tmp/json config.zip package.json tsconfig.json
```

### Example 4: Pipeline Usage

```bash
# Read and process JSON
./tebakofs cat data.dwarfs /config.json | jq '.version'

# Search and count
./tebakofs find source.zip "*.cpp" | wc -l

# Extract and verify
./tebakofs extract archive.zip && echo "Extraction complete"
```

## Integration with Other Tools

### With jq (JSON processing)

```bash
./tebakofs cat archive.zip /package.json | jq '.dependencies'
```

### With grep (text search)

```bash
./tebakofs cat archive.zip /log.txt | grep ERROR
```

### With wc (word count)

```bash
./tebakofs cat archive.zip /document.txt | wc -l
```

## Common Workflows

### Workflow 1: Archive Inspection

```bash
# 1. Get overview
./tebakofs info archive.zip

# 2. See structure
./tebakofs tree archive.zip

# 3. List specific directory
./tebakofs ls -l archive.zip /src

# 4. Check specific file
./tebakofs stat archive.zip /src/main.cpp
```

### Workflow 2: Selective Extraction

```bash
# 1. Find files
./tebakofs find archive.zip "*.h"

# 2. Extract headers only
./tebakofs extract -d /tmp/headers archive.zip $(./tebakofs find archive.zip "*.h")
```

### Workflow 3: Content Verification

```bash
# 1. List all files
./tebakofs ls -r archive.zip > file_list.txt

# 2. Check specific files exist
./tebakofs stat archive.zip /critical/file.dat

# 3. Verify content
./tebakofs cat archive.zip /version.txt
```

## Error Handling

### Common Errors

**Archive not found:**
```
Error: Failed to open archive: /path/to/archive.zip
       Unsupported format or file does not exist
```
**Solution:** Check file path and format

**Path doesn't exist:**
```
Error: Path does not exist: /nonexistent
```
**Solution:** Use `ls` to find correct path

**Permission denied:**
```
Error: Failed to create output file: /restricted/path
```
**Solution:** Check destination directory permissions

## Performance Tips

1. **Use specific paths**: Instead of `ls -r`, use `ls /specific/dir`
2. **Extract selectively**: Extract only needed files instead of entire archive
3. **Pipe efficiently**: Use `cat` with pipes instead of extracting temporary files

## Environment Variables

```bash
# Set vcpkg root (required for building)
export VCPKG_ROOT=/path/to/vcpkg

# Set library path (if needed at runtime)
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH  # Linux
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH  # macOS
```

## Installation

### Local Build (Already Complete)

```bash
cd build
./tebakofs  # Run from build directory
```

### System-Wide Installation

```bash
cd build
sudo cmake --install . --component tebakofs
# Now available as 'tebakofs' from anywhere
```

### Add to PATH (Alternative)

```bash
# Add to ~/.bashrc or ~/.zshrc
export PATH="/path/to/libdwarfs/build:$PATH"

# Reload shell
source ~/.bashrc  # or ~/.zshrc
```

## Troubleshooting

### Build Issues

**vcpkg not found:**
```bash
export VCPKG_ROOT=/path/to/vcpkg
./build_tebakofs.sh clean
```

**Link errors:**
```bash
# Rebuild from scratch
rm -rf build
./build_tebakofs.sh
```

### Runtime Issues

**Shared library not found:**
```bash
# Linux
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# macOS
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
```

**Archive format not recognized:**
- Ensure file has correct extension
- Check file is not corrupted: `file archive.zip`

## See Also

- [`README.adoc`](../README.adoc) - Main project documentation
- [`VCPKG_INTEGRATION.md`](VCPKG_INTEGRATION.md) - vcpkg integration guide
- [`examples/vcpkg_example/`](../examples/vcpkg_example/) - Example application