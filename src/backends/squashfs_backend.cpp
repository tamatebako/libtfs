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

#include <tebako/fs/backends/squashfs_backend.h>

#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <sqfs/sqfs.h>
#include <sqfs/io.h>
#include <sqfs/super.h>
#include <sqfs/compressor.h>
#include <sqfs/id_table.h>
#include <sqfs/inode.h>
#include <sqfs/dir_reader.h>
#include <sqfs/data_reader.h>

#include <fcntl.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace tebako {
namespace fs {

// ===================================================================
// SquashFSFileHandle - Implementation of FileHandle for SquashFS archives
// ===================================================================

/**
 * @brief FileHandle implementation for SquashFS archive files
 *
 * Handles file reading from SquashFS archives using squashfs-tools-ng.
 * SquashFS supports native seeking unlike ZIP.
 */
class SquashFSFileHandle : public FileHandle {
 public:
  /**
   * @brief Construct a file handle for a SquashFS file
   *
   * @param data_reader SquashFS data reader
   * @param inode File inode
   * @param path Full path to the file
   */
  SquashFSFileHandle(sqfs_data_reader_t* data_reader,
                     sqfs_inode_generic_t* inode,
                     const std::string& path)
      : data_reader_(data_reader),
        inode_(inode),
        path_(path),
        size_(0),
        current_pos_(0),
        eof_(false),
        buffer_(nullptr),
        buffer_size_(0) {
    if (!data_reader_ || !inode_) {
      throw std::invalid_argument("SquashFSFileHandle: invalid parameters");
    }

    // Get file size from inode
    size_ = static_cast<int64_t>(sqfs_inode_get_file_size(inode_));

    // Allocate read buffer
    buffer_size_ = 65536;  // 64KB buffer
    buffer_ = new char[buffer_size_];
  }

  /**
   * @brief Destructor - ensures resources are freed
   */
  ~SquashFSFileHandle() override {
    close();
  }

  /**
   * @brief Read data from the file
   */
  ssize_t read(void* buffer, size_t count) override {
    if (!data_reader_ || eof_) {
      return eof_ ? 0 : -1;
    }

    if (count == 0) {
      return 0;
    }

    // Don't read past end of file
    if (current_pos_ >= size_) {
      eof_ = true;
      return 0;
    }

    size_t to_read = count;
    if (current_pos_ + static_cast<off_t>(to_read) > size_) {
      to_read = size_ - current_pos_;
    }

    // Read from SquashFS using data reader
    sqfs_s32 bytes_read = sqfs_data_reader_read(data_reader_, inode_,
                                                  current_pos_, buffer, to_read);
    if (bytes_read < 0) {
      return -1;
    }

    current_pos_ += bytes_read;
    if (current_pos_ >= size_) {
      eof_ = true;
    }

    return static_cast<ssize_t>(bytes_read);
  }

