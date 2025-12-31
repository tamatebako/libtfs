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
#include <string>
#include <sys/types.h>

namespace tebako {
namespace fs {

/**
 * @brief Abstract file handle for reading from backend filesystems
 *
 * Provides a POSIX-like interface for reading files from archive backends.
 * Each backend (DwarFS, ZIP, etc.) implements this interface to provide
 * consistent file access semantics.
 *
 * Thread Safety: FileHandle instances are NOT thread-safe. Each thread
 * should open its own file handle.
 *
 * @example
 * @code
 * auto handle = backend->open("/mnt/app/file.txt", O_RDONLY);
 * if (handle) {
 *     char buffer[1024];
 *     ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
 *     if (bytes_read > 0) {
 *         // Process buffer...
 *     }
 *     handle->close();
 * }
 * @endcode
 */
class FileHandle {
 public:
  virtual ~FileHandle() = default;

  // ===================================================================
  // Read Operations
  // ===================================================================

  /**
   * @brief Read data from the file
   *
   * Reads up to 'count' bytes from the current file position into 'buffer'.
   * Advances the file position by the number of bytes read.
   *
   * @param buffer Destination buffer for read data
   * @param count Maximum number of bytes to read
   * @return Number of bytes actually read, 0 on EOF, -1 on error
   *
   * @note Behavior matches POSIX read():
   *       - May read fewer bytes than requested
   *       - Returns 0 when at end of file
   *       - Returns -1 on error (check errno if applicable)
   */
  virtual ssize_t read(void* buffer, size_t count) = 0;

  // ===================================================================
  // Seek Operations
  // ===================================================================

  /**
   * @brief Seek to a position in the file
   *
   * Changes the current file position according to 'offset' and 'whence'.
   *
   * @param offset Offset in bytes
   * @param whence Position reference:
   *               - SEEK_SET: offset from beginning of file
   *               - SEEK_CUR: offset from current position
   *               - SEEK_END: offset from end of file
   * @return New file position from beginning, or -1 on error
   *
   * @note Some backends (e.g., ZIP) may have limited seek capabilities
   *       and may need to buffer data internally
   */
  virtual off_t seek(off_t offset, int whence) = 0;

  /**
   * @brief Get the current file position
   *
   * @return Current position in bytes from beginning of file, or -1 on error
   */
  virtual off_t tell() const = 0;

  // ===================================================================
  // Status Operations
  // ===================================================================

  /**
   * @brief Check if at end of file
   *
   * @return true if current position is at or beyond end of file
   */
  virtual bool eof() const = 0;

  /**
   * @brief Close the file handle
   *
   * Releases any resources associated with this file handle.
   * After close(), all other methods become invalid.
   *
   * @note The destructor will call close() if not already closed
   */
  virtual void close() = 0;

  // ===================================================================
  // File Information
  // ===================================================================

  /**
   * @brief Get the path of the file
   *
   * @return Absolute path to the file within the mounted filesystem
   */
  virtual std::string path() const = 0;

  /**
   * @brief Get the size of the file
   *
   * @return File size in bytes
   */
  virtual int64_t size() const = 0;
};

}  // namespace fs
}  // namespace tebako