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

#include <tebako/fs/filesystem.h>

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <sys/types.h>

// Forward declarations for libzip to avoid exposing implementation details
struct zip;

namespace tebako {
namespace fs {

/**
 * @brief ZIP archive backend implementation
 *
 * Provides read-only access to ZIP archives through the FileSystem interface.
 * Uses libzip for archive operations.
 *
 * Thread Safety: All methods are thread-safe for concurrent access using
 * std::shared_mutex for read/write locking.
 *
 * Limitations:
 * - ZIP format doesn't support native seeking. Seek operations are implemented
 *   by closing and reopening the file, then skipping to the desired position.
 * - ZIP archives don't reliably store POSIX permissions. Default permissions
 *   (0644 for files, 0755 for directories) are provided.
 *
 * @example
 * @code
 * auto backend = std::make_unique<ZipBackend>();
 * if (backend->mount("/data/app.zip", "/mnt/app")) {
 *     auto handle = backend->open("/mnt/app/file.txt", O_RDONLY);
 *     // ... use file ...
 *     backend->unmount();
 * }
 * @endcode
 */
class ZipBackend : public FileSystem {
 public:
  /**
   * @brief Construct a new ZipBackend
   *
   * Creates an unmounted ZIP backend instance.
   */
  ZipBackend();

  /**
   * @brief Destroy the ZipBackend
   *
   * Automatically unmounts the archive if still mounted.
   */
  ~ZipBackend() override;

  // ===================================================================
  // Lifecycle Operations (FileSystem interface)
  // ===================================================================

  /**
   * @brief Mount a ZIP archive
   *
   * Opens the ZIP archive and prepares it for access.
   *
   * @param archive_path Path to the ZIP file
   * @param mount_point Virtual mount point
   * @return true if mount succeeded, false otherwise
   *
   * @note Returns false if already mounted or if archive cannot be opened
   */
  bool mount(const std::string& archive_path,
             const std::string& mount_point) override;

  /**
   * @brief Mount a ZIP archive from memory buffer
   *
   * Opens a ZIP archive from memory (typically embedded in executable).
   * The memory buffer must remain valid until unmount() is called.
   *
   * @param data Pointer to ZIP archive data in memory
   * @param size Size of archive in bytes
   * @param mount_point Virtual mount point
   * @return true if mount succeeded, false otherwise
   *
   * @note Returns false if already mounted or if archive cannot be opened
   * @note The archive_path will be empty for memory-mounted archives
   */
  bool mount_from_memory(const void* data, size_t size,
                         const std::string& mount_point) override;

  /**
   * @brief Unmount the ZIP archive
   *
   * Closes the archive and releases all resources.
   * All open file handles become invalid after unmount.
   */
  void unmount() override;

  /**
   * @brief Check if the archive is mounted
   *
   * @return true if mounted, false otherwise
   */
  bool is_mounted() const override;

  // ===================================================================
  // File Operations (FileSystem interface)
  // ===================================================================

  /**
   * @brief Open a file in the ZIP archive
   *
   * @param path Absolute path to the file
   * @param flags Open flags (only O_RDONLY supported)
   * @return Unique pointer to FileHandle, or nullptr on error
   */
  std::unique_ptr<FileHandle> open(const std::string& path,
                                   int flags) override;

  /**
   * @brief Check if a path exists in the archive
   *
   * @param path Absolute path to check
   * @return true if path exists, false otherwise
   */
  bool exists(const std::string& path) const override;

  /**
   * @brief Check if a path is a regular file
   *
   * @param path Absolute path to check
   * @return true if path is a file, false otherwise
   */
  bool is_file(const std::string& path) const override;

  /**
   * @brief Check if a path is a directory
   *
   * @param path Absolute path to check
   * @return true if path is a directory, false otherwise
   */
  bool is_directory(const std::string& path) const override;

  // ===================================================================
  // Directory Operations (FileSystem interface)
  // ===================================================================

  /**
   * @brief List contents of a directory
   *
   * @param path Absolute path to the directory
   * @return Unique pointer to DirectoryIterator, or nullptr on error
   */
  std::unique_ptr<DirectoryIterator> list_directory(
      const std::string& path) override;

  // ===================================================================
  // Metadata Operations (FileSystem interface)
  // ===================================================================

  /**
   * @brief Get the size of a file
   *
   * @param path Absolute path to the file
   * @return File size in bytes, or -1 on error
   */
  int64_t file_size(const std::string& path) const override;

  /**
   * @brief Get the modification time of a file
   *
   * @param path Absolute path to the file
   * @return Modification time as Unix timestamp, or 0 on error
   */
  time_t modification_time(const std::string& path) const override;

  /**
   * @brief Get the permissions of a file
   *
   * @param path Absolute path to the file
   * @return File permissions as mode_t, or 0 on error
   *
   * @note ZIP archives don't reliably store POSIX permissions.
   *       Returns 0644 for files, 0755 for directories.
   */
  mode_t permissions(const std::string& path) const override;

  // ===================================================================
  // Backend Information (FileSystem interface)
  // ===================================================================

  /**
   * @brief Get the backend name
   *
   * @return "ZIP"
   */
  std::string backend_name() const override { return "ZIP"; }

  /**
   * @brief Get the backend version
   *
   * @return Version string of libzip library
   */
  std::string backend_version() const override;

  /**
   * @brief Get the archive path
   *
   * @return Path to the mounted archive, or empty if not mounted
   */
  std::string archive_path() const override { return archive_path_; }

  /**
   * @brief Get the mount point
   *
   * @return Mount point path, or empty if not mounted
   */
  std::string mount_point() const override { return mount_point_; }

 private:
  /**
   * @brief Locate an entry in the ZIP archive
   *
   * @param path Relative path (without mount point)
   * @return ZIP entry index, or -1 if not found
   */
  int64_t locate_entry(const std::string& path) const;

  /**
   * @brief Strip the mount point prefix from a path
   *
   * Converts absolute path to relative path within the archive.
   *
   * @param path Absolute path with mount point
   * @return Relative path without mount point
   *
   * @example "/mnt/app/file.txt" -> "file.txt"
   */
  std::string strip_mount_point(const std::string& path) const;

  /**
   * @brief Normalize a path for ZIP lookup
   *
   * Removes leading/trailing slashes and handles directory notation.
   *
   * @param path Path to normalize
   * @return Normalized path suitable for zip_name_locate()
   */
  std::string normalize_path(const std::string& path) const;

  /**
   * @brief Check if a path represents a directory entry
   *
   * In ZIP format, directories are entries ending with '/'.
   *
   * @param path Path to check
   * @return true if path represents a directory
   */
  bool is_directory_entry(const std::string& path) const;

  // Member variables
  struct zip* archive_;           ///< libzip archive handle
  std::string archive_path_;      ///< Path to the ZIP file
  std::string mount_point_;       ///< Virtual mount point
  mutable std::shared_mutex mutex_;  ///< Thread-safe access synchronization
};

}  // namespace fs
}  // namespace tebako