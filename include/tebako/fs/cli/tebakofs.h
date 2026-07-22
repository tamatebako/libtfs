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
 *
 * Copyright (c) 2021-2025 Ribose Inc.
 * All rights reserved.
 */

#pragma once

#include <tebako/fs/filesystem.h>
#include <tebako/fs/directory_iterator.h>
#include <tebako/fs/file_handle.h>
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
  bool recursive = false;    // For ls -r
  bool long_format = false;  // For ls -l
  std::string dest_dir;      // For extract --dest
  std::string runtime_ref;   // For bundle --runtime-ref
  bool lean = false;         // For bundle --lean (TPKG_FLAG_LEAN)
  int launcher_abi = 0;      // For bundle --launcher-abi
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
  int cmd_extract(const std::string& archive, const std::vector<std::string>& files, const CLIOptions& opts);

  /**
   * @brief Search for files matching pattern
   */
  int cmd_find(const std::string& archive, const std::string& pattern, const CLIOptions& opts);

  /**
   * @brief Assemble a three-part package (bootstrap + images + tpkg trailer)
   *
   * @param bootstrap Path to the bootstrap/runtime executable
   * @param images Image specs "<img[:mountpoint]>" (at least one)
   * @param output Output binary path
   * @param opts Command options (runtime_ref, package_flags, launcher_abi)
   * @return Exit code
   */
  int cmd_bundle(const std::string& bootstrap,
                 const std::vector<std::string>& images,
                 const std::string& output,
                 const CLIOptions& opts);

  /**
   * @brief Decompose a three-part package into a directory
   */
  int cmd_unbundle(const std::string& binary, const std::string& output_dir, const CLIOptions& opts);

  /**
   * @brief Rebuild a binary from an unbundled directory
   */
  int cmd_reassemble(const std::string& input_dir, const std::string& output, const CLIOptions& opts);

  /**
   * @brief Append an image slot to a package (in place)
   */
  int cmd_insert_image(const std::string& binary, const std::string& image_spec, const CLIOptions& opts);

  /**
   * @brief Remove an image slot from a package (in place)
   */
  int cmd_remove_image(const std::string& binary, uint32_t slot_index, const CLIOptions& opts);

  /**
   * @brief Replace the bootstrap (executable) portion of a package (in place)
   */
  int cmd_set_runtime(const std::string& binary, const std::string& runtime_file, const CLIOptions& opts);

  /**
   * @brief Create a filesystem image from a directory (wraps mkdwarfs)
   */
  int cmd_mkimage(const std::string& format,
                  const std::string& source_dir,
                  const std::string& output,
                  const CLIOptions& opts);

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
  void print_entry(const DirectoryEntry& entry, const std::string& path, bool long_format);

  /**
   * @brief List directory recursively
   */
  void list_recursive(FileSystem* fs, const std::string& path, const std::string& prefix, bool long_format);

  /**
   * @brief Print directory tree
   */
  void print_tree(FileSystem* fs, const std::string& path, int depth, const std::string& prefix);

  /**
   * @brief Extract single file
   */
  bool extract_file(FileSystem* fs, const std::string& src, const std::string& dest);

  /**
   * @brief Extract directory recursively
   */
  bool extract_directory(FileSystem* fs, const std::string& src, const std::string& dest);

  /**
   * @brief Extract specific files/directories
   *
   * @param fs Mounted filesystem
   * @param files List of paths to extract
   * @param dest_base Destination base directory
   * @return true if all extractions succeeded
   */
  bool extract_selected(FileSystem* fs, const std::vector<std::string>& files, const std::string& dest_base);

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