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

#include <tebako/fs/backends/zip_backend.h>

#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <zip.h>

#include <fcntl.h>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <string>

namespace tebako {
namespace fs {

// ===================================================================
// ZipFileHandle - Implementation of FileHandle for ZIP archives
// ===================================================================

/**
 * @brief FileHandle implementation for ZIP archive files
 *
 * Handles file reading from ZIP archives using libzip.
 * Implements seek by closing and reopening the file.
 */
class ZipFileHandle : public FileHandle {
 public:
  /**
   * @brief Construct a file handle for a ZIP entry
   *
   * @param archive ZIP archive handle
   * @param index ZIP entry index
   * @param path Full path to the file
   */
  ZipFileHandle(struct zip* archive, int64_t index, std::string_view path)
      : archive_(archive), index_(index), path_(path), file_(nullptr), size_(0), current_pos_(0), eof_(false)
  {
    if (!archive_) {
      throw std::invalid_argument("ZipFileHandle: archive is null");
    }

    // Get file size from ZIP entry statistics
    struct zip_stat stat;
    zip_stat_init(&stat);
    if (zip_stat_index(archive_, index_, 0, &stat) == 0) {
      if (stat.valid & ZIP_STAT_SIZE) {
        size_ = static_cast<int64_t>(stat.size);
      }
    }

    // Open the file for reading
    file_ = zip_fopen_index(archive_, index_, 0);
    if (!file_) {
      throw std::runtime_error("Failed to open ZIP entry");
    }
  }

  /**
   * @brief Destructor - ensures file is closed
   */
  ~ZipFileHandle() override { close(); }

  /**
   * @brief Read data from the file
   */
  ssize_t read(void* buffer, size_t count) override
  {
    if (!file_) {
      return -1;  // File is closed - return error
    }

    if (eof_) {
      return 0;  // At EOF - return 0
    }

    if (count == 0) {
      return 0;
    }

    zip_int64_t bytes_read = zip_fread(file_, buffer, count);
    if (bytes_read < 0) {
      return -1;
    }

    current_pos_ += bytes_read;
    if (bytes_read == 0 || current_pos_ >= size_) {
      eof_ = true;
    }

    return static_cast<ssize_t>(bytes_read);
  }

  /**
   * @brief Read data at a given offset (POSIX pread semantics)
   *
   * libzip streams are forward-only, so the read runs on an independent
   * temporary stream (open, skip to offset, read, close); the handle's
   * own stream, position, and eof state are not modified.
   */
  ssize_t pread(void* buffer, size_t count, off_t offset) override
  {
    if (!file_) {
      return -1;  // File is closed - return error
    }

    if (offset < 0) {
      return -1;
    }

    if (count == 0 || offset >= size_) {
      return 0;
    }

    size_t to_read = std::min(count, static_cast<size_t>(size_ - offset));

    zip_file_t* tmp = zip_fopen_index(archive_, index_, 0);
    if (!tmp) {
      return -1;
    }

    // Skip to the requested offset
    off_t remaining = offset;
    char skip[8192];
    while (remaining > 0) {
      zip_int64_t n = zip_fread(tmp, skip, std::min(remaining, static_cast<off_t>(sizeof(skip))));
      if (n <= 0) {
        zip_fclose(tmp);
        return -1;
      }
      remaining -= n;
    }

    size_t total = 0;
    while (total < to_read) {
      zip_int64_t n = zip_fread(tmp, static_cast<char*>(buffer) + total, to_read - total);
      if (n < 0) {
        zip_fclose(tmp);
        return -1;
      }
      if (n == 0) {
        break;  // EOF
      }
      total += static_cast<size_t>(n);
    }

    zip_fclose(tmp);
    return static_cast<ssize_t>(total);
  }