  /**
   * @brief Seek to a position in the file
   *
   * SquashFS supports native seeking, so this is straightforward.
   */
  off_t seek(off_t offset, int whence) override {
    if (!data_reader_) {
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

    // Update position
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
    if (buffer_) {
      delete[] buffer_;
      buffer_ = nullptr;
    }
    data_reader_ = nullptr;
    inode_ = nullptr;
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
  sqfs_data_reader_t* data_reader_;  ///< SquashFS data reader
  sqfs_inode_generic_t* inode_;      ///< File inode
  std::string path_;                 ///< Full file path
  int64_t size_;                     ///< File size in bytes
  off_t current_pos_;                ///< Current position in file
  bool eof_;                         ///< End-of-file flag
  char* buffer_;                     ///< Read buffer
  size_t buffer_size_;               ///< Buffer size
};

// ===================================================================
// SquashFSDirectoryIterator - Implementation of DirectoryIterator for SquashFS
// ===================================================================

/**
 * @brief DirectoryIterator implementation for SquashFS archives
 *
 * Uses SquashFS dir_reader to iterate directory entries.
 */
class SquashFSDirectoryIterator : public DirectoryIterator {
 public:
  /**
   * @brief Construct a directory iterator for a SquashFS directory
   *
   * @param dir_reader SquashFS directory reader
   * @param inode Directory inode
   */
  SquashFSDirectoryIterator(sqfs_dir_reader_t* dir_reader,
                            sqfs_inode_generic_t* inode)
      : dir_reader_(dir_reader),
        inode_(inode),
        current_index_(0),
        state_(nullptr) {
    if (!dir_reader_ || !inode_) {
      return;
    }

    // Open directory for reading
    if (sqfs_dir_reader_open_dir(dir_reader_, inode_, 0) != 0) {
      return;
    }

    // Read all entries
    sqfs_dir_entry_t* entry;
    while (sqfs_dir_reader_read(dir_reader_, &entry) == 0) {
      if (!entry) {
        break;
      }

      DirectoryEntry dir_entry;
      dir_entry.name = std::string(entry->name);
      dir_entry.is_directory = (entry->type == SQFS_INODE_DIR ||
                                entry->type == SQFS_INODE_EXT_DIR);

      // Get inode for metadata
      sqfs_inode_generic_t* entry_inode;
      if (sqfs_dir_reader_get_inode(dir_reader_, entry, &entry_inode) == 0) {
        if (entry_inode) {
          dir_entry.size = static_cast<int64_t>(
              sqfs_inode_get_file_size(entry_inode));
          dir_entry.mtime = static_cast<time_t>(entry_inode->base.mod_time);
          free(entry_inode);
        }
      }

      entries_.push_back(dir_entry);
      free(entry);
    }
  }

  /**
   * @brief Destructor
   */
  ~SquashFSDirectoryIterator() override {
    // Nothing to clean up
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
  sqfs_dir_reader_t* dir_reader_;        ///< SquashFS directory reader
  sqfs_inode_generic_t* inode_;          ///< Directory inode
  std::vector<DirectoryEntry> entries_;  ///< Directory entries
  size_t current_index_;                 ///< Current iteration position
  void* state_;                          ///< Iterator state
};

// ===================================================================
// SquashFSBackend Implementation
// ===================================================================

SquashFSBackend::SquashFSBackend()
    : sqfs_file_(nullptr),
      sqfs_super_(nullptr),
      sqfs_dir_reader_(nullptr) {}

SquashFSBackend::~SquashFSBackend() {
  unmount();
}

bool SquashFSBackend::mount(const std::string& archive_path,
                            const std::string& mount_point) {
  std::unique_lock lock(mutex_);

  if (sqfs_file_) {
    return false;  // Already mounted
  }

  // Open SquashFS file
  sqfs_file_ = sqfs_open_file(archive_path.c_str(), SQFS_FILE_OPEN_READ_ONLY);
  if (!sqfs_file_) {
    return false;
  }

  // Read superblock
  sqfs_super_ = reinterpret_cast<sqfs_super_t*>(malloc(sizeof(sqfs_super_t)));
  if (sqfs_super_read(sqfs_super_, sqfs_file_) != 0) {
    sqfs_close(sqfs_file_);
    free(sqfs_super_);
    sqfs_file_ = nullptr;
    sqfs_super_ = nullptr;
    return false;
  }

  // Create directory reader
  sqfs_compressor_config_t cfg;
  sqfs_compressor_t* cmp = sqfs_compressor_create(&cfg);
  sqfs_id_table_t* id_tbl = sqfs_id_table_create(0);

  if (sqfs_id_table_read(id_tbl, sqfs_file_, sqfs_super_, cmp) != 0) {
    sqfs_drop(cmp);
    sqfs_drop(id_tbl);
    sqfs_close(sqfs_file_);
    free(sqfs_super_);
    sqfs_file_ = nullptr;
    sqfs_super_ = nullptr;
    return false;
  }

  sqfs_dir_reader_ = sqfs_dir_reader_create(sqfs_super_, cmp, sqfs_file_, id_tbl);
  sqfs_drop(cmp);
  sqfs_drop(id_tbl);

  if (!sqfs_dir_reader_) {
    sqfs_close(sqfs_file_);
    free(sqfs_super_);
    sqfs_file_ = nullptr;
    sqfs_super_ = nullptr;
    return false;
  }

  archive_path_ = archive_path;
  mount_point_ = mount_point;
  return true;
}

bool SquashFSBackend::mount_from_memory(const void* data, size_t size,
                                         const std::string& mount_point) {
  std::unique_lock lock(mutex_);

  if (is_mounted_) {
    return false;  // Already mounted
  }

  if (!data || size == 0) {
    return false;  // Invalid parameters
  }

  // Create a sqfs_file_t from memory buffer
  // Note: sqfs_file_open_memory expects non-const void*, so we need to cast
  // The library won't modify the data in read-only mode
  int ret = sqfs_file_open_memory(&file_, const_cast<void*>(data), size);
  if (ret != 0) {
    return false;
  }

  // Create and initialize reader
  reader_ = sqfs_reader_create();
  if (!reader_) {
    sqfs_destroy(reinterpret_cast<sqfs_object_t*>(file_));
    file_ = nullptr;
    return false;
  }

  // Open the reader with the memory-backed file
  ret = sqfs_reader_open(reader_, file_);
  if (ret != 0) {
    sqfs_destroy(reinterpret_cast<sqfs_object_t*>(reader_));
    sqfs_destroy(reinterpret_cast<sqfs_object_t*>(file_));
    reader_ = nullptr;
    file_ = nullptr;
    return false;
  }

  // Store details (archive_path is empty for memory mounts)
  archive_path_ = "";
  mount_point_ = mount_point;
  is_mounted_ = true;
  return true;
}

void SquashFSBackend::unmount() {
  std::unique_lock lock(mutex_);

  if (sqfs_dir_reader_) {
    sqfs_drop(sqfs_dir_reader_);
    sqfs_dir_reader_ = nullptr;
  }

  if (sqfs_super_) {
    free(sqfs_super_);
    sqfs_super_ = nullptr;
  }

  if (sqfs_file_) {
    sqfs_close(sqfs_file_);
    sqfs_file_ = nullptr;
  }

  archive_path_.clear();
  mount_point_.clear();
}

bool SquashFSBackend::is_mounted() const {
  std::shared_lock lock(mutex_);
  return sqfs_file_ != nullptr;
}

std::unique_ptr<FileHandle> SquashFSBackend::open(const std::string& path,
                                                  int flags) {
  std::shared_lock lock(mutex_);

  if (!sqfs_file_) {
    return nullptr;
  }

  // SquashFS backend is read-only - reject write flags
  if ((flags & O_WRONLY) || (flags & O_RDWR)) {
    return nullptr;
  }

  std::string rel_path = strip_mount_point(path);
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return nullptr;
  }

  // Verify it's a file, not a directory
  if (inode->base.type == SQFS_INODE_DIR ||
      inode->base.type == SQFS_INODE_EXT_DIR) {
    free(inode);
    return nullptr;
  }

  // Create data reader
  sqfs_compressor_config_t cfg;
  sqfs_compressor_t* cmp = sqfs_compressor_create(&cfg);
  sqfs_data_reader_t* data_reader = sqfs_data_reader_create(
      sqfs_file_, sqfs_super_->block_size, cmp, 0);
  sqfs_drop(cmp);

  if (!data_reader) {
    free(inode);
    return nullptr;
  }

  try {
    auto handle = std::make_unique<SquashFSFileHandle>(data_reader, inode, path);
    return handle;
  } catch (...) {
    sqfs_drop(data_reader);
    free(inode);
    return nullptr;
  }
}

bool SquashFSBackend::exists(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!sqfs_file_) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (inode) {
    free(inode);
    return true;
  }
  return false;
}

bool SquashFSBackend::is_file(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!sqfs_file_) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return false;
  }

