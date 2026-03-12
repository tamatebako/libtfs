# Tebako Integration Implementation Plan

**Version**: 1.0
**Created**: 2025-12-31
**Status**: Ready to Begin
**Timeline**: 2-3 weeks (compressed schedule)

---

## Executive Summary

Integrate libtfs v0.12.0 as the filesystem backend for Tebako, replacing the current memfs implementation with high-performance DwarFS/ZIP support.

**Key Benefits:**
- 70% smaller executables using DwarFS compression
- 100x faster random file access (native seek)
- POSIX permission support for executable scripts
- Unified API supporting multiple archive formats

---

## Phase 1: Architecture Analysis (Week 1, Days 1-2)

### Objective
Understand Tebako's current filesystem architecture and identify all integration points.

### Tasks

#### 1.1: Analyze Tebako Repository Structure

```bash
# Clone and analyze Tebako
git clone https://github.com/tamatebako/tebako.git
cd tebako

# Identify key files
find . -name "*memfs*" -o -name "*fs*" -o -name "*vfs*" | grep -v node_modules
find . -name "*.rb" -path "*/lib/*" | xargs grep -l "File\|Dir\|IO"
```

**Deliverables:**
- Document current memfs implementation architecture
- List all filesystem integration points
- Identify Ruby core class patches
- Map tebako-specific filesystem requirements

#### 1.2: Create Integration Architecture Document

**File**: `docs/TEBAKO_LIBTFS_ARCHITECTURE.adoc`

**Contents:**
```
┌─────────────────────────────────────┐
│    Tebako Ruby Application          │
│    (Single Executable)               │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    Ruby Core Class Patches          │
│  • File.read / File.open             │
│  • Dir.entries / Dir.glob            │
│  • IO.read / IO.binread              │
│  • Kernel.load / Kernel.require      │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    Tebako::FileSystem Module        │
│  • Path routing logic                │
│  • Embedded vs native dispatch       │
│  • Error handling wrapper            │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    Ruby FFI Bindings                │
│    (tebako/filesystem/bindings.rb)  │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    libtfs C API                      │
│    (libtfs.so / libtfs.dylib)       │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    Backend Factory                  │
│    (Auto-detect: DwarFS/ZIP)        │
└───────┬──────────────┬───────────────┘
        │              │
   ┌────▼────┐    ┌────▼────┐
   │ DwarFS  │    │   ZIP   │
   │ Backend │    │ Backend │
   └─────────┘    └─────────┘
```

**Deliverables:**
- Architecture diagram
- Integration points documented
- API contract specified
- Error handling strategy

#### 1.3: Define FFI Binding Requirements

Analyze what C API functions Tebako needs:

**Essential Functions:**
```ruby
# Lifecycle
tebako_fs_init_from_file(archive_path, mount_point)
tebako_fs_init(data, size, mount_point)  # For embedded archives
tebako_fs_unmount()

# File Operations
tebako_open(path, flags)
tebako_read(fd, buffer, count)
tebako_lseek(fd, offset, whence)
tebako_close(fd)

# Directory Operations
tebako_opendir(path)
tebako_readdir(dir)
tebako_closedir(dir)

# Metadata
tebako_stat(path, statbuf)
tebako_fstat(fd, statbuf)

# Utilities
tebako_path_is_embedded(path)
tebako_get_errno()
```

**Deliverables:**
- Complete function mapping
- Data structure definitions (struct stat, struct dirent)
- Platform-specific considerations (Windows vs Unix)

---

## Phase 2: FFI Implementation (Week 1, Days 3-5)

### Objective
Create Ruby FFI bindings and core filesystem module.

### Tasks

#### 2.1: Create FFI Bindings Module

**File**: `lib/tebako/filesystem/bindings.rb`

```ruby
require 'ffi'

module Tebako
  module FileSystem
    module Bindings
      extend FFI::Library

      # Platform-specific library loading
      ffi_lib_flags :now, :global
      lib_paths = [
        'tfs',                    # Unix-style
        'libtfs',                 # Alternate
        'libtfs.so',              # Linux explicit
        'libtfs.dylib',           # macOS explicit
        'tfs.dll'                 # Windows
      ]

      begin
        ffi_lib lib_paths
      rescue LoadError => e
        # Try from Tebako installation directory
        tebako_lib = File.join(Tebako.install_dir, 'lib', FFI::Platform::LIBPREFIX + 'tfs' + '.' + FFI::Platform::LIBSUFFIX)
        ffi_lib tebako_lib
      end

      # Define all C API functions (20+ functions)
      # ... (as documented in TEBAKO_INTEGRATION_GUIDE.adoc)

      # Platform-specific constants
      case FFI::Platform::OS
      when 'darwin', 'linux'
        O_RDONLY = 0x0000
        SEEK_SET = 0
        # ... Unix constants
      when 'windows'
        O_RDONLY = 0x0000
        # ... Windows constants
      end
    end
  end
end
```