  /**
   * @brief Seek to a position in the file
   *
   * ZIP doesn't support native seeking, so we implement it by:
   * 1. Closing the current file handle
   * 2. Reopening the file
   * 3. Skipping to the desired position
   */
  off_t seek(off_t offset, int whence) override
  {
    if (!file_) {
      return -1;
    }

    // Calculate target position
    off_t target_pos = 0;
    switch (whence) {
      case SEEK_SET:
        target_pos = offset;
        break;
      case SEEK_CUR:
        target_pos = current_pos_ + offset;
        break;
      case SEEK_END:
        target_pos = size_ + offset;
        break;
      default:
        return -1;
    }

    // Validate target position
    if (target_pos < 0 || target_pos > size_) {
      return -1;
    }

    // If already at target position, nothing to do
    if (target_pos == current_pos_) {
      return current_pos_;
    }

    // Close current file handle
    zip_fclose(file_);
    file_ = nullptr;

    // Reopen the file
    file_ = zip_fopen_index(archive_, index_, 0);
    if (!file_) {
      return -1;
    }

    // Skip to target position
    current_pos_ = 0;
    eof_ = false;

    if (target_pos > 0) {
      const size_t BUFFER_SIZE = 8192;
      char buffer[BUFFER_SIZE];
      off_t remaining = target_pos;

      while (remaining > 0) {
        size_t to_read = std::min(remaining, static_cast<off_t>(BUFFER_SIZE));
        ssize_t bytes_read = read(buffer, to_read);
        if (bytes_read <= 0) {
          return -1;
        }
        remaining -= bytes_read;
      }
    }

    return current_pos_;
  }

  /**
   * @brief Get current position in the file
   */
  off_t tell() const override { return current_pos_; }

  /**
   * @brief Check if at end of file
   */
  bool eof() const override { return eof_; }

  /**
   * @brief Close the file handle
   */
  void close() override
  {
    if (file_) {
      zip_fclose(file_);
      file_ = nullptr;
    }
  }

  /**
   * @brief Get the file path
   */
  std::string path() const override { return path_; }

  /**
   * @brief Get the file size
   */
  int64_t size() const override { return size_; }

 private:
  struct zip* archive_;    ///< ZIP archive handle
  int64_t index_;          ///< ZIP entry index
  std::string path_;       ///< Full file path
  struct zip_file* file_;  ///< libzip file handle
  int64_t size_;           ///< File size in bytes
  off_t current_pos_;      ///< Current position in file
  bool eof_;               ///< End-of-file flag
};

// ===================================================================
// ZipDirectoryIterator - Implementation of DirectoryIterator for ZIP
// ===================================================================

/**
 * @brief DirectoryIterator implementation for ZIP archives
 *
 * Scans ZIP entries to build a directory listing.
 */
class ZipDirectoryIterator : public DirectoryIterator {
 public:
  /**
   * @brief Construct a directory iterator for a ZIP directory
   *
   * @param archive ZIP archive handle
   * @param dir_path Path to the directory (relative to archive root)
   */
  ZipDirectoryIterator(struct zip* archive, const std::string& dir_path) : current_index_(0)
  {
    if (!archive) {
      return;
    }

    // Normalize directory path (ensure it ends with '/')
    std::string normalized_dir = dir_path;
    if (!normalized_dir.empty() && normalized_dir.back() != '/') {
      normalized_dir += '/';
    }
    // Remove leading slash for ZIP entry comparison
    if (!normalized_dir.empty() && normalized_dir.front() == '/') {
      normalized_dir = normalized_dir.substr(1);
    }

    zip_int64_t num_entries = zip_get_num_entries(archive, 0);

    // Scan all ZIP entries to find immediate children
    for (zip_int64_t i = 0; i < num_entries; ++i) {
      const char* entry_name = zip_get_name(archive, i, 0);
      if (!entry_name) {
        continue;
      }

      std::string full_name(entry_name);

      // Skip if not in this directory
      if (normalized_dir.empty()) {
        // Root directory: only entries without '/' in the middle
        size_t slash_pos = full_name.find('/');
        if (slash_pos == std::string::npos) {
          // It's a file in root
          add_entry(archive, i, full_name, false);
        }
        else if (slash_pos == full_name.length() - 1) {
          // It's a top-level directory
          std::string name = full_name.substr(0, slash_pos);
          add_entry(archive, i, name, true);
        }
        else {
          // It's in a subdirectory - skip
          continue;
        }
      }
      else {
        // Non-root directory
        if (full_name.size() <= normalized_dir.size() || full_name.substr(0, normalized_dir.size()) != normalized_dir) {
          continue;  // Not in this directory
        }

        // Get the relative path within this directory
        std::string relative = full_name.substr(normalized_dir.size());

        // Check if it's an immediate child
        size_t slash_pos = relative.find('/');
        if (slash_pos == std::string::npos) {
          // It's a file in this directory
          add_entry(archive, i, relative, false);
        }
        else if (slash_pos == relative.length() - 1) {
          // It's a subdirectory (entry ends with '/')
          std::string name = relative.substr(0, slash_pos);
          add_entry(archive, i, name, true);
        }
        // Otherwise, it's in a deeper subdirectory - skip
      }
    }
  }

