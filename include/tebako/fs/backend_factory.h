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
#include <memory>
#include <string>

namespace tebako {
namespace fs {

/**
 * @brief Static factory for creating filesystem backends
 *
 * Provides simple, compile-time backend creation with optional
 * format auto-detection. No dynamic registration or thread state.
 *
 * The factory supports three detection strategies:
 * 1. Magic number detection (most reliable)
 * 2. File extension detection (fallback)
 * 3. Explicit backend selection
 *
 * @example Basic usage
 * @code
 * // Auto-detect and create
 * auto fs = BackendFactory::create_from_file("/path/to/archive.zip");
 * if (fs && fs->mount("/path/to/archive.zip", "/mnt/app")) {
 *     // Use filesystem...
 *     auto handle = fs->open("/mnt/app/file.txt", O_RDONLY);
 *     fs->unmount();
 * }
 *
 * // Explicit backend selection
 * auto dwarfs = BackendFactory::create_dwarfs();
 * if (dwarfs->mount("/path/to/archive.dwarfs", "/mnt/data")) {
 *     // Use DwarFS...
 * }
 * @endcode
 */
class BackendFactory {
 public:
  // ===================================================================
  // Primary Factory Methods
  // ===================================================================

  /**
   * @brief Auto-detect format and create appropriate backend
   *
   * Detects archive format using:
   * 1. Magic number detection (highest confidence)
   * 2. File extension (fallback)
   *
   * Supported formats:
   * - DwarFS: Magic "DWARFS" at offset 0, extensions: .dwarfs, .dfs
   * - ZIP: Magic PK\x03\x04 at offset 0, extensions: .zip, .jar, .apk, .war, .ear
   * - SquashFS: Magic "hsqs" or "sqsh" at offset 0, extensions: .sqfs, .squashfs
   *
   * @param archive_path Path to archive file
   * @return Unique pointer to FileSystem, or nullptr if format unknown
   *
   * @note Does NOT mount the filesystem, only creates the backend
   * @note Caller must call [`mount()`](filesystem.h:82) on the returned backend
   * @note Returns nullptr for non-existent files or unrecognized formats
   */
  static std::unique_ptr<FileSystem> create_from_file(const std::string& archive_path);

  /**
   * @brief Create backend from memory buffer
   *
   * Auto-detects archive format from magic bytes in memory.
   * Supports the same formats as create_from_file.
   *
   * @param data Pointer to archive data in memory
   * @param size Size of archive in bytes
   * @return Unique pointer to FileSystem, or nullptr if format unknown
   *
   * @note Does NOT mount the filesystem, only creates the backend
   * @note Caller must call mount_from_memory() on the returned backend
   * @note Returns nullptr for invalid data or unrecognized formats
   */
  static std::unique_ptr<FileSystem> create_from_memory(const void* data, size_t size);

  /**
   * @brief Explicitly create DwarFS backend
   *
   * Creates a DwarFS filesystem backend without format detection.
   * The backend uses FlatBuffers for metadata serialization.
   *
   * @return Unique pointer to DwarFS backend
   *
   * @note Backend must still be mounted via [`mount()`](filesystem.h:82)
   */
  static std::unique_ptr<FileSystem> create_dwarfs();

  /**
   * @brief Explicitly create ZIP backend
   *
   * Creates a ZIP filesystem backend without format detection.
   * The backend uses libzip for archive access.
   *
   * @return Unique pointer to ZIP backend
   *
   * @note Backend must still be mounted via [`mount()`](filesystem.h:82)
   */
  static std::unique_ptr<FileSystem> create_zip();

  /**
   * @brief Explicitly create SquashFS backend
   *
   * Creates a SquashFS filesystem backend without format detection.
   * The backend uses squashfs-tools-ng for archive access.
   *
   * @return Unique pointer to SquashFS backend
   *
   * @note Backend must still be mounted via [`mount()`](filesystem.h:82)
   */
  static std::unique_ptr<FileSystem> create_squashfs();

  // ===================================================================
  // Format Detection (Public for Testing)
  // ===================================================================

  /**
   * @brief Detect if file is DwarFS format
   *
   * Checks for DwarFS magic signature "DWARFS" (6 bytes) at file offset 0.
   *
   * @param path Path to file
   * @return true if DwarFS format detected, false otherwise
   *
   * @note Returns false if file cannot be read or is too small
   */
  static bool is_dwarfs_format(const std::string& path);

  /**
   * @brief Detect if file is ZIP format
   *
   * Checks for ZIP magic signatures:
   * - PK\x03\x04 (local file header)
   * - PK\x05\x06 (central directory, for empty archives)
   *
   * @param path Path to file
   * @return true if ZIP format detected, false otherwise
   *
   * @note Returns false if file cannot be read or is too small
   */
  static bool is_zip_format(const std::string& path);

  /**
   * @brief Detect if file is SquashFS format
   *
   * Checks for SquashFS magic signatures at offset 0:
   * - "hsqs" (little-endian: 0x68, 0x73, 0x71, 0x73)
   * - "sqsh" (big-endian: 0x73, 0x71, 0x73, 0x68)
   *
   * @param path Path to file
   * @return true if SquashFS format detected, false otherwise
   *
   * @note Returns false if file cannot be read or is too small
   */
  static bool is_squashfs_format(const std::string& path);

 private:
  // ===================================================================
  // Internal Helpers
  // ===================================================================

  /**
   * @brief Read magic bytes from file header
   *
   * Reads up to `size` bytes from the beginning of the file into `buffer`.
   * Used internally for format detection.
   *
   * @param path Path to file
   * @param buffer Buffer to store bytes (must be at least size bytes)
   * @param size Number of bytes to read
   * @return true if exactly `size` bytes were read, false otherwise
   */
  static bool read_magic_bytes(const std::string& path, uint8_t* buffer, size_t size);

  /**
   * @brief Check if path has given extension
   *
   * Performs case-insensitive comparison of file extension.
   *
   * @param path File path
   * @param ext Extension to check (including dot, e.g., ".zip")
   * @return true if path ends with extension (case-insensitive)
   */
  static bool has_extension(const std::string& path, const std::string& ext);
};

}  // namespace fs
}  // namespace tebako