**Test with:**
```bash
ruby -r ./lib/tebako/filesystem/bindings.rb -e "puts Tebako::FileSystem::Bindings.methods"
```

#### 2.2: Create High-Level API Wrapper

**File**: `lib/tebako/filesystem/api.rb`

Implement object-oriented wrapper over FFI:

```ruby
module Tebako
  module FileSystem
    class EmbeddedFile
      def initialize(path, mode = 'r')
        @path = path
        @fd = Bindings.tebako_open(path, Bindings::O_RDONLY)
        raise Errno::ENOENT, path if @fd < 0
      end

      def read(length = nil)
        # Implementation
      end

      def seek(offset, whence = IO::SEEK_SET)
        # Implementation
      end

      def close
        Bindings.tebako_close(@fd) if @fd >= 0
        @fd = -1
      end
    end

    class EmbeddedDir
      def initialize(path)
        @path = path
        @handle = Bindings.tebako_opendir(path)
        raise Errno::ENOENT, path if @handle.null?
      end

      def each
        # Implementation
      end

      def close
        Bindings.tebako_closedir(@handle) unless @handle.null?
      end
    end
  end
end
```

#### 2.3: Create Test Suite for Bindings

**File**: `test/filesystem/test_bindings.rb`

```ruby
require 'minitest/autorun'
require 'tebako/filesystem/bindings'

class TebakoFSBindingsTest < Minitest::Test
  def setup
    # Create test archive
    @archive = create_test_dwarfs_archive
    @mount_point = '/__tebako_test__'
  end

  def test_lifecycle
    result = Tebako::FileSystem::Bindings.tebako_fs_init_from_file(
      @archive, @mount_point
    )
    assert_equal 0, result, "Failed to initialize filesystem"

    initialized = Tebako::FileSystem::Bindings.tebako_is_initialized
    assert_equal 1, initialized

    Tebako::FileSystem::Bindings.tebako_fs_unmount
  end

  # ... more tests
end
```

---

## Phase 3: Ruby Core Class Integration (Week 2, Days 1-3)

### Objective
Patch Ruby's File, Dir, and IO classes to use libtfs for embedded files.

### Tasks

#### 3.1: Implement File Class Patches

**File**: `lib/tebako/filesystem/file_patch.rb`

**Strategy**: Use `prepend` for clean method interception

```ruby
module Tebako
  module FileSystem
    module FilePatch
      def read(path, *args, **kwargs)
        if embedded?(path)
          EmbeddedFile.new(path).read
        else
          super
        end
      end

      def open(path, mode = 'r', *args, **kwargs, &block)
        if embedded?(path)
          file = EmbeddedFile.new(path, mode)
          return file unless block_given?

          begin
            yield file
          ensure
            file.close
          end
        else
          super
        end
      end

      def exist?(path)
        embedded?(path) ? embedded_exist?(path) : super
      end

      private

      def embedded?(path)
        Bindings.tebako_path_is_embedded(path) == 1
      end

      def embedded_exist?(path)
        stat_buf = FFI::MemoryPointer.new(:char, 144)
        Bindings.tebako_stat(path, stat_buf) == 0
      end
    end

    # Apply patch
    File.singleton_class.prepend(FilePatch)
  end
end
```

**Methods to Override:**
- `File.read`, `File.binread`
- `File.open`
- `File.exist?`, `File.file?`, `File.directory?`
- `File.size`, `File.mtime`
- `File.readable?`, `File.executable?`
- `File.stat`

#### 3.2: Implement Dir Class Patches

**File**: `lib/tebako/filesystem/dir_patch.rb`