  /**
   * @brief Check if there are more entries
   */
  bool has_next() const override { return current_index_ < entries_.size(); }

  /**
   * @brief Get the next directory entry
   */
  DirectoryEntry next() override
  {
    if (!has_next()) {
      throw std::runtime_error("No more directory entries");
    }
    return entries_[current_index_++];
  }

  /**
   * @brief Reset the iterator to the beginning
   */
  void reset() override { current_index_ = 0; }

 private:
  /**
   * @brief Add an entry to the directory listing
   *
   * @param archive ZIP archive handle
   * @param index ZIP entry index
   * @param name Entry name (basename only)
   * @param is_dir Whether this is a directory
   */
  void add_entry(struct zip* archive, zip_int64_t index, const std::string& name, bool is_dir)
  {
    // Check if we already have this entry (duplicates can occur with
    // directories)
    for (const auto& entry : entries_) {
      if (entry.name == name) {
        return;  // Already added
      }
    }

    DirectoryEntry entry;
    entry.name = name;
    entry.is_directory = is_dir;

    // Get file statistics
    struct zip_stat stat;
    zip_stat_init(&stat);
    if (zip_stat_index(archive, index, 0, &stat) == 0) {
      if (stat.valid & ZIP_STAT_SIZE) {
        entry.size = static_cast<int64_t>(stat.size);
      }
      if (stat.valid & ZIP_STAT_MTIME) {
        entry.mtime = static_cast<time_t>(stat.mtime);
      }
    }

    entries_.push_back(entry);
  }

