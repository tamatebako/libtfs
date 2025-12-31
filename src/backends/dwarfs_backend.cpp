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

#include <tebako/fs/backends/dwarfs_backend.h>

#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>
#include <tebako/fs/internal/memory_file_view.h>

#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/internal/inode_reader_v2.h>
#include <dwarfs/file_stat.h>
#include <dwarfs/os_access_generic.h>
#include <dwarfs/logger.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <string>
#include <filesystem>

namespace tebako {
namespace fs {

// ===================================================================
// DwarfsFileHandle - Implementation of FileHandle for DwarFS archives
// ===================================================================

/**
 * @brief FileHandle implementation for DwarFS archive files
 *
 * Handles file reading from DwarFS archives using DwarFS v0.9+ reader.
 * Supports efficient seek operations natively.
 */
class DwarfsFileHandle : public FileHandle {
 public:
  /**
   * @brief Construct a file handle for a DwarFS file
   *
   * @param fs Reference to the DwarFS filesystem
   * @param inode The inode view for this file
   * @param path Full path to the file
   * @param size File size in bytes
   */
  DwarfsFileHandle(dwarfs::reader::filesystem_v2& fs,
                   dwarfs::reader::inode_view inode,
                   const std::string& path,
                   int64_t size)
      : fs_(fs),
        inode_(inode),
        path_(path),
        size_(size),
        current_pos_(0),
        eof_(false),
        closed_(false) {}

  /**
   * @brief Destructor - ensures file is closed
   */
  ~DwarfsFileHandle() override {
    close();
  }

  /**
   * @brief Read data from the file
   */
  ssize_t read(void* buffer, size_t count) override {
    if (closed_ || eof_ || current_pos_ >= size_) {
      if (!closed_) {
        eof_ = true;
      }
      return closed_ ? -1 : 0;
    }

    if (count == 0) {
      return 0;
    }

    // Calculate how much we can actually read
    size_t to_read = std::min(count, static_cast<size_t>(size_ - current_pos_));

    try {
      // Use DwarFS reader's read function
      int bytes_read = fs_.read(inode_.inode_num(),
                                static_cast<char*>(buffer),
                                to_read,
                                current_pos_);

      if (bytes_read < 0) {
        return -1;
      }

      current_pos_ += bytes_read;
      if (current_pos_ >= size_) {
        eof_ = true;
      }

      return static_cast<ssize_t>(bytes_read);
    } catch (...) {
      return -1;
    }
  }

  /**
   * @brief Seek to a position in the file
   *
   * DwarFS supports native seeking, so this is efficient.
   */
  off_t seek(off_t offset, int whence) override {
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

    current_pos_ = target_pos;
    eof_ = (current_pos_ >= size_);
    return current_pos_;
  }

  /**
   * @brief Get current position in the file
   */
  off_t tell() const override {
    return current_pos_;
  }

  /**
   * @brief Check if at end of file
   */
  bool eof() const override {
    return eof_;
  }

  /**
   * @brief Close the file handle
   */
  void close() override {
    closed_ = true;
  }

  /**
   * @brief Get the file path
   */
  std::string path() const override {
    return path_;
  }

  /**
   * @brief Get the file size
   */
  int64_t size() const override {
    return size_;
  }

 private:
  dwarfs::reader::filesystem_v2& fs_;  ///< DwarFS filesystem reference
  dwarfs::reader::inode_view inode_;   ///< Inode for this file
  std::string path_;                    ///< Full file path
  int64_t size_;                        ///< File size in bytes
  off_t current_pos_;                   ///< Current position in file
  bool eof_;                            ///< End-of-file flag
  bool closed_;                         ///< Closed flag
};

// ===================================================================
// DwarfsDirectoryIterator - Implementation of DirectoryIterator for DwarFS
// ===================================================================

/**
 * @brief DirectoryIterator implementation for DwarFS archives
 *
 * Iterates through directory entries in a DwarFS filesystem.
 */
class DwarfsDirectoryIterator : public DirectoryIterator {
 public:
  /**
   * @brief Construct a directory iterator for a DwarFS directory
   *
   * @param fs Reference to the DwarFS filesystem
   * @param inode The directory inode
   */
  DwarfsDirectoryIterator(dwarfs::reader::filesystem_v2& fs,
                          dwarfs::reader::inode_view dir_inode)
      : fs_(fs), current_index_(0) {

    try {
      // Get directory entries using DwarFS API
      auto dir = fs_.opendir(dir_inode);
      if (dir) {
        size_t offset = 0;
        while (true) {
          auto entry = fs_.readdir(*dir, offset++);
          if (!entry) break;  // End of directory

          // Skip ".", "..", and empty entries
          std::string name = entry->name();
          if (name.empty() || name == "." || name == "..") {
            continue;
          }

          DirectoryEntry de;
          de.name = name;

          // Get the inode for this entry
          auto child_inode = entry->inode();
          std::error_code ec;
          auto stat = fs_.getattr(child_inode, ec);

          if (!ec) {
            de.is_directory = S_ISDIR(stat.mode());
            de.size = stat.size();
            de.mtime = stat.mtime();
          }

          entries_.push_back(de);
        }
      }
    } catch (...) {
      // If we fail to read directory, leave entries empty
    }
  }

