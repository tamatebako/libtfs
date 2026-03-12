# Stage 2 Day 6: SquashFS Backend Testing & Documentation + tebakofs CLI Tool

**Date**: 2025-12-23 (Next Business Day)
**Mode**: Code
**Duration**: 1 day (8 hours)
**Prerequisites**: Day 5 Complete ✅ (SquashFS Backend Implementation)

---

## Context

You are continuing Stage 2 implementation of the Tebako filesystem library (libtfs). Day 5 successfully delivered a complete SquashFS backend implementation (1,202 lines) following the ZIP backend architecture pattern. Now you're implementing comprehensive testing, documentation, and a **production-quality CLI tool** called `tebakofs`.

**Day 5 Achievements**:
- ✅ SquashFSBackend header (268 lines)
- ✅ SquashFSBackend implementation (757 lines)
- ✅ SquashFSFileHandle with native seek support
- ✅ SquashFSDirectoryIterator with metadata
- ✅ BackendFactory integration with format detection
- ✅ CMakeLists.txt updates
- ✅ Manual test program

**Day 6 Objectives**:
1. Create comprehensive test suite for SquashFS backend
2. Complete documentation for SquashFS
3. **Build production-quality `tebakofs` CLI tool with Docker-like commands**

---

## What Has Been Completed

### SquashFS Backend Implementation ✅

Complete implementation following ZIP pattern:
- **Header**: [`include/tebako/fs/backends/squashfs_backend.h`](../include/tebako/fs/backends/squashfs_backend.h)
- **Implementation**: [`src/backends/squashfs_backend.cpp`](../src/backends/squashfs_backend.cpp)
- **Factory Integration**: [`src/backend_factory.cpp`](../src/backend_factory.cpp)
- **Build Configuration**: [`CMakeLists.txt`](../CMakeLists.txt)
- **Manual Test**: [`examples/test_squashfs_manual.cpp`](../examples/test_squashfs_manual.cpp)

### Key Features ✅
- Native seek support (advantage over ZIP)
- Full POSIX permissions preserved
- Thread-safe with `std::shared_mutex`
- RAII resource management
- Complete FileSystem interface implementation

---

## Your Task: Day 6 - Testing, Documentation & CLI Tool

### Objectives

1. Create comprehensive test suite for SquashFS backend
2. Complete documentation
3. **Build `tebakofs` CLI tool with Docker-like commands supporting all filesystem types**

### Success Criteria

- ✅ 5 SquashFS test fixtures created using `mksquashfs` command
- ✅ 47+ comprehensive unit tests (matching ZIP coverage)
- ✅ 13+ integration tests
- ✅ SQUASHFS_BACKEND.adoc documentation complete
- ✅ README.adoc updated with SquashFS support
- ✅ **`tebakofs` CLI tool complete with 7+ commands**
- ✅ **CLI uses argtable3 for argument parsing**
- ✅ **CLI supports advanced features (ls -r, selective extract)**
- ✅ **CLI demonstrates all backend features**
- ✅ All tests passing
- ✅ >95% code coverage

---

## Implementation Steps

### PART 1: CLI Tool Development (3 hours) - **PRIORITY**

We're building a production-quality CLI tool called `tebakofs` that demonstrates the library's capabilities and serves as a comprehensive testing tool.

#### Step 1.1: Architecture Design (15 min)

The `tebakofs` CLI follows Docker-style architecture with enhanced features:

```
tebakofs <command> [options] <archive> [args...]

Commands:
  ls       List directory contents (supports -r for recursive, -l for long format)
  info     Show archive information
  cat      Display file contents
  tree     Show directory tree
  stat     Show file/directory metadata
  extract  Extract archive contents (whole archive or specific files)
  find     Search for files
  help     Show help for a command

Options:
  -v, --verbose    Enable verbose output
  -q, --quiet      Suppress non-error output
  -r, --recursive  Recursive operation (for ls)
  -l, --long       Long format listing (for ls)
```

**Key Features**:
1. **ls with path**: `tebakofs ls archive.zip /subdir`
2. **ls -r (recursive)**: `tebakofs ls -r archive.zip` lists all files recursively
3. **ls -l (long format)**: Shows size, permissions, modification time
4. **extract specific files**: `tebakofs extract archive.zip file1.txt dir/file2.txt`
5. **extract to destination**: `tebakofs extract archive.zip --dest /tmp/out`

