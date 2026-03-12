# Tebako Integration - Continuation Prompt

**Version**: 1.0
**Created**: 2025-12-31
**Phase**: Tebako Integration
**Estimated Duration**: 3 weeks (compressed)

---

## Current Status

### ✅ COMPLETED - libtfs v0.12.0

- **Test Coverage**: 230/230 tests passing (100%)
- **DwarFS Backend**: Fully functional with 70:1 compression
- **Extraction API**: Complete with metadata preservation
- **C API**: Production-ready for FFI integration
- **Documentation**: Comprehensive guides completed

### 🎯 NEXT PHASE - Tebako Integration

Integrate libtfs as Tebako's filesystem backend, replacing memfs with high-performance DwarFS/ZIP support.

---

## Objectives

### Primary Goals

1. **FFI Integration** - Create Ruby bindings for libtfs C API
2. **Core Class Patching** - Override File/Dir/IO for embedded FS
3. **Build Integration** - Link libtfs into Tebako executable
4. **Archive Embedding** - Embed DwarFS/ZIP archives in binary
5. **Validation** - Test with real Ruby applications

### Success Metrics

- ✅ Tebako builds with libtfs linked
- ✅ Ruby apps run from embedded DwarFS archives
- ✅ 30-50% smaller executables vs current ZIP
- ✅ No performance regression
- ✅ All Tebako tests pass

---

## Implementation Plan

### Week 1: Foundation (Days 1-5)

#### Day 1: Repository Analysis

**Tasks**:
1. Clone Tebako repository
2. Analyze current memfs implementation
3. Identify all filesystem integration points
4. Document current architecture

**Commands**:
```bash
git clone https://github.com/tamatebako/tebako.git
cd tebako

# Find filesystem-related code
find . -type f \( -name "*.rb" -o -name "*.c" -o -name "*.cpp" \) \
  -exec grep -l "memfs\|File\.\|Dir\.\|IO\." {} \; | head -20

# Analyze structure
tree -L 3 lib/
tree -L 3 src/
```

**Deliverables**:
- `docs/TEBAKO_CURRENT_ARCHITECTURE.md`
- List of files requiring modification
- Integration point mapping

#### Days 2-3: FFI Bindings Implementation

**File**: `lib/tebako/filesystem/bindings.rb`

See link:TEBAKO_INTEGRATION_PLAN.md#_2_1_create_ffi_bindings_module[Integration Plan Section 2.1]

**Test Command**:
```bash
cd tebako
bundle exec ruby -r ./lib/tebako/filesystem/bindings.rb \
  -e "puts Tebako::FileSystem::Bindings.methods.grep(/tebako/)"
```

**Validation**:
- All 20+ C API functions bound
- FFI library loads successfully
- Constants defined correctly
- Platform-specific handling works

#### Days 4-5: High-Level API & Tests

**Files**:
- `lib/tebako/filesystem/file_ops.rb`
- `lib/tebako/filesystem/dir_ops.rb`
- `test/filesystem/test_bindings.rb`

**Test Command**:
```bash
bundle exec ruby test/filesystem/test_bindings.rb
```

**Validation**:
- Object-oriented wrappers functional
- Error handling correct
- Memory safety verified

---

### Week 2: Core Integration (Days 1-5)

#### Days 1-2: Ruby Core Class Patches

**Files**:
- `lib/tebako/filesystem/file_patch.rb` - File class overrides
- `lib/tebako/filesystem/dir_patch.rb` - Dir class overrides
- `lib/tebako/filesystem/io_patch.rb` - IO class overrides

**Critical Methods**:
```ruby
# File class
File.read, File.open, File.exist?, File.size, File.stat

# Dir class
Dir.entries, Dir.foreach, Dir.glob, Dir.exist?

# Kernel
Kernel.load, Kernel.require
```

**Test Strategy**:
```ruby
# Test File.read
content = File.read('/__tebako__/test.txt')
assert_equal "expected", content

# Test require
require '/__tebako__/lib/mylib'
assert defined?(MyLib)

# Test Dir.glob
files = Dir.glob('/__tebako__/**/*.rb')
assert files.any?
```

#### Days 3-4: Build System Integration

**Modify**:
- `tebako/CMakeLists.txt` - Add libtfs dependency
- `tebako/vcpkg.json` - Add libtfs to dependencies

**Add**:
- Archive creation target
- Archive embedding (objcopy/ld)
- Platform-specific linking

**Test Command**:
```bash
cd tebako/build
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
ldd ./tebako | grep libtfs  # Verify linking
```

#### Day 5: Runtime Initialization

**File**: `lib/tebako/filesystem/init.rb`