```ruby
module Tebako
  module FileSystem
    module DirPatch
      def entries(path, encoding: Encoding::UTF_8)
        if embedded?(path)
          embedded_entries(path)
        else
          super
        end
      end

      def glob(pattern, flags = 0, base: nil, &block)
        # Complex: Need to handle patterns like "*.rb"
        # in embedded filesystem
        if pattern.start_with?(mount_point)
          embedded_glob(pattern, flags, &block)
        else
          super
        end
      end

      private

      def embedded_entries(path)
        dir = Bindings.tebako_opendir(path)
        return [] if dir.null?

        entries = []
        loop do
          entry = Bindings.tebako_readdir(dir)
          break if entry.null?
          entries << entry.read_string
        end
        Bindings.tebako_closedir(dir)
        entries
      end
    end

    Dir.singleton_class.prepend(DirPatch)
  end
end
```

**Methods to Override:**
- `Dir.entries`, `Dir.children`
- `Dir.foreach`, `Dir.each_child`
- `Dir.glob`
- `Dir.exist?`, `Dir.empty?`

#### 3.3: Implement IO Class Patches

**File**: `lib/tebako/filesystem/io_patch.rb`

For `IO.read`, `IO.binread`, and `Kernel.load`:

```ruby
module Kernel
  alias_method :original_load, :load

  def load(filename, wrap = false)
    if Tebako::FileSystem.embedded?(filename)
      content = Tebako::FileSystem::FileOps.read(filename)
      eval(content, TOPLEVEL_BINDING, filename)
    else
      original_load(filename, wrap)
    end
  end
end
```

#### 3.4: Create Integration Tests

**File**: `test/filesystem/test_ruby_integration.rb`

```ruby
class TebakoRubyIntegrationTest < Minitest::Test
  def setup
    # Initialize with test archive containing Ruby files
    @archive = create_ruby_app_archive
    Tebako::FileSystem.initialize(archive_path: @archive)
  end

  def test_file_read_works
    content = File.read('/__tebako__/lib/myapp.rb')
    assert_includes content, "class MyApp"
  end

  def test_require_works
    require '/__tebako__/lib/myapp'
    assert defined?(MyApp)
  end

  def test_dir_glob_works
    files = Dir.glob('/__tebako__/**/*.rb')
    assert files.any?
  end
end
```

---

## Phase 4: Tebako Build Integration (Week 2, Days 4-5)

### Objective
Modify Tebako's build system to link libtfs and embed archives.

### Tasks

#### 4.1: Update Tebako's CMakeLists.txt

**Location**: Tebako repository

```cmake
# Add libtfs dependency
find_package(libtfs CONFIG REQUIRED)

# Link to tebako executable
target_link_libraries(tebako PRIVATE
  libtfs::tfs
  Ruby::Ruby
)

# Ensure dynamic symbol export (for FFI)
if(UNIX AND NOT APPLE)
  target_link_options(tebako PRIVATE
    "-Wl,--export-dynamic"
    "-Wl,--whole-archive"
  )
endif()

# Add custom target to create archive
add_custom_target(create_app_archive
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bundle
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/app
          ${CMAKE_BINARY_DIR}/bundle
  COMMAND mkdwarfs -i ${CMAKE_BINARY_DIR}/bundle
                   -o ${CMAKE_BINARY_DIR}/app.dwarfs
                   --compression=zstd:level=22
  COMMENT "Creating DwarFS archive from application"
)

# Embed archive in executable
if(UNIX AND NOT APPLE)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/app_archive.o
    COMMAND objcopy --input binary --output elf64-x86-64
            --binary-architecture i386
            ${CMAKE_BINARY_DIR}/app.dwarfs
            ${CMAKE_BINARY_DIR}/app_archive.o
    DEPENDS create_app_archive
  )
elseif(APPLE)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/app_archive.o
    COMMAND ld -r -o ${CMAKE_BINARY_DIR}/app_archive.o
            -sectcreate __DATA __archive
            ${CMAKE_BINARY_DIR}/app.dwarfs
    DEPENDS create_app_archive
  )
endif()

# Link archive object
target_sources(tebako PRIVATE ${CMAKE_BINARY_DIR}/app_archive.o)
```

#### 4.2: Update vcpkg Dependencies

**File**: `tebako/vcpkg.json`

```json
{
  "name": "tebako",
  "version-string": "0.9.0",
  "dependencies": [
    "libtfs",
    "ruby",
    "yaml-cpp"
  ],
  "builtin-baseline": "latest"
}
```

#### 4.3: Create Archive Embedding Utilities