**Usage Examples**:
```bash
# List root directory
tebakofs ls archive.zip

# List specific directory
tebakofs ls archive.zip /subdir

# List recursively
tebakofs ls -r archive.zip

# List with details
tebakofs ls -l archive.zip

# List recursively with details
tebakofs ls -rl archive.zip /subdir

# Extract entire archive
tebakofs extract archive.sqfs /tmp/extracted

# Extract specific files
tebakofs extract archive.sqfs file1.txt dir/file2.txt

# Extract specific files to destination
tebakofs extract archive.sqfs --dest /tmp/out file1.txt dir/

# Extract with verbose output
tebakofs extract -v archive.zip file.txt

# Show file metadata
tebakofs stat archive.sqfs /path/to/file.txt

# Display file contents
tebakofs cat archive.zip README.md

# Show directory tree
tebakofs tree archive.sqfs

# Search for files
tebakofs find archive.zip "*.txt"

# Archive information
tebakofs info archive.sqfs
```

**Architecture Principle**: CLI is a **thin layer** on top of the API. All business logic is in the library, CLI just parses arguments and calls API methods.

#### Step 1.2: Add argtable3 Dependency (10 min)

Update [`vcpkg.json`](../vcpkg.json):

```json
{
  "dependencies": [
    "argtable3",
    "boost-asio",
    ...
  ]
}
```

Update [`CMakeLists.txt`](../CMakeLists.txt):

```cmake
# Find argtable3 for CLI tool
find_package(argtable3 CONFIG REQUIRED)
```

#### Step 1.3: Create Enhanced CLI Header (30 min)

Create [`include/tebako/fs/cli/tebakofs.h`](../include/tebako/fs/cli/tebakofs.h):

```cpp
/**
 * @file tebakofs.h
 * @brief CLI tool for Tebako filesystem library
 *
 * Docker-style CLI interface for interacting with ZIP, SquashFS, and DwarFS archives.
 *
 * Examples:
 *   tebakofs ls -rl archive.zip /dir
 *   tebakofs extract archive.sqfs file1.txt file2.txt
 *   tebakofs cat archive.zip /path/to/file.txt
 */

#pragma once

#include <tebako/fs/filesystem.h>
#include <memory>
#include <string>
#include <vector>

namespace tebako {
namespace fs {
namespace cli {

/**
 * @brief Command-line options structure
 */
struct CLIOptions {
  bool verbose = false;
  bool quiet = false;
  bool recursive = false;      // For ls -r
  bool long_format = false;    // For ls -l
  std::string dest_dir;        // For extract --dest
};

/**
 * @brief Main CLI application class
 *
 * Thin layer on top of the FileSystem API. All business logic
 * resides in the library, CLI only handles argument parsing and
 * user interaction.
 */
class TebakofsCLI {
 public:
  TebakofsCLI();
  ~TebakofsCLI();

  /**
   * @brief Execute CLI command
   *
   * @param argc Argument count
   * @param argv Argument vector
   * @return Exit code (0 = success, non-zero = error)
   */
  int run(int argc, char* argv[]);

 private:
  // Command implementations (thin wrappers over API)

  /**
   * @brief List directory contents
   *
   * @param archive Path to archive file
   * @param path Path within archive (default: /)
   * @param opts Command options (recursive, long_format)
   * @return Exit code
   */
  int cmd_ls(const std::string& archive, const std::string& path, const CLIOptions& opts);

  /**
   * @brief Show archive information
   */
  int cmd_info(const std::string& archive, const CLIOptions& opts);

  /**
   * @brief Display file contents
   */
  int cmd_cat(const std::string& archive, const std::string& file, const CLIOptions& opts);

  /**
   * @brief Show directory tree
   */
  int cmd_tree(const std::string& archive, const std::string& path, const CLIOptions& opts);

  /**
   * @brief Show file/directory metadata
   */
  int cmd_stat(const std::string& archive, const std::string& path, const CLIOptions& opts);

  /**
   * @brief Extract archive contents
   *
   * If files vector is empty, extracts entire archive.
   * Otherwise, extracts only specified files/directories.
   *
   * @param archive Path to archive file
   * @param files List of files/directories to extract (empty = all)
   * @param opts Command options (dest_dir, verbose)
   * @return Exit code
   */
  int cmd_extract(const std::string& archive, const std::vector<std::string>& files,
                  const CLIOptions& opts);

  /**
   * @brief Search for files matching pattern
   */
  int cmd_find(const std::string& archive, const std::string& pattern, const CLIOptions& opts);

  /**
   * @brief Show help information
   */
  int cmd_help(const std::string& command);

  // Helper methods

  /**
   * @brief Open and mount an archive using auto-detection
   */
  std::unique_ptr<FileSystem> open_archive(const std::string& path);

  /**
   * @brief Print directory entry (short or long format)
   */
  void print_entry(const DirectoryEntry& entry, const std::string& path,
                   bool long_format);

  /**
   * @brief List directory recursively
   */
  void list_recursive(FileSystem* fs, const std::string& path,
                     const std::string& prefix, bool long_format);

  /**
   * @brief Print directory tree
   */
  void print_tree(FileSystem* fs, const std::string& path,
                  int depth, const std::string& prefix);

  /**
   * @brief Extract single file
   */
  bool extract_file(FileSystem* fs, const std::string& src,
                    const std::string& dest);

  /**
   * @brief Extract directory recursively
   */
  bool extract_directory(FileSystem* fs, const std::string& src,
                        const std::string& dest);

  /**
   * @brief Extract specific files/directories
   *
   * @param fs Mounted filesystem
   * @param files List of paths to extract
   * @param dest_base Destination base directory
   * @return true if all extractions succeeded
   */
  bool extract_selected(FileSystem* fs, const std::vector<std::string>& files,
                       const std::string& dest_base);

  /**
   * @brief Extract entire archive
   */
  bool extract_all(FileSystem* fs, const std::string& dest);

  /**
   * @brief Format file size for display
   */
  std::string format_size(int64_t size);

  /**
   * @brief Format permissions for display
   */
  std::string format_permissions(mode_t mode);

  /**
   * @brief Format modification time for display
   */
  std::string format_time(time_t mtime);

  CLIOptions options_;
};

}  // namespace cli
}  // namespace fs
}  // namespace tebako
```