**Entry Point Modification**:
```ruby
# In tebako executable entry point
require 'tebako/filesystem/init'
Tebako::FileSystem::Initializer.init!

# Then load application
require '/__tebako__/bin/app'
```

**Test with Simple App**:
```bash
# Create minimal test app
mkdir -p test_app/{lib,bin}
echo "puts 'Hello from Tebako!'" > test_app/bin/app

# Package
mkdwarfs -i test_app -o test.dwarfs

# Run
./tebako test.dwarfs
# Expected: "Hello from Tebako!"
```

---

### Week 3: Validation & Polish (Days 1-5)

#### Days 1-2: Integration Testing

**Create Test Apps**:
1. Simple Hello World
2. App with dependencies (gems)
3. App with data files (YAML/JSON)
4. App with ERB templates
5. App with executable scripts

**Test Each**:
```bash
# Package with DwarFS
tebako package --backend=dwarfs app/ -o app_dwarfs
./app_dwarfs --test

# Package with ZIP
tebako package --backend=zip app/ -o app_zip
./app_zip --test

# Compare
ls -lh app_dwarfs app_zip
# Expected: app_dwarfs 30-50% smaller
```

#### Days 3-4: Performance Validation

**Benchmark Suite**:
```ruby
# benchmark/tebako_comparison.rb
require 'benchmark/ips'

Benchmark.ips do |x|
  x.report("Current (memfs)") { run_with_memfs }
  x.report("DwarFS backend") { run_with_dwarfs }
  x.report("ZIP backend") { run_with_zip }

  x.compare!
end
```

**Metrics to Capture**:
- Startup time (cold start)
- File read throughput
- Random access latency
- Memory usage
- Executable size

#### Day 5: Documentation & Release

**Update**:
- Tebako README with new backend features
- Migration guide for existing users
- Troubleshooting section
- Performance comparison table

**Create**:
- Release notes for Tebako with libtfs
- Blog post announcing integration
- Example repository

---

## Technical Specifications

### FFI Library Loading

**Search Paths** (in order):
1. `ENV['TEBAKO_LIBTFS_PATH']` - User override
2. Tebako installation lib/ directory
3. System library paths (/usr/local/lib, /usr/lib)
4. Bundled with executable (static link)

```ruby
lib_paths = [
  ENV['TEBAKO_LIBTFS_PATH'],
  File.join(Tebako.install_dir, 'lib', 'libtfs'),
  'tfs',     # System search
  'libtfs'   # Alternate name
].compact

ffi_lib lib_paths
```

### Archive Format Selection

**Environment Variable**: `TEBAKO_BACKEND`

```bash
# Use DwarFS (default for production)
export TEBAKO_BACKEND=dwarfs
tebako package app/

# Use ZIP (for compatibility)
export TEBAKO_BACKEND=zip
tebako package app/

# Auto-detect from file
tebako run app.dwarfs  # Uses DwarFS
tebako run app.zip     # Uses ZIP
```

### Error Handling Strategy

**Principle**: Fail fast with clear error messages

```ruby
def initialize_filesystem
  result = Bindings.tebako_fs_init(data, size, mount_point)

  if result != 0
    errno = Bindings.tebako_get_errno
    case errno
    when Errno::EINVAL::Errno
      abort "Tebako: Invalid archive format or corrupted file"
    when Errno::ENOMEM::Errno
      abort "Tebako: Out of memory during filesystem initialization"
    when Errno::EIO::Errno
      abort "Tebako: I/O error reading embedded archive"
    else
      abort "Tebako: Unknown error (errno: #{errno})"
    end
  end
end
```

---

## Testing Strategy

### Unit Tests

Test FFI bindings in isolation:

```ruby
# test/filesystem/test_ffi_bindings.rb
class FFIBindingsTest < Minitest::Test
  def test_library_loads
    assert Tebako::FileSystem::Bindings.respond_to?(:tebako_fs_init)
  end

  def test_lifecycle_functions
    # Use minimal test archive
    result = Bindings.tebako_fs_init_from_file('test.dwarfs', '/test')
    assert_equal 0, result
    Bindings.tebako_fs_unmount
  end
end
```

### Integration Tests

Test with Ruby core classes:

```ruby
# test/integration/test_file_operations.rb
class FileOperationsTest < Minitest::Test
  def setup
    init_test_filesystem
  end

  def test_file_read
    content = File.read('/__tebako__/README.md')
    assert_kind_of String, content
  end

  def test_require_works
    require '/__tebako__/lib/test_module'
    assert defined?(TestModule)
  end
end
```

### End-to-End Tests

Test complete application packaging:

```ruby
# test/e2e/test_packaged_app.rb
class PackagedAppTest < Minitest::Test
  def test_rails_app_runs
    output = run_packaged_app('rails_app.dwarfs', ['--version'])
    assert_match /Rails \d+\.\d+/, output
  end

  def test_sinatra_app_responds
    output = run_packaged_app('sinatra_app.dwarfs', ['--test'])
    assert_includes output, "All tests passed"
  end
end
```