**File**: `lib/tebako/packager/archive_embedder.rb`

```ruby
module Tebako
  module Packager
    class ArchiveEmbedder
      def self.embed(source_dir, archive_type: :dwarfs)
        case archive_type
        when :dwarfs
          create_dwarfs_archive(source_dir)
        when :zip
          create_zip_archive(source_dir)
        else
          raise ArgumentError, "Unsupported archive type"
        end
      end

      private

      def self.create_dwarfs_archive(source_dir)
        output = "#{source_dir}.dwarfs"
        cmd = [
          'mkdwarfs',
          '-i', source_dir,
          '-o', output,
          '--compression=zstd:level=22',
          '--file-hash=xxh3-128'
        ]

        system(*cmd) or raise "Failed to create DwarFS archive"
        output
      end
    end
  end
end
```

---

## Phase 5: Runtime Integration (Week 3, Days 1-3)

### Objective
Initialize libtfs at Tebako startup and handle embedded archive mounting.

### Tasks

#### 5.1: Create Initialization Module

**File**: `lib/tebako/filesystem/init.rb`

```ruby
module Tebako
  module FileSystem
    class Initializer
      # Initialize embedded filesystem
      def self.init!
        @mount_point = ENV['TEBAKO_MOUNT_POINT'] || '/__tebako__'

        if ENV['TEBAKO_EXTRACT_ON_INIT']
          # Extract to temporary directory on startup
          init_with_extraction!
        else
          # Mount embedded archive directly
          init_embedded!
        end

        setup_exit_handler
        true
      rescue => e
        warn "Tebako filesystem initialization failed: #{e.message}"
        false
      end

      private

      def self.init_embedded!
        # Get embedded archive from linker symbols
        archive_ptr = get_embedded_archive_pointer
        archive_size = get_embedded_archive_size

        result = Bindings.tebako_fs_init(
          archive_ptr,
          archive_size,
          @mount_point
        )

        raise "Mount failed: errno #{Bindings.tebako_get_errno}" if result != 0

        backend = Bindings.tebako_get_backend_name
        $stderr.puts "Tebako: Mounted with #{backend} backend at #{@mount_point}"
      end

      def self.init_with_extraction!
        # Extract to temp directory for faster access
        extract_dir = Dir.mktmpdir('tebako-')

        init_embedded!
        Bindings.tebako_fs_extract_all(extract_dir)
        Bindings.tebako_fs_unmount

        # Update load path to extracted directory
        $LOAD_PATH.unshift(File.join(extract_dir, 'lib'))
        @extract_dir = extract_dir
      end

      def self.get_embedded_archive_pointer
        # Access linker-provided symbols
        # Platform-specific implementation
        case FFI::Platform::OS
        when 'darwin', 'linux'
          # Use DL or Fiddle to access _binary_app_dwarfs_start
          require 'fiddle'
          sym_start = Fiddle::Handle::DEFAULT['_binary_app_dwarfs_start']
          FFI::Pointer.new(sym_start)
        else
          raise "Platform not supported for embedded archives"
        end
      end

      def self.setup_exit_handler
        at_exit do
          Bindings.tebako_fs_unmount if Bindings.tebako_is_initialized == 1
          FileUtils.rm_rf(@extract_dir) if @extract_dir
        end
      end
    end
  end
end
```

#### 5.2: Update Tebako Entry Point

**File**: `lib/tebako/runner.rb` (or equivalent)

```ruby
module Tebako
  class Runner
    def self.run(argv = ARGV)
      # Initialize filesystem BEFORE loading any application code
      unless FileSystem::Initializer.init!
        abort "Tebako: Failed to initialize embedded filesystem"
      end

      # Update load path
      $LOAD_PATH.unshift('/__tebako__/lib')

      # Load application entry point
      app_entry = ENV['TEBAKO_ENTRY_POINT'] || '/__tebako__/bin/app'
      require app_entry

      # Run application
      if defined?(App) && App.respond_to?(:run)
        App.run(argv)
      else
        warn "No App.run method found"
      end
    end
  end
end

# Run if this is the main script
Tebako::Runner.run if __FILE__ == $PROGRAM_NAME
```

#### 5.3: Create Diagnostic Tool

**File**: `lib/tebako/filesystem/diagnostics.rb`

