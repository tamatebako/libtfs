/**
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project (libtfs).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <tebako/fs/core/result.h>
#include <tebako/fs/core/path.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <sys/types.h>

namespace tebako {
namespace fs {

// Forward declarations
class FileHandle;
class DirectoryIterator;

/**
 * @brief Abstract filesystem backend interface
 *
 * All filesystem backends (DwarFS, ZIP, TAR, etc.) must implement this interface.
 * This provides a unified API for mounting, reading, and navigating different
 * archive formats.
 *
 * All operations that can fail return `Result<T, Error>` for structured error
 * handling. This provides:
 * - Clear indication of success/failure
 * - Machine-readable error codes
 * - Human-readable error messages with context
 * - No exceptions for expected error conditions
 *
 * Thread Safety: All methods must be thread-safe for concurrent access.
 * Implementations should use appropriate synchronization primitives.
 *
 * @example
 * @code
 * auto backend = std::make_unique<ZipBackend>();
 * auto mount_result = backend->mount("/path/to/archive.zip", "/mnt/app");
 * if (mount_result.is_ok()) {
 *     auto open_result = backend->open("/mnt/app/file.txt", O_RDONLY);
 *     if (open_result.is_ok()) {
 *         auto handle = open_result.unwrap();
 *         // ... use file ...
 *     } else {
 *         std::cerr << "Failed to open: " << open_result.error().full_message() << std::endl;
 *     }
 *     backend->unmount();
 * }
 * @endcode
 */
class FileSystem {
 public:
  virtual ~FileSystem() = default;

  // ===================================================================
  // Lifecycle Operations
  // ===================================================================

  /**
   * @brief Mount an archive at the specified mount point
   *
   * @param archive_path Path to the archive file (e.g., "/data/app.zip")
   * @param mount_point Virtual mount point (e.g., "/mnt/app")
   * @return Result<void> - success or error with details
   *
   * @note The mount point should be an absolute path
   * @note Multiple archives can be mounted at different mount points
   */
  virtual Result<void> mount(std::string_view archive_path, std::string_view mount_point) = 0;

  /**
   * @brief Mount an archive from memory buffer
   *
   * Mounts an archive that resides in memory (typically embedded in executable).
   * The memory buffer must remain valid for the lifetime of the mounted filesystem.
   *
   * @param data Pointer to archive data in memory
   * @param size Size of archive in bytes
   * @param mount_point Virtual mount point (e.g., "/__tebako__")
   * @return Result<void> - success or error with details
   *
   * @note The caller must ensure 'data' remains valid until unmount()
   * @note Only one archive can be mounted at a time
   * @note The archive path will be empty for memory-mounted filesystems
   */
  virtual Result<void> mount_from_memory(const void* data, size_t size,
                                          std::string_view mount_point) = 0;

  /**
   * @brief Unmount the filesystem
   *
   * Closes all open file handles and releases resources.
   * After unmount, all file handles become invalid.
   */
  virtual void unmount() = 0;

  /**
   * @brief Check if the filesystem is currently mounted
   *
   * @return true if mounted, false otherwise
   */
  virtual bool is_mounted() const = 0;

  // ===================================================================
  // File Operations
  // ===================================================================

  /**
   * @brief Open a file for reading
   *
   * @param path Absolute path to the file within the mounted filesystem
   * @param flags Open flags (e.g., O_RDONLY) - write operations not supported
   * @return Result containing FileHandle or error
   *
   * @note Paths are relative to the mount point
   * @note Only read operations are supported in current implementation
   */
  virtual Result<std::unique_ptr<FileHandle>> open(std::string_view path, int flags) = 0;

  /**
   * @brief Check if a path exists
   *
   * @param path Absolute path to check
   * @return true if path exists (file or directory), false otherwise
   *
   * @note This returns bool because non-existence is not an error condition
   */
  virtual bool exists(std::string_view path) const = 0;

  /**
   * @brief Check if a path is a regular file
   *
   * @param path Absolute path to check
   * @return true if path is a regular file, false otherwise (including if not found)
   */
  virtual bool is_file(std::string_view path) const = 0;

  /**
   * @brief Check if a path is a directory
   *
   * @param path Absolute path to check
   * @return true if path is a directory, false otherwise (including if not found)
   */
  virtual bool is_directory(std::string_view path) const = 0;

  // ===================================================================
  // Directory Operations
  // ===================================================================

  /**
   * @brief List contents of a directory
   *
   * @param path Absolute path to the directory
   * @return Result containing DirectoryIterator or error
   *
   * @note The iterator remains valid until the filesystem is unmounted
   * @note Use the iterator's has_next() and next() methods to traverse entries
   */
  virtual Result<std::unique_ptr<DirectoryIterator>> list_directory(std::string_view path) = 0;

  // ===================================================================
  // Metadata Operations
  // ===================================================================

  /**
   * @brief Get the size of a file in bytes
   *
   * @param path Absolute path to the file
   * @return Result containing file size or error
   */
  virtual Result<int64_t> file_size(std::string_view path) const = 0;

  /**
   * @brief Get the modification time of a file
   *
   * @param path Absolute path to the file
   * @return Result containing modification time or error
   */
  virtual Result<time_t> modification_time(std::string_view path) const = 0;

  /**
   * @brief Get the permissions of a file
   *
   * @param path Absolute path to the file
   * @return Result containing permissions or error
   */
  virtual Result<mode_t> permissions(std::string_view path) const = 0;

  // ===================================================================
  // Backend Information
  // ===================================================================

  /**
   * @brief Get the name of this backend
   *
   * @return Backend name (e.g., "DwarFS", "ZIP", "TAR")
   */
  virtual std::string backend_name() const = 0;

  /**
   * @brief Get the version of this backend
   *
   * @return Backend version string
   */
  virtual std::string backend_version() const = 0;

  /**
   * @brief Get the path to the mounted archive
   *
   * @return Archive path, or empty string if not mounted
   */
  virtual std::string archive_path() const = 0;

  /**
   * @brief Get the mount point of this filesystem
   *
   * @return Mount point path, or empty string if not mounted
   */
  virtual std::string mount_point() const = 0;
};

}  // namespace fs
}  // namespace tebako