  /**
   * @brief Check if there are more entries
   */
  bool has_next() const override {
    return current_index_ < entries_.size();
  }

  /**
   * @brief Get the next directory entry
   */
  DirectoryEntry next() override {
    if (!has_next()) {
      throw std::runtime_error("No more directory entries");
    }
    return entries_[current_index_++];
  }

  /**
   * @brief Reset the iterator to the beginning
   */
  void reset() override {
    current_index_ = 0;
  }

 private:
  dwarfs::reader::filesystem_v2& fs_;   ///< DwarFS filesystem reference
  std::vector<DirectoryEntry> entries_;  ///< Directory entries
  size_t current_index_;                 ///< Current iteration position
};

// ===================================================================
// DwarfsBackend::Impl - PIMPL Implementation
// ===================================================================

/**
 * @brief PIMPL implementation for DwarfsBackend
 *
 * Hides DwarFS library details from the public interface.
 */
class DwarfsBackend::Impl {
 public:
  Impl() : is_mounted_(false), logger_(std::make_shared<dwarfs::stream_logger>()) {
    logger_->set_threshold(dwarfs::logger::INFO);
  }

  ~Impl() {
    unmount();
  }

  bool mount_file(const std::string& archive_path) {
    if (is_mounted_) {
      return false;
    }

    try {
      dwarfs::reader::filesystem_options opts;
      opts.image_offset = dwarfs::reader::filesystem_options::IMAGE_OFFSET_AUTO;

      dwarfs::os_access_generic os;

      // Create filesystem from file (opts passed as const&)
      fs_ = std::make_unique<dwarfs::reader::filesystem_v2>(
          *logger_, os, std::filesystem::path(archive_path), opts);

      is_mounted_ = true;
      return true;
    } catch (...) {
      return false;
    }
  }

  bool mount_memory(const void* data, size_t size) {
    if (is_mounted_) {
      return false;
    }

    try {
      dwarfs::reader::filesystem_options opts;
      opts.image_offset = 0;  // Memory buffer starts at offset 0

      // Create memory file view using our internal implementation
      auto mem_view = std::make_shared<tebako::memory_file_view_impl>(
          data, size, "/__tebako_dwarfs__");
      dwarfs::file_view view{mem_view};

      dwarfs::os_access_generic os;

      // Create filesystem from memory (opts passed as const&)
      fs_ = std::make_unique<dwarfs::reader::filesystem_v2>(
          *logger_, os, view, opts);

      is_mounted_ = true;
      return true;
    } catch (const std::exception& e) {
      // Log error for debugging
      return false;
    }
  }

  void unmount() {
    if (is_mounted_) {
      fs_.reset();
      is_mounted_ = false;
    }
  }

  bool is_mounted() const {
    return is_mounted_;
  }

  dwarfs::reader::filesystem_v2* get_fs() {
    return fs_.get();
  }

  std::optional<dwarfs::reader::inode_view> find_inode(const std::string& path) {
    if (!fs_) {
      return std::nullopt;
    }

    try {
      // Normalize path for DwarFS lookup
      std::string normalized = path;
      if (normalized.empty() || normalized == "/") {
        // Return root inode - find root directory
        auto entry = fs_->find("/");
        if (entry) {
          return entry->inode();
        }
        return std::nullopt;
      }

      // Remove leading slash
      if (normalized.front() == '/') {
        normalized = normalized.substr(1);
      }

      // Find the entry and extract inode
      auto entry = fs_->find(normalized);
      if (entry) {
        return entry->inode();
      }
      return std::nullopt;
    } catch (...) {
      return std::nullopt;
    }
  }

 private:
  bool is_mounted_;
  std::shared_ptr<dwarfs::stream_logger> logger_;
  std::unique_ptr<dwarfs::reader::filesystem_v2> fs_;
};

// ===================================================================
// DwarfsBackend Implementation
// ===================================================================

DwarfsBackend::DwarfsBackend() : impl_(std::make_unique<Impl>()) {}

DwarfsBackend::~DwarfsBackend() {
  unmount();
}

bool DwarfsBackend::mount(const std::string& archive_path,
                          const std::string& mount_point) {
  std::unique_lock lock(mutex_);

  if (impl_->is_mounted()) {
    return false;  // Already mounted
  }

  if (!impl_->mount_file(archive_path)) {
    return false;
  }

  archive_path_ = archive_path;
  mount_point_ = mount_point;
  return true;
}

bool DwarfsBackend::mount_from_memory(const void* data, size_t size,
                                       const std::string& mount_point) {
  std::unique_lock lock(mutex_);

  if (impl_->is_mounted()) {
    return false;  // Already mounted
  }

  if (!data || size == 0) {
    return false;  // Invalid parameters
  }

  if (!impl_->mount_memory(data, size)) {
    return false;
  }

  archive_path_ = "";  // Empty for memory mounts
  mount_point_ = mount_point;
  return true;
}

void DwarfsBackend::unmount() {
  std::unique_lock lock(mutex_);
  impl_->unmount();
  archive_path_.clear();
  mount_point_.clear();
}

bool DwarfsBackend::is_mounted() const {
  std::shared_lock lock(mutex_);
  return impl_->is_mounted();
}

std::unique_ptr<FileHandle> DwarfsBackend::open(const std::string& path,
                                                 int flags) {
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return nullptr;
  }

