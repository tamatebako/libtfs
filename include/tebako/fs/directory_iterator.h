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
 * @brief Directory entry information
 *
 * Represents a single entry (file or directory) within a directory listing.
 * Contains essential metadata for each entry.
 */
struct DirectoryEntry {
  /**
   * @brief Name of the entry (basename only, not full path)
   *
   * For example, if the full path is "/mnt/app/config/database.yml",
   * the name would be "database.yml"
   */
  std::string name;

  /**
   * @brief Whether this entry is a directory
   *
   * true = directory, false = file (or other type)
   */
  bool is_directory;

  /**
   * @brief Size of the entry in bytes
   *
   * For directories, this is typically 0 or the number of entries.
   * For files, this is the uncompressed file size.
   */
  int64_t size;

  /**
   * @brief Modification time as Unix timestamp
   *
   * Seconds since epoch (1970-01-01 00:00:00 UTC).
   * May be 0 if the backend doesn't support timestamps.
   */
  time_t mtime;

  /**
   * @brief Default constructor
   */
  DirectoryEntry() : name(), is_directory(false), size(0), mtime(0) {}

  /**
   * @brief Constructor with all fields
   */
  DirectoryEntry(const std::string& n, bool is_dir, int64_t sz, time_t mt)
      : name(n), is_directory(is_dir), size(sz), mtime(mt) {}
};

/**
 * @brief Abstract directory iterator for traversing directory contents
 *
 * Provides an iterator interface for listing files and subdirectories
 * within a directory. Each backend implements this interface to provide
 * consistent directory traversal semantics.
 *
 * Thread Safety: DirectoryIterator instances are NOT thread-safe.
 * Each thread should create its own iterator.
 *
 * @example
 * @code
 * auto iter = backend->list_directory("/mnt/app/config");
 * if (iter) {
 *     while (iter->has_next()) {
 *         DirectoryEntry entry = iter->next();
 *         std::cout << entry.name;
 *         if (entry.is_directory) {
 *             std::cout << "/";
 *         }
 *         std::cout << " (" << entry.size << " bytes)" << std::endl;
 *     }
 * }
 * @endcode
 */
class DirectoryIterator {
 public:
  virtual ~DirectoryIterator() = default;

  // ===================================================================
  // Iterator Operations
  // ===================================================================

  /**
   * @brief Check if there are more entries to read
   *
   * @return true if next() can be called to retrieve another entry
   *
   * @note This method does not advance the iterator position
   */
  virtual bool has_next() const = 0;

  /**
   * @brief Get the next directory entry
   *
   * Advances the iterator and returns the next entry.
   *
   * @return DirectoryEntry containing information about the next entry
   *
   * @throws std::runtime_error if has_next() is false
   *
   * @note Special entries "." and ".." are typically excluded by backends
   */
  virtual DirectoryEntry next() = 0;

  /**
   * @brief Reset the iterator to the beginning
   *
   * After reset(), has_next() will reflect the first entry again,
   * and next() will return the first entry.
   *
   * @note Not all backends may support efficient reset.
   *       If unsupported, this may recreate the iterator internally.
   */
  virtual void reset() = 0;
};

}  // namespace fs
}  // namespace tebako