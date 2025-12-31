# Test Fixtures

This directory contains test fixtures for validating the libtfs backends and tebakofs CLI tool.

## Structure

```
tests/fixtures/
├── README.md              # This file
├── regenerate_all.sh      # Script to regenerate all fixtures
├── zip/                   # ZIP backend fixtures
│   ├── README.md          # ZIP fixtures documentation
│   ├── create_fixtures.sh # Generate ZIP test archives
│   ├── simple.zip         # Basic functionality (2 files)
│   ├── nested.zip         # Directory hierarchy
│   ├── empty.zip          # Empty files/directories
│   ├── large.zip          # Performance testing (10 MB + 100 files)
│   └── corrupted.zip      # Error handling
└── squashfs/              # SquashFS backend fixtures
    ├── README.md          # SquashFS fixtures documentation
    ├── create_fixtures.sh # Generate SquashFS test archives
    ├── simple.sqfs        # Basic functionality (2 files)
    ├── nested.sqfs        # Directory hierarchy
    ├── empty.sqfs         # Empty files/directories
    ├── permissions.sqfs   # POSIX permissions (444, 755, 600, 700)
    ├── large.sqfs         # Performance testing (10 MB + 100 files)
    └── corrupted.sqfs     # Error handling
```

## Regenerating Fixtures

To regenerate all test fixtures:

```bash
cd tests/fixtures
./regenerate_all.sh
```

To regenerate specific backend fixtures:

```bash
# ZIP fixtures only
cd tests/fixtures/zip
./create_fixtures.sh

# SquashFS fixtures only
cd tests/fixtures/squashfs
./create_fixtures.sh
```

## Prerequisites

### For ZIP Fixtures
- `zip` command (usually pre-installed)
- `dd` command (for creating test data)

### For SquashFS Fixtures
- `mksquashfs` command from squashfs-tools
  - Ubuntu/Debian: `sudo apt-get install squashfs-tools`
  - macOS: `brew install squashfs`
  - Fedora/RHEL: `sudo dnf install squashfs-tools`

## Using Fixtures

### In Unit Tests

The fixtures are automatically copied to the build directory:

```cpp
// In tests
const std::string fixtures_path = "tests/fixtures/zip/";
auto backend = BackendFactory::create_zip();
backend->mount(fixtures_path + "simple.zip", "/mnt/test");
```

### With tebakofs CLI

```bash
# List contents
./build/tebakofs ls tests/fixtures/zip/simple.zip
./build/tebakofs ls tests/fixtures/squashfs/simple.sqfs

# Show info
./build/tebakofs info tests/fixtures/zip/nested.zip
./build/tebakofs info tests/fixtures/squashfs/nested.sqfs

# Display file
./build/tebakofs cat tests/fixtures/zip/simple.zip /test.txt
./build/tebakofs cat tests/fixtures/squashfs/simple.sqfs /test.txt

# Show tree
./build/tebakofs tree tests/fixtures/zip/nested.zip
./build/tebakofs tree tests/fixtures/squashfs/nested.sqfs

# Test permissions (SquashFS only)
./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /readonly.txt
./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /script.sh
```

## Fixture Details

### simple.zip / simple.sqfs
- **Purpose**: Basic read/write operations
- **Contents**: 
  - `test.txt`: "Hello from [FORMAT]!\n"
  - `file2.txt`: "Second file\n"

### nested.zip / nested.sqfs
- **Purpose**: Directory traversal and nested paths
- **Contents**:
  - `dir1/file1.txt`
  - `dir1/subdir/file2.txt`
  - `dir2/file3.txt`

### empty.zip / empty.sqfs
- **Purpose**: Edge cases (empty files/directories)
- **Contents**:
  - `empty_file.txt` (0 bytes)
  - `empty_dir/` (empty directory)

### permissions.sqfs (SquashFS only)
- **Purpose**: POSIX permissions testing
- **Contents**:
  - `readonly.txt` (444 permissions)
  - `script.sh` (755 permissions)
  - `private.txt` (600 permissions)
  - `restricted_dir/` (700 permissions)

### large.zip / large.sqfs
- **Purpose**: Performance testing
- **Contents**:
  - `large.txt` (10 MB random data)
  - `many_files/file1.txt` through `file100.txt`

### corrupted.zip / corrupted.sqfs
- **Purpose**: Error handling
- **Contents**: Deliberately corrupted archive data

## Testing Workflow

### 1. Generate Fixtures

```bash
cd tests/fixtures
./regenerate_all.sh
```

### 2. Build Project with Tests

```bash
cmake -B build -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### 3. Run Tests

```bash
# All tests
cd build && ctest --verbose

# Backend-specific tests
ctest -R test_zip_backend --verbose
ctest -R test_squashfs_backend --verbose

# Integration tests
ctest -R test_zip_integration --verbose
ctest -R test_squashfs_integration --verbose
```

### 4. Manual CLI Testing

```bash
# Test ZIP backend
./build/tebakofs info tests/fixtures/zip/simple.zip
./build/tebakofs ls -rl tests/fixtures/zip/nested.zip
./build/tebakofs extract tests/fixtures/zip/simple.zip /tmp/test-extract

# Test SquashFS backend
./build/tebakofs info tests/fixtures/squashfs/simple.sqfs
./build/tebakofs ls -rl tests/fixtures/squashfs/nested.sqfs
./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /readonly.txt
./build/tebakofs extract tests/fixtures/squashfs/simple.sqfs /tmp/test-extract

# Performance test
time ./build/tebakofs cat tests/fixtures/squashfs/large.sqfs /large.txt > /dev/null
```

## Continuous Integration

The fixtures are regenerated in CI pipelines to ensure consistency. See `.github/workflows/` for CI configuration.

## Troubleshooting

### "mksquashfs: command not found"
Install squashfs-tools for your platform (see Prerequisites above).

### "zip: command not found"
Install zip utility:
- Ubuntu/Debian: `sudo apt-get install zip`
- macOS: Usually pre-installed
- RHEL/Fedora: `sudo dnf install zip`

### Fixtures not found during tests
Ensure fixtures are copied to build directory:
```bash
cmake --build build
# Fixtures automatically copied via CMakeLists.txt
```

### Permission errors with SquashFS fixtures
Some platforms may not preserve all permission bits. This is expected behavior and test assertions account for platform differences.

## Adding New Fixtures

To add a new test fixture:

1. Update the appropriate `create_fixtures.sh` script
2. Document the fixture in this README
3. Add tests that use the new fixture
4. Update `regenerate_all.sh` if needed
5. Commit both the script and generated fixture

## Maintenance

Fixtures should be regenerated:
- After changing fixture creation scripts
- Before major releases
- When adding new test scenarios
- If fixture corruption is suspected

Always commit both the creation scripts and the generated fixtures to the repository.