```ruby
module Tebako
  module FileSystem
    class Diagnostics
      def self.report
        puts "\n=== Tebako Filesystem Diagnostics ==="

        if Bindings.tebako_is_initialized == 1
          puts "Status: ✓ Initialized"
          puts "Backend: #{Bindings.tebako_get_backend_name}"
          puts "Mount Point: #{Bindings.tebako_get_mount_point}"

          # List root directory
          puts "\nRoot Contents:"
          Dir.entries('/__tebako__').each do |entry|
            next if entry == '.' || entry == '..'
            path = File.join('/__tebako__', entry)
            type = File.directory?(path) ? '[DIR]' : '[FILE]'
            size = File.directory?(path) ? '' : "(#{File.size(path)} bytes)"
            puts "  #{type} #{entry} #{size}"
          end
        else
          puts "Status: ✗ Not initialized"
          puts "Error: #{Bindings.tebako_get_errno}"
        end

        puts "=================================\n"
      end
    end
  end
end
```

---

## Phase 6: Testing & Validation (Week 3, Days 4-5)

### Objective
Validate integration with real Ruby applications.

### Tasks

#### 6.1: Create Test Ruby Application

**Structure**:
```
test_app/
├── bin/
│   └── myapp
├── lib/
│   ├── myapp.rb
│   └── myapp/
│       ├── core.rb
│       └── utils.rb
├── data/
│   ├── config.yml
│   └── templates/
│       └── default.erb
└── Gemfile
```

**Test scenarios**:
- Loading Ruby files from embedded FS
- Reading data files (YAML, JSON, text)
- Using ERB templates
- Accessing bundled gems
- Requiring nested modules

#### 6.2: Integration Test Suite

**File**: `test/integration/test_real_app.rb`

```ruby
class RealAppIntegrationTest < Minitest::Test
  def test_full_application_lifecycle
    # Package test app
    archive = package_test_app

    # Run tebako executable
    output = run_tebako_app(archive, ['--help'])
    assert_includes output, "Usage:"

    # Verify file operations
    output = run_tebako_app(archive, ['--list-files'])
    assert_includes output, "lib/myapp.rb"
  end

  def test_gem_loading
    # Test bundled gems work
    archive = package_app_with_gems
    output = run_tebako_app(archive, ['--check-gems'])
    assert_includes output, "All gems loaded successfully"
  end
end
```

#### 6.3: Performance Benchmarking

**File**: `benchmark/tebako_integration_bench.rb`

```ruby
require 'benchmark'

Benchmark.bmbm do |x|
  x.report("ZIP backend startup") do
    100.times { initialize_with_zip }
  end

  x.report("DwarFS backend startup") do
    100.times { initialize_with_dwarfs }
  end

  x.report("File.read (ZIP)") do
    with_zip_backend { 1000.times { File.read('/__tebako__/data.txt') } }
  end

  x.report("File.read (DwarFS)") do
    with_dwarfs_backend { 1000.times { File.read('/__tebako__/data.txt') } }
  end

  x.report("Random access (ZIP)") do
    with_zip_backend { random_access_test }
  end

  x.report("Random access (DwarFS)") do
    with_dwarfs_backend { random_access_test }
  end
end
```

Expected Results:
- DwarFS startup: ~5ms (vs ZIP ~2ms)
- File reads: DwarFS 2-3x faster
- Random access: DwarFS 100x faster

---

## Phase 7: Deployment & Documentation (Week 3, Day 5)

### Tasks

#### 7.1: Update Tebako Documentation

Add to Tebako README:

```markdown
## Filesystem Backends

Tebako now supports multiple archive formats:

### DwarFS (Recommended)
- 70% smaller executables
- 100x faster random access
- Best for production deployment

### ZIP (Compatible)
- Universal compatibility
- Standard tooling
- Best for development
```

#### 7.2: Create Migration Guide

**File**: `docs/MIGRATION_TO_LIBTFS.md`

Guide existing Tebako users to new implementation.

#### 7.3: Update CI/CD

Ensure GitHub Actions test both backends:

```yaml
jobs:
  test-zip-backend:
    runs-on: ubuntu-latest
    env:
      TEBAKO_BACKEND: zip
    steps:
      - uses: actions/checkout@v4
      - name: Test with ZIP
        run: bundle exec rake test

  test-dwarfs-backend:
    runs-on: ubuntu-latest
    env:
      TEBAKO_BACKEND: dwarfs
    steps:
      - uses: actions/checkout@v4
      - name: Test with DwarFS
        run: bundle exec rake test
```