  std::vector<DirectoryEntry> entries_;  ///< Directory entries
  size_t current_index_;                 ///< Current iteration position
};

// ===================================================================
// ZipBackend Implementation
// ===================================================================

ZipBackend::ZipBackend() : archive_(nullptr) {}

ZipBackend::~ZipBackend()
{
  unmount();
}

Result<void> ZipBackend::mount(std::string_view archive_path, std::string_view mount_point)
{
  std::unique_lock lock(mutex_);

  if (archive_) {
    return Err{ErrorCode::AlreadyMounted, "Filesystem already mounted"};
  }

  int error = 0;
  std::string archive_path_str(archive_path);
  archive_ = zip_open(archive_path_str.c_str(), ZIP_RDONLY, &error);
  if (!archive_) {
    return Err{ErrorCode::IOError, "Failed to open ZIP archive", archive_path};
  }

  archive_path_ = archive_path_str;
  mount_point_ = std::string(mount_point);
  return make_ok();
}

Result<void> ZipBackend::mount_from_memory(const void* data, size_t size, std::string_view mount_point)
{
  std::unique_lock lock(mutex_);

  if (archive_) {
    return Err{ErrorCode::AlreadyMounted, "Filesystem already mounted"};
  }

  if (!data || size == 0) {
    return Err{ErrorCode::InvalidArgument, "Invalid memory buffer parameters"};
  }

  // Create zip source from memory buffer
  // Note: freep = 0 means we don't own the memory, caller must keep it valid
  zip_error_t error;
  zip_source_t* src = zip_source_buffer_create(data, size,
                                               0,  // freep = 0, we don't own the memory
                                               &error);

  if (!src) {
    return Err{ErrorCode::OutOfMemory, "Failed to create ZIP source from memory"};
  }

  // Open archive from source
  archive_ = zip_open_from_source(src, ZIP_RDONLY, &error);
  if (!archive_) {
    zip_source_free(src);
    return Err{ErrorCode::CorruptedArchive, "Failed to open ZIP archive from memory"};
  }

  // Store details (archive_path is empty for memory mounts)
  archive_path_ = "";
  mount_point_ = std::string(mount_point);
  return make_ok();
}

void ZipBackend::unmount()
{
  std::unique_lock lock(mutex_);

  if (archive_) {
    zip_close(archive_);
    archive_ = nullptr;
  }

  archive_path_.clear();
  mount_point_.clear();
}

bool ZipBackend::is_mounted() const
{
  std::shared_lock lock(mutex_);
  return archive_ != nullptr;
}

Result<std::unique_ptr<FileHandle>> ZipBackend::open(std::string_view path, int flags)
{
  std::shared_lock lock(mutex_);

  if (!archive_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  // ZIP backend is read-only - reject write flags
  if ((flags & O_WRONLY) || (flags & O_RDWR)) {
    return Err{ErrorCode::NotSupported, "Write operations not supported for ZIP archives"};
  }

  std::string rel_path = strip_mount_point(path);
  int64_t index = locate_entry(rel_path);
  if (index < 0) {
    return Err{ErrorCode::NotFound, "File not found", path};
  }

  // Verify it's a file, not a directory
  const char* entry_name = zip_get_name(archive_, index, 0);
  if (entry_name && entry_name[strlen(entry_name) - 1] == '/') {
    return Err{ErrorCode::NotAFile, "Path is a directory, not a file", path};
  }

  try {
    return Ok<std::unique_ptr<FileHandle>>{std::make_unique<ZipFileHandle>(archive_, index, path)};
  }
  catch (const std::exception& e) {
    return Err{ErrorCode::IOError, "Failed to open file", path};
  }
}

bool ZipBackend::exists(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!archive_) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);

  // Root directory always exists (mount point itself)
  if (rel_path.empty() || rel_path == "/") {
    return true;
  }

  return locate_entry(rel_path) >= 0;
}

bool ZipBackend::is_file(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!archive_) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);
  int64_t index = locate_entry(rel_path);
  if (index < 0) {
    return false;
  }

  const char* entry_name = zip_get_name(archive_, index, 0);
  if (!entry_name) {
    return false;
  }

  // Files don't end with '/'
  return entry_name[strlen(entry_name) - 1] != '/';
}

bool ZipBackend::is_directory(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!archive_) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);

  // Root directory always exists
  if (rel_path.empty() || rel_path == "/") {
    return true;
  }

  // Check if there's a directory entry
  std::string dir_path = rel_path;
  if (dir_path.back() != '/') {
    dir_path += '/';
  }
  int64_t index = locate_entry(dir_path);
  if (index >= 0) {
    return true;
  }

  // Check if any entries start with this path (implicit directory)
  if (dir_path.front() == '/') {
    dir_path = dir_path.substr(1);
  }

  zip_int64_t num_entries = zip_get_num_entries(archive_, 0);
  for (zip_int64_t i = 0; i < num_entries; ++i) {
    const char* entry_name = zip_get_name(archive_, i, 0);
    if (entry_name) {
      std::string name(entry_name);
      if (name.size() > dir_path.size() && name.substr(0, dir_path.size()) == dir_path) {
        return true;  // Found a child entry
      }
    }
  }

  return false;
}