  // DwarFS backend is read-only - reject write flags
  if ((flags & O_WRONLY) || (flags & O_RDWR)) {
    return nullptr;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  auto inode_opt = impl_->find_inode(rel_path);
  if (!inode_opt) {
    return nullptr;
  }

  // Verify it's a file, not a directory
  try {
    std::error_code ec;
    auto stat = impl_->get_fs()->getattr(*inode_opt, ec);
    if (ec || !S_ISREG(stat.mode())) {
      return nullptr;  // Not a regular file
    }

    return std::make_unique<DwarfsFileHandle>(
        *impl_->get_fs(), *inode_opt, path, stat.size());
  } catch (...) {
    return nullptr;
  }
}

bool DwarfsBackend::exists(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  auto inode_opt = impl_->find_inode(rel_path);
  return inode_opt.has_value();
}

bool DwarfsBackend::is_file(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  auto inode_opt = impl_->find_inode(rel_path);
  if (!inode_opt) {
    return false;
  }

  try {
    std::error_code ec;
    auto stat = impl_->get_fs()->getattr(*inode_opt, ec);
    return !ec && S_ISREG(stat.mode());
  } catch (...) {
    return false;
  }
}

bool DwarfsBackend::is_directory(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  // Root directory always exists
  if (rel_path.empty() || rel_path == "/") {
    return true;
  }

  auto inode_opt = impl_->find_inode(rel_path);
  if (!inode_opt) {
    return false;
  }

  try {
    std::error_code ec;
    auto stat = impl_->get_fs()->getattr(*inode_opt, ec);
    return !ec && S_ISDIR(stat.mode());
  } catch (...) {
    return false;
  }
}

std::unique_ptr<DirectoryIterator> DwarfsBackend::list_directory(
    const std::string& path) {
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return nullptr;
  }

  if (!is_directory(path)) {
    return nullptr;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  auto inode_opt = impl_->find_inode(rel_path);
  if (!inode_opt) {
    return nullptr;
  }

  return std::make_unique<DwarfsDirectoryIterator>(*impl_->get_fs(), *inode_opt);
}

int64_t DwarfsBackend::file_size(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return -1;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  auto inode_opt = impl_->find_inode(rel_path);
  if (!inode_opt) {
    return -1;
  }

  try {
    std::error_code ec;
    auto stat = impl_->get_fs()->getattr(*inode_opt, ec);
    if (ec) {
      return -1;
    }
    // Return -1 for directories
    if (S_ISDIR(stat.mode())) {
      return -1;
    }
    return static_cast<int64_t>(stat.size());
  } catch (...) {
    return -1;
  }
}

time_t DwarfsBackend::modification_time(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return 0;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  auto inode_opt = impl_->find_inode(rel_path);
  if (!inode_opt) {
    return 0;
  }

  try {
    std::error_code ec;
    auto stat = impl_->get_fs()->getattr(*inode_opt, ec);
    return ec ? 0 : stat.mtime();
  } catch (...) {
    return 0;
  }
}

mode_t DwarfsBackend::permissions(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return 0;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  auto inode_opt = impl_->find_inode(rel_path);
  if (!inode_opt) {
    return 0;
  }

  try {
    std::error_code ec;
    auto stat = impl_->get_fs()->getattr(*inode_opt, ec);
    return ec ? 0 : (stat.mode() & 0777);
  } catch (...) {
    return 0;
  }
}

std::string DwarfsBackend::backend_version() const {
  // TODO: Get actual DwarFS version from library
  return "DwarFS v0.9+";
}

// ===================================================================
// Private Helper Methods
// ===================================================================

std::string DwarfsBackend::strip_mount_point(const std::string& path) const {
  if (path.size() < mount_point_.size()) {
    return path;
  }

  if (path.substr(0, mount_point_.size()) == mount_point_) {
    std::string result = path.substr(mount_point_.size());
    // Remove leading slash
    if (!result.empty() && result.front() == '/') {
      result = result.substr(1);
    }
    return result;
  }

  return path;
}

std::string DwarfsBackend::normalize_path(const std::string& path) const {
  std::string result = path;

  // Remove leading slash
  if (!result.empty() && result.front() == '/') {
    result = result.substr(1);
  }

  // Remove embedded "./" segments
  size_t pos = 0;
  while ((pos = result.find("/./", pos)) != std::string::npos) {
    result.replace(pos, 3, "/");
  }

  // Handle leading "./"
  if (result.size() >= 2 && result[0] == '.' && result[1] == '/') {
    result = result.substr(2);
  }

  // Remove trailing slash for non-root paths
  if (!result.empty() && result.back() == '/') {
    result = result.substr(0, result.size() - 1);
  }

  return result;
}

}  // namespace fs
}  // namespace tebako