---

## Implementation Checklist

### Week 1: Foundation
- [ ] Clone and analyze Tebako repository
- [ ] Document current memfs architecture
- [ ] Create integration architecture document
- [ ] Implement FFI bindings module
- [ ] Test FFI bindings standalone
- [ ] Create high-level API wrapper
- [ ] Write binding unit tests

### Week 2: Core Integration
- [ ] Implement File class patches
- [ ] Implement Dir class patches
- [ ] Implement IO class patches
- [ ] Implement Kernel.load/require patches
- [ ] Update Tebako CMakeLists.txt
- [ ] Update vcpkg.json dependencies
- [ ] Create initialization module
- [ ] Update Tebako entry point

### Week 3: Testing & Polish
- [ ] Create test Ruby application
- [ ] Run integration tests
- [ ] Performance benchmarking
- [ ] Fix any issues found
- [ ] Update Tebako documentation
- [ ] Create migration guide
- [ ] Update CI/CD workflows

### Validation
- [ ] Build Tebako with libtfs
- [ ] Package real Ruby app with DwarFS
- [ ] Package real Ruby app with ZIP
- [ ] Compare executable sizes
- [ ] Benchmark startup time
- [ ] Benchmark file access performance
- [ ] Test on Linux, macOS, Windows
- [ ] Memory leak testing with Valgrind

---

## Success Criteria

### Functional Requirements
- ✅ Tebako builds successfully with libtfs linked
- ✅ Packaged executables run without errors
- ✅ All Ruby file operations work transparently
- ✅ Gems load correctly from embedded filesystem
- ✅ DwarFS backend produces smaller executables
- ✅ No performance regression vs current implementation

### Performance Targets
- **Executable Size**: 30-50% smaller with DwarFS
- **Startup Time**: <500ms (no regression)
- **File Access**: 2-3x faster with DwarFS
- **Memory Usage**: <100MB overhead

### Quality Gates
- All existing Tebako tests pass
- No memory leaks (Valgrind clean)
- No thread safety issues
- Works on Linux, macOS, Windows

---

## Risk Management

### Risk: FFI Loading Fails

**Mitigation**:
- Test library loading on all platforms
- Provide fallback paths
- Document LD_LIBRARY_PATH requirements

### Risk: Symbol Access Fails

**Mitigation**:
- Test linker symbol access patterns
- Provide alternative embedding methods
- Document platform-specific requirements

### Risk: Performance Regression

**Mitigation**:
- Benchmark early and often
- Compare against baseline
- Tune if needed (caching, prefetch)

### Risk: Ruby Compatibility Issues

**Mitigation**:
- Test with Ruby 2.7, 3.0, 3.1, 3.2, 3.3
- Use conservative monkey-patching
- Provide compatibility shims

---

## Timeline

### Compressed Schedule (3 Weeks)

**Week 1: Foundation**
- Days 1-2: Architecture analysis
- Days 3-5: FFI implementation and testing

**Week 2: Integration**
- Days 1-3: Ruby core class patches
- Days 4-5: Build system integration

**Week 3: Validation**
- Days 1-3: Testing with real applications
- Days 4-5: Documentation and polish

**Total**: 15 working days

### Contingency

Add 1 week buffer for:
- Unexpected platform issues
- Performance tuning
- Bug fixes

---

## Next Steps

1. **Start Week 1** - Clone Tebako and analyze architecture
2. **Create tracking document** - docs/TEBAKO_INTEGRATION_STATUS.md
3. **Set up development environment** - Both libtfs and Tebako
4. **Begin FFI implementation** - Following this plan

---

## Resources

### Documentation
- link:TEBAKO_INTEGRATION_GUIDE.adoc[Tebako Integration Guide]
- link:../include/tebako/fs/c_api.h[libtfs C API Header]
- link:backends/DWARFS_BACKEND.adoc[DwarFS Backend Documentation]

### Example Code
- link:../examples/ruby_integration_example.rb[Ruby Integration Example]
- link:TEBAKO_INTEGRATION_GUIDE.adoc#_step_2_create_ruby_ffi_bindings[FFI Bindings Template]

### External References
- https://github.com/ffi/ffi[Ruby FFI Documentation]
- https://github.com/tamatebako/tebako[Tebako Repository]
- https://github.com/mhx/dwarfs[DwarFS Project]