Result<std::unique_ptr<DirectoryIterator>> ZipBackend::list_directory(std::string_view path)
{
  std::shared_lock lock(mutex_);

  if (!archive_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = strip_mount_point(path);

  // Root directory always exists
  if (rel_path.empty() || rel_path == "/") {
    return Ok<std::unique_ptr<DirectoryIterator>>{std::make_unique<ZipDirectoryIterator>(archive_, rel_path)};
  }

  // Check if path exists as a directory (with trailing slash)
  std::string dir_path = rel_path;
  if (dir_path.back() != '/') {
    dir_path += '/';
  }

  if (locate_entry(dir_path) >= 0) {
    // It's a directory, return the iterator
    return Ok<std::unique_ptr<DirectoryIterator>>{std::make_unique<ZipDirectoryIterator>(archive_, dir_path)};
  }

  // Check if path exists as a file (without trailing slash)
  if (locate_entry(rel_path) >= 0) {
    // Path exists but is a file, not a directory
    return Err{ErrorCode::NotADirectory, "Path is not a directory", path};
  }

  // Path doesn't exist at all
  return Err{ErrorCode::NotFound, "Path not found", path};
}

Result<int64_t> ZipBackend::file_size(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!archive_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = strip_mount_point(path);
  int64_t index = locate_entry(rel_path);
  if (index < 0) {
    return Err{ErrorCode::NotFound, "File not found", path};
  }

  struct zip_stat stat;
  zip_stat_init(&stat);
  if (zip_stat_index(archive_, index, 0, &stat) != 0) {
    return Err{ErrorCode::IOError, "Failed to get file stats", path};
  }

  if (stat.valid & ZIP_STAT_SIZE) {
    return Ok<int64_t>{static_cast<int64_t>(stat.size)};
  }

  return Err{ErrorCode::IOError, "File size not available", path};
}

Result<time_t> ZipBackend::modification_time(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!archive_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = strip_mount_point(path);
  int64_t index = locate_entry(rel_path);
  if (index < 0) {
    return Err{ErrorCode::NotFound, "File not found", path};
  }

  struct zip_stat stat;
  zip_stat_init(&stat);
  if (zip_stat_index(archive_, index, 0, &stat) != 0) {
    return Err{ErrorCode::IOError, "Failed to get file stats", path};
  }

  if (stat.valid & ZIP_STAT_MTIME) {
    return Ok<time_t>{static_cast<time_t>(stat.mtime)};
  }

  return Err{ErrorCode::IOError, "Modification time not available", path};
}

Result<mode_t> ZipBackend::permissions(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!archive_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  // Check if path exists
  if (!exists(path)) {
    return Err{ErrorCode::NotFound, "Path not found", path};
  }

  // ZIP archives don't reliably store POSIX permissions
  // Return default permissions
  if (is_directory(path)) {
    return Ok<mode_t>{0755};  // rwxr-xr-x for directories
  }
  else {
    return Ok<mode_t>{0644};  // rw-r--r-- for files
  }
}

std::string ZipBackend::backend_version() const
{
  return "libzip " + std::string(zip_libzip_version());
}

// ===================================================================
// Private Helper Methods
// ===================================================================

int64_t ZipBackend::locate_entry(std::string_view path) const
{
  if (!archive_) {
    return -1;
  }

  std::string normalized = normalize_path(path);

  // Try exact match first
  zip_int64_t index = zip_name_locate(archive_, normalized.c_str(), 0);
  if (index >= 0) {
    return static_cast<int64_t>(index);
  }

  // Try with trailing slash (for directories)
  if (!normalized.empty() && normalized.back() != '/') {
    std::string dir_path = normalized + '/';
    index = zip_name_locate(archive_, dir_path.c_str(), 0);
    if (index >= 0) {
      return static_cast<int64_t>(index);
    }
  }

  return -1;
}

std::string ZipBackend::strip_mount_point(std::string_view path) const
{
  if (path.size() < mount_point_.size()) {
    return std::string(path);
  }

  if (path.substr(0, mount_point_.size()) == mount_point_) {
    std::string result(path.substr(mount_point_.size()));
    // Remove leading slash
    if (!result.empty() && result.front() == '/') {
      result.erase(0, 1);
    }
    return result;
  }

  return std::string(path);
}

std::string ZipBackend::normalize_path(std::string_view path) const
{
  std::string result(path);

  // Remove leading slash
  if (!result.empty() && result.front() == '/') {
    result.erase(0, 1);
  }

  return result;
}

bool ZipBackend::is_directory_entry(std::string_view path) const
{
  return !path.empty() && path.back() == '/';
}

}  // namespace fs
}  // namespace tebako