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
#include <string_view>
#include <sys/types.h>

namespace tebako {
namespace fs {

/**
 * @brief DwarFS archive backend implementation
 *
 * Provides read-only access to DwarFS archives through the FileSystem interface.
 * Uses DwarFS v0.9+ reader library for archive operations.
 *
 * Thread Safety: All methods are thread-safe for concurrent access using
 * std::shared_mutex for read/write locking.
 *
 * Advantages over ZIP and SquashFS:
 * - Extremely high compression ratios (often 30-50% better than SquashFS)
 * - Fast random access with native seek support
 * - Stores full POSIX permissions and metadata
 * - Efficient deduplication at file and block level
 * - FlatBuffers-based metadata for fast parsing
 * - Optimized for embedded read-only filesystems
 *
 * @example
 * @code
 * auto backend = std::make_unique<DwarfsBackend>();
 * if (backend->mount("/data/app.dwarfs", "/mnt/app")) {
 *     auto handle = backend->open("/mnt/app/file.txt", O_RDONLY);
 *     // ... use file ...
 *     backend->unmount();
 * }
 * @endcode
 */
class DwarfsBackend : public FileSystem {
 public:
  /**
   * @brief Construct a new DwarfsBackend
   *
   * Creates an unmounted DwarFS backend instance.
   */
  DwarfsBackend();

  /**
   * @brief Destroy the DwarfsBackend
   *
   * Automatically unmounts the archive if still mounted.
   */
  ~DwarfsBackend() override;

  // ===================================================================
  // Lifecycle Operations (FileSystem interface)
  // ===================================================================

  /**
   * @brief Mount a DwarFS archive
   *
   * Opens the DwarFS archive and prepares it for access.
   *
   * @param archive_path Path to the DwarFS file
   * @param mount_point Virtual mount point
   * @return Result<void> - success or error with details
   */
  Result<void> mount(std::string_view archive_path,
                     std::string_view mount_point) override;

  /**
   * @brief Mount a DwarFS archive from memory buffer
   *
   * Opens a DwarFS archive from memory (typically embedded in executable).
   * The memory buffer must remain valid until unmount() is called.
   *
   * @param data Pointer to DwarFS archive data in memory
   * @param size Size of archive in bytes
   * @param mount_point Virtual mount point
   * @return Result<void> - success or error with details
   *
   * @note The archive_path will be empty for memory-mounted archives
   */
  Result<void> mount_from_memory(const void* data, size_t size,
                                 std::string_view mount_point) override;

  /**
   * @brief Unmount the DwarFS archive
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
   * @brief Open a file in the DwarFS archive
   *
   * @param path Absolute path to the file
   * @param flags Open flags (only O_RDONLY supported)
   * @return Result containing FileHandle or error
   */
  Result<std::unique_ptr<FileHandle>> open(std::string_view path,
                                           int flags) override;

  /**
   * @brief Check if a path exists in the archive
   *
   * @param path Absolute path to check
   * @return true if path exists, false otherwise
   */
  bool exists(std::string_view path) const override;

  /**
   * @brief Check if a path is a regular file
   *
   * @param path Absolute path to check
   * @return true if path is a file, false otherwise
   */
  bool is_file(std::string_view path) const override;

  /**
   * @brief Check if a path is a directory
   *
   * @param path Absolute path to check
   * @return true if path is a directory, false otherwise
   */
  bool is_directory(std::string_view path) const override;

  // ===================================================================
  // Directory Operations (FileSystem interface)
  // ===================================================================

  /**
   * @brief List contents of a directory
   *
   * @param path Absolute path to the directory
   * @return Result containing DirectoryIterator or error
   */
  Result<std::unique_ptr<DirectoryIterator>> list_directory(
      std::string_view path) override;

  // ===================================================================
  // Metadata Operations (FileSystem interface)
  // ===================================================================

  /**
   * @brief Get the size of a file
   *
   * @param path Absolute path to the file
   * @return Result containing file size or error
   */
  Result<int64_t> file_size(std::string_view path) const override;

  /**
   * @brief Get the modification time of a file
   *
   * @param path Absolute path to the file
   * @return Result containing modification time or error
   */
  Result<time_t> modification_time(std::string_view path) const override;

  /**
   * @brief Get the permissions of a file
   *
   * @param path Absolute path to the file
   * @return Result containing permissions or error
   *
   * @note DwarFS archives store full POSIX permissions.
   */
  Result<mode_t> permissions(std::string_view path) const override;

  // ===================================================================
  // Backend Information (FileSystem interface)
  // ===================================================================

  /**
   * @brief Get the backend name
   *
   * @return "DwarFS"
   */
  std::string backend_name() const override { return "DwarFS"; }

  /**
   * @brief Get the backend version
   *
   * @return Version string of DwarFS library
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
   * @brief PIMPL implementation class
   *
   * Hides DwarFS library details from the public interface.
   */
  class Impl;

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
  std::string strip_mount_point(std::string_view path) const;

  /**
   * @brief Normalize a path for DwarFS lookup
   *
   * Removes leading/trailing slashes and handles directory notation.
   *
   * @param path Path to normalize
   * @return Normalized path suitable for inode lookup
   */
  std::string normalize_path(std::string_view path) const;

  // Member variables
  std::unique_ptr<Impl> impl_;       ///< PIMPL implementation
  std::string archive_path_;          ///< Path to the DwarFS file
  std::string mount_point_;           ///< Virtual mount point
  mutable std::shared_mutex mutex_;  ///< Thread-safe access synchronization
};

}  // namespace fs
}  // namespace tebako