---

## Troubleshooting Guide

### Issue: FFI Library Not Found

```ruby
LoadError: Could not open library 'tfs'

# Solution 1: Set explicit path
export TEBAKO_LIBTFS_PATH=/usr/local/lib/libtfs.so

# Solution 2: Update LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Solution 3: Verify installation
find /usr -name "libtfs.*" 2>/dev/null
```

### Issue: Mount Fails with EINVAL

```bash
# Check archive integrity
dwarfsck app.dwarfs
unzip -t app.zip

# Verify format
file app.dwarfs  # Should show "DWARFS"
hexdump -C app.dwarfs | head -1  # Should start with "DWARFS"
```

### Issue: Segmentation Fault

**Causes**:
1. Memory buffer freed before unmount
2. Using file handle after unmount (use-after-free)
3. Thread safety violation

**Debug**:
```bash
# Run with Valgrind
valgrind --leak-check=full ./tebako_app

# Check for use-after-free
valgrind --track-origins=yes ./tebako_app
```

---

## Deliverables

### Code
- [ ] `lib/tebako/filesystem/bindings.rb` - FFI bindings
- [ ] `lib/tebako/filesystem/file_patch.rb` - File overrides
- [ ] `lib/tebako/filesystem/dir_patch.rb` - Dir overrides
- [ ] `lib/tebako/filesystem/init.rb` - Initialization
- [ ] Updated `CMakeLists.txt` - Build integration
- [ ] Updated `vcpkg.json` - Dependencies

### Tests
- [ ] `test/filesystem/test_bindings.rb` - FFI tests
- [ ] `test/integration/test_file_ops.rb` - File operation tests
- [ ] `test/e2e/test_packaging.rb` - End-to-end tests

### Documentation
- [ ] Updated Tebako README
- [ ] Migration guide
- [ ] Troubleshooting section
- [ ] Performance comparison

---

## Key Files Reference

### libtfs Side
- [`include/tebako/fs/c_api.h`](../include/tebako/fs/c_api.h) - C API header
- [`docs/TEBAKO_INTEGRATION_GUIDE.adoc`](TEBAKO_INTEGRATION_GUIDE.adoc) - Integration guide
- [`docs/TEBAKO_INTEGRATION_PLAN.md`](TEBAKO_INTEGRATION_PLAN.md) - This plan

### Tebako Side (to be created)
- `lib/tebako/filesystem/bindings.rb` - FFI bindings
- `lib/tebako/filesystem/init.rb` - Initialization
- `tebako/CMakeLists.txt` - Build configuration
- `tebako/vcpkg.json` - Dependencies

---

## Success Criteria

### Functional
- ✅ Packaged Ruby apps run without modification
- ✅ All Ruby file operations work transparently
- ✅ Gems load from embedded filesystem
- ✅ Executable scripts have correct permissions

### Performance
- ✅ 30-50% smaller executables with DwarFS
- ✅ <500ms startup time (no regression)
- ✅ 2-3x faster file access with DwarFS
- ✅ <100MB memory overhead

### Quality
- ✅ All existing Tebako tests pass
- ✅ No memory leaks (Valgrind clean)
- ✅ Thread-safe operations
- ✅ Cross-platform (Linux, macOS, Windows)

---

## START HERE

### Immediate Next Steps

1. **Clone Tebako Repository**
   ```bash
   cd ~/src
   git clone https://github.com/tamatebako/tebako.git
   cd tebako
   ```

2. **Analyze Current Implementation**
   - Read `lib/tebako.rb` entry point
   - Find memfs initialization code
   - Identify File/Dir patches
   - Document in `docs/TEBAKO_CURRENT_ARCHITECTURE.md`

3. **Create Status Tracker**
   - File: `docs/TEBAKO_INTEGRATION_STATUS.md`
   - Track progress on this plan

4. **Begin FFI Implementation**
   - Create `lib/tebako/filesystem/bindings.rb`
   - Test library loading
   - Bind essential functions first

---

## Contact & Resources

### Questions?
- Technical: See link:TEBAKO_INTEGRATION_GUIDE.adoc[Integration Guide]
- Architecture: See link:TEBAKO_INTEGRATION_PLAN.md[Integration Plan]
- API Reference: See link:../include/tebako/fs/c_api.h[C API Header]

### Examples
- link:../examples/ruby_integration_example.rb[Ruby Integration Example]
- link:TEBAKO_INTEGRATION_GUIDE.adoc#_example_implementation[Complete Implementation]

**Ready to begin integration! 🚀**