  bool result = (inode->base.type != SQFS_INODE_DIR &&
                 inode->base.type != SQFS_INODE_EXT_DIR);
  free(inode);
  return result;
}

bool SquashFSBackend::is_directory(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!sqfs_file_) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);

  // Root directory always exists
  if (rel_path.empty() || rel_path == "/") {
    return true;
  }

  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return false;
  }

  bool result = (inode->base.type == SQFS_INODE_DIR ||
                 inode->base.type == SQFS_INODE_EXT_DIR);
  free(inode);
  return result;
}

std::unique_ptr<DirectoryIterator> SquashFSBackend::list_directory(
    const std::string& path) {
  std::shared_lock lock(mutex_);

  if (!sqfs_file_) {
    return nullptr;
  }

  if (!is_directory(path)) {
    return nullptr;
  }

  std::string rel_path = strip_mount_point(path);
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return nullptr;
  }

  return std::make_unique<SquashFSDirectoryIterator>(sqfs_dir_reader_, inode);
}

int64_t SquashFSBackend::file_size(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!sqfs_file_) {
    return -1;
  }

  std::string rel_path = strip_mount_point(path);
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return -1;
  }

  int64_t size = static_cast<int64_t>(sqfs_inode_get_file_size(inode));
  free(inode);
  return size;
}

time_t SquashFSBackend::modification_time(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!sqfs_file_) {
    return 0;
  }

  std::string rel_path = strip_mount_point(path);
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return 0;
  }

  time_t mtime = static_cast<time_t>(inode->base.mod_time);
  free(inode);
  return mtime;
}

mode_t SquashFSBackend::permissions(const std::string& path) const {
  std::shared_lock lock(mutex_);

  if (!sqfs_file_) {
    return 0;
  }

  std::string rel_path = strip_mount_point(path);
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return 0;
  }

  mode_t mode = static_cast<mode_t>(inode->base.mode);
  free(inode);
  return mode;
}

std::string SquashFSBackend::backend_version() const {
  // Return squashfs-tools-ng version
  return "squashfs-tools-ng 1.3.0";  // Update with actual version detection
}

// ===================================================================
// Private Helper Methods
// ===================================================================

sqfs_inode_generic_t* SquashFSBackend::lookup_inode(
    const std::string& path) const {
  if (!sqfs_dir_reader_) {
    return nullptr;
  }

  std::string normalized = normalize_path(path);

  // Root directory
  if (normalized.empty() || normalized == "/") {
    sqfs_inode_generic_t* root_inode;
    if (sqfs_dir_reader_get_root_inode(sqfs_dir_reader_, &root_inode) == 0) {
      return root_inode;
    }
    return nullptr;
  }

  // Lookup by path
  sqfs_inode_generic_t* inode;
  if (sqfs_dir_reader_find_by_path(sqfs_dir_reader_, normalized.c_str(),
                                     &inode) == 0) {
    return inode;
  }

  return nullptr;
}

std::string SquashFSBackend::strip_mount_point(const std::string& path) const {
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

std::string SquashFSBackend::normalize_path(const std::string& path) const {
  std::string result = path;

  // Remove leading slash
  if (!result.empty() && result.front() == '/') {
    result = result.substr(1);
  }

  // Remove trailing slash (unless root)
  if (result.size() > 1 && result.back() == '/') {
    result = result.substr(0, result.size() - 1);
  }

  return result;
}

}  // namespace fs
}  // namespace tebako