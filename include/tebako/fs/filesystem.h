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

#include <cstdint>
#include <memory>
#include <string>
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
 * Thread Safety: All methods must be thread-safe for concurrent access.
 * Implementations should use appropriate synchronization primitives.
 *
 * @example
 * @code
 * auto backend = std::make_unique<ZipBackend>();
 * if (backend->mount("/path/to/archive.zip", "/mnt/app")) {
 *     auto handle = backend->open("/mnt/app/file.txt", O_RDONLY);
 *     // ... use file ...
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
   * @return true if mount succeeded, false otherwise
   *
   * @note The mount point should be an absolute path
   * @note Multiple archives can be mounted at different mount points
   */
  virtual bool mount(const std::string& archive_path, const std::string& mount_point) = 0;

  /**
   * @brief Mount an archive from memory buffer
   *
   * Mounts an archive that resides in memory (typically embedded in executable).
   * The memory buffer must remain valid for the lifetime of the mounted filesystem.
   *
   * @param data Pointer to archive data in memory
   * @param size Size of archive in bytes
   * @param mount_point Virtual mount point (e.g., "/__tebako__")
   * @return true if mount succeeded, false otherwise
   *
   * @note The caller must ensure 'data' remains valid until unmount()
   * @note Only one archive can be mounted at a time
   * @note The archive path will be empty for memory-mounted filesystems
   */
  virtual bool mount_from_memory(const void* data, size_t size,
                                  const std::string& mount_point) = 0;

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
   * @return Unique pointer to FileHandle, or nullptr on error
   *
   * @note Paths are relative to the mount point
   * @note Only read operations are supported in current implementation
   */
  virtual std::unique_ptr<FileHandle> open(const std::string& path, int flags) = 0;

  /**
   * @brief Check if a path exists
   *
   * @param path Absolute path to check
   * @return true if path exists (file or directory), false otherwise
   */
  virtual bool exists(const std::string& path) const = 0;

  /**
   * @brief Check if a path is a regular file
   *
   * @param path Absolute path to check
   * @return true if path is a regular file, false otherwise
   */
  virtual bool is_file(const std::string& path) const = 0;

  /**
   * @brief Check if a path is a directory
   *
   * @param path Absolute path to check
   * @return true if path is a directory, false otherwise
   */
  virtual bool is_directory(const std::string& path) const = 0;

  // ===================================================================
  // Directory Operations
  // ===================================================================

  /**
   * @brief List contents of a directory
   *
   * @param path Absolute path to the directory
   * @return Unique pointer to DirectoryIterator, or nullptr on error
   *
   * @note The iterator remains valid until the filesystem is unmounted
   * @note Use the iterator's has_next() and next() methods to traverse entries
   */
  virtual std::unique_ptr<DirectoryIterator> list_directory(const std::string& path) = 0;

  // ===================================================================
  // Metadata Operations
  // ===================================================================

  /**
   * @brief Get the size of a file in bytes
   *
   * @param path Absolute path to the file
   * @return File size in bytes, or -1 on error
   */
  virtual int64_t file_size(const std::string& path) const = 0;

  /**
   * @brief Get the modification time of a file
   *
   * @param path Absolute path to the file
   * @return Modification time as Unix timestamp, or 0 on error
   */
  virtual time_t modification_time(const std::string& path) const = 0;

  /**
   * @brief Get the permissions of a file
   *
   * @param path Absolute path to the file
   * @return File permissions as mode_t, or 0 on error
   */
  virtual mode_t permissions(const std::string& path) const = 0;

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