#### Step 1.4: Implement Enhanced CLI Core (1.5 hours)

Create [`src/cli/tebakofs.cpp`](../src/cli/tebakofs.cpp) with full implementation:

**Key Implementation Details**:

1. **ls command with -r and -l**:
```cpp
int TebakofsCLI::cmd_ls(const std::string& archive, const std::string& path,
                        const CLIOptions& opts) {
  auto fs = open_archive(archive);
  if (!fs) return 1;

  std::string full_path = "/mnt" + (path.empty() || path[0] != '/' ? "/" : "") + path;

  if (!fs->exists(full_path)) {
    std::cerr << "Error: Path does not exist: " << path << std::endl;
    return 1;
  }

  if (fs->is_file(full_path)) {
    // For single file, just show its details
    DirectoryEntry entry;
    entry.name = path.substr(path.find_last_of('/') + 1);
    entry.is_directory = false;
    entry.size = fs->file_size(full_path);
    entry.mtime = fs->modification_time(full_path);
    print_entry(entry, path, opts.long_format);
    return 0;
  }

  if (opts.recursive) {
    // Recursive listing
    list_recursive(fs.get(), full_path, "", opts.long_format);
  } else {
    // Single directory listing
    auto iter = fs->list_directory(full_path);
    if (!iter) {
      std::cerr << "Error: Failed to list directory: " << path << std::endl;
      return 1;
    }

    while (iter->has_next()) {
      auto entry = iter->next();
      print_entry(entry, path + "/" + entry.name, opts.long_format);
    }
  }

  return 0;
}

void TebakofsCLI::list_recursive(FileSystem* fs, const std::string& path,
                                 const std::string& prefix, bool long_format) {
  auto iter = fs->list_directory(path);
  if (!iter) return;

  while (iter->has_next()) {
    auto entry = iter->next();
    std::string entry_path = path + "/" + entry.name;
    std::string display_path = entry_path.substr(4);  // Remove "/mnt"

    print_entry(entry, display_path, long_format);

    if (entry.is_directory) {
      list_recursive(fs, entry_path, prefix + "  ", long_format);
    }
  }
}

void TebakofsCLI::print_entry(const DirectoryEntry& entry,
                              const std::string& path,
                              bool long_format) {
  if (long_format) {
    std::cout << format_permissions(entry.mode)
              << "  " << std::setw(10) << format_size(entry.size)
              << "  " << format_time(entry.mtime)
              << "  " << path << std::endl;
  } else {
    std::cout << path << std::endl;
  }
}
```

2. **extract with selective file support**:
```cpp
int TebakofsCLI::cmd_extract(const std::string& archive,
                             const std::vector<std::string>& files,
                             const CLIOptions& opts) {
  auto fs = open_archive(archive);
  if (!fs) return 1;

  std::string dest = opts.dest_dir.empty() ? "." : opts.dest_dir;

  bool success;
  if (files.empty()) {
    // Extract entire archive
    if (opts.verbose) {
      std::cout << "Extracting entire archive to: " << dest << std::endl;
    }
    success = extract_all(fs.get(), dest);
  } else {
    // Extract specific files
    if (opts.verbose) {
      std::cout << "Extracting " << files.size() << " item(s) to: "
                << dest << std::endl;
    }
    success = extract_selected(fs.get(), files, dest);
  }

  if (success && !opts.quiet) {
    std::cout << "Extraction complete" << std::endl;
  }

  return success ? 0 : 1;
}

bool TebakofsCLI::extract_selected(FileSystem* fs,
                                   const std::vector<std::string>& files,
                                   const std::string& dest_base) {
  bool all_success = true;

  for (const auto& file : files) {
    std::string full_path = "/mnt" + (file.empty() || file[0] != '/' ? "/" : "") + file;

    if (!fs->exists(full_path)) {
      std::cerr << "Warning: Path does not exist: " << file << std::endl;
      all_success = false;
      continue;
    }

    if (fs->is_directory(full_path)) {
      if (options_.verbose) {
        std::cout << "Extracting directory: " << file << std::endl;
      }
      if (!extract_directory(fs, full_path, dest_base + "/" + file)) {
        all_success = false;
      }
    } else {
      if (options_.verbose) {
        std::cout << "Extracting file: " << file << std::endl;
      }
      if (!extract_file(fs, full_path, dest_base + "/" + file)) {
        all_success = false;
      }
    }
  }

  return all_success;
}
```

3. **Argument parsing with argtable3**:
```cpp
int TebakofsCLI::run(int argc, char* argv[]) {
  if (argc < 2) {
    return cmd_help("");
  }

  std::string command = argv[1];

  // Parse command-specific options
  // (Implementation uses argtable3 for robust parsing)

  // Example for ls command:
  if (command == "ls") {
    struct arg_lit *recursive = arg_lit0("r", "recursive", "list recursively");
    struct arg_lit *long_fmt = arg_lit0("l", "long", "use long listing format");
    struct arg_lit *verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file *archive = arg_file1(NULL, NULL, "<archive>", "archive file");
    struct arg_str *path = arg_str0(NULL, NULL, "[path]", "path within archive");
    struct arg_end *end = arg_end(20);

    void* argtable[] = {recursive, long_fmt, verbose, archive, path, end};

    if (arg_parse(argc - 1, argv + 1, argtable) == 0) {
      CLIOptions opts;
      opts.recursive = recursive->count > 0;
      opts.long_format = long_fmt->count > 0;
      opts.verbose = verbose->count > 0;

      std::string archive_path = archive->filename[0];
      std::string list_path = path->count > 0 ? path->sval[0] : "/";

      int result = cmd_ls(archive_path, list_path, opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }

  // Similar parsing for other commands...
}
```

(Delete the extraneous PART 2: SquashFS Testing step)

#### Step 1.5: Create CLI Main (15 min)

Create [`src/tebakofs_main.cpp`](../src/tebakofs_main.cpp):

```cpp
/**
 * Main entry point for tebakofs CLI tool
 */

#include <tebako/fs/cli/tebakofs.h>
#include <iostream>

int main(int argc, char* argv[]) {
  try {
    tebako::fs::cli::TebakofsCLI cli;
    return cli.run(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
}
```

#### Step 1.6: Update CMakeLists.txt for CLI (30 min)

Add to [`CMakeLists.txt`](../CMakeLists.txt):

```cmake
# tebakofs CLI tool
add_executable(tebakofs
  "src/tebakofs_main.cpp"
  "src/cli/tebakofs.cpp"
  "include/tebako/fs/cli/tebakofs.h"
)
target_link_libraries(tebakofs PRIVATE tfs argtable3::argtable3)

install(TARGETS tebakofs DESTINATION ${CMAKE_INSTALL_BINDIR})
```

#### Step 1.7: Test CLI Manually (15 min)

```bash
# Build
cmake --build build --target tebakofs

# Test with ZIP
./build/tebakofs info tests/fixtures/zip/simple.zip
./build/tebakofs ls tests/fixtures/zip/simple.zip
./build/tebakofs cat tests/fixtures/zip/simple.zip hello.txt
./build/tebakofs tree tests/fixtures/zip/nested.zip

# Test with SquashFS (once fixtures created)
./build/tebakofs info tests/fixtures/squashfs/simple.sqfs
./build/tebakofs ls -v tests/fixtures/squashfs/permissions.sqfs
./build/tebakofs stat tests/fixtures/squashfs/permissions.sqfs /readonly.txt
```

### PART 2: SquashFS Testing & Documentation (2.5 hours)

(Keep the original testing steps from previous prompt...)