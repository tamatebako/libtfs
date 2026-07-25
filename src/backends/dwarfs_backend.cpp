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

// The ONLY dwarfs header consumed by libtfs: the stable C ABI reader
// binding (libdwarfs_c). No dwarfs C++ headers are included anywhere in
// libtfs; the C++ runtime stays inside the binding.
#include <dwarfs_c.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <string>

namespace tebako {
namespace fs {

// ===================================================================
// DwarfsFileHandle - Implementation of FileHandle for DwarFS archives
// ===================================================================

/**
 * @brief FileHandle implementation for DwarFS archive files
 *
 * Reads file data through the libdwarfs_c pread primitive.
 * Supports efficient seek operations natively.
 */
class DwarfsFileHandle : public FileHandle {
 public:
  /**
   * @brief Construct a file handle for a DwarFS file
   *
   * @param fs The libdwarfs_c filesystem handle (borrowed; must outlive
   *           this handle, i.e. the archive must stay mounted)
   * @param rel_path Path of the file relative to the archive root
   * @param path Full path to the file (as reported by path())
   * @param size File size in bytes
   */
  DwarfsFileHandle(dwarfs_c_filesystem* fs, std::string rel_path, std::string_view path, int64_t size)
      : fs_(fs), rel_path_(std::move(rel_path)), path_(path), size_(size), current_pos_(0), eof_(false), closed_(false)
  {
  }

  /**
   * @brief Destructor - ensures file is closed
   */
  ~DwarfsFileHandle() override { close(); }

  /**
   * @brief Read data from the file
   */
  ssize_t read(void* buffer, size_t count) override
  {
    if (closed_ || eof_ || current_pos_ >= size_) {
      if (!closed_) {
        eof_ = true;
      }
      return closed_ ? -1 : 0;
    }

    if (count == 0) {
      return 0;
    }

    // dwarfs_c_pread clamps the read to the end of the file
    int64_t bytes_read = dwarfs_c_pread(fs_, rel_path_.c_str(), buffer, count, current_pos_);
    if (bytes_read < 0) {
      return -1;
    }

    if (bytes_read == 0) {
      return 0;
    }

    current_pos_ += bytes_read;
    if (current_pos_ >= size_) {
      eof_ = true;
    }

    return static_cast<ssize_t>(bytes_read);
  }

  /**
   * @brief Read data at a given offset (POSIX pread semantics)
   *
   * Maps directly onto dwarfs_c_pread; the handle's current position and
   * eof state are not modified.
   */
  ssize_t pread(void* buffer, size_t count, off_t offset) override
  {
    if (closed_ || offset < 0) {
      return -1;
    }

    if (count == 0 || offset >= size_) {
      return 0;
    }

    int64_t bytes_read = dwarfs_c_pread(fs_, rel_path_.c_str(), buffer, count, offset);
    if (bytes_read < 0) {
      return -1;
    }
    return static_cast<ssize_t>(bytes_read);
  }

  /**
   * @brief Read data at a given offset (POSIX pread semantics)
   *
   * Uses the DwarFS reader's offset-based read directly; the handle's
   * current position and eof state are not modified.
   */
  ssize_t pread(void* buffer, size_t count, off_t offset) override
  {
    if (closed_ || offset < 0) {
      return -1;
    }

    if (count == 0 || offset >= size_) {
      return 0;
    }

    size_t to_read = std::min(count, static_cast<size_t>(size_ - offset));

    try {
      std::error_code ec;
      size_t bytes_read = fs_.read(inode_.inode_num(), static_cast<char*>(buffer), to_read, offset, ec);
      if (ec) {
        return -1;
      }
      return static_cast<ssize_t>(bytes_read);
    }
    catch (...) {
      return -1;
    }
  }

  /**
   * @brief Seek to a position in the file
   *
   * DwarFS supports native seeking, so this is efficient.
   */
  off_t seek(off_t offset, int whence) override
  {
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
  off_t tell() const override { return current_pos_; }

  /**
   * @brief Check if at end of file
   */
  bool eof() const override { return eof_; }

  /**
   * @brief Close the file handle
   */
  void close() override { closed_ = true; }

  /**
   * @brief Get the file path
   */
  std::string path() const override { return path_; }

  /**
   * @brief Get the file size
   */
  int64_t size() const override { return size_; }

 private:
  dwarfs_c_filesystem* fs_;  ///< libdwarfs_c filesystem handle (borrowed)
  std::string rel_path_;     ///< Path relative to the archive root
  std::string path_;         ///< Full file path
  int64_t size_;             ///< File size in bytes
  off_t current_pos_;        ///< Current position in file
  bool eof_;                 ///< End-of-file flag
  bool closed_;              ///< Closed flag
};

// ===================================================================
// DwarfsDirectoryIterator - Implementation of DirectoryIterator for DwarFS
// ===================================================================

/**
 * @brief DirectoryIterator implementation for DwarFS archives
 *
 * Iterates through directory entries in a DwarFS filesystem via the
 * libdwarfs_c directory iterator.
 */
class DwarfsDirectoryIterator : public DirectoryIterator {
 public:
  /**
   * @brief Construct a directory iterator for a DwarFS directory
   *
   * @param fs The libdwarfs_c filesystem handle (borrowed)
   * @param rel_path Directory path relative to the archive root
   *                 ("" denotes the root)
   */
  DwarfsDirectoryIterator(dwarfs_c_filesystem* fs, const std::string& rel_path)
  {
    dwarfs_c_dir* dir = dwarfs_c_opendir(fs, rel_path.c_str());
    if (dir == nullptr) {
      // Leave entries empty, mirroring the old catch-all behavior
      return;
    }

    dwarfs_c_dirent entry;
    while (dwarfs_c_readdir(dir, &entry) == 1) {
      DirectoryEntry de;
      de.name = entry.name;
      de.is_directory = (entry.type == DWARFS_C_FILE_DIRECTORY);
      de.size = 0;
      de.mtime = 0;

      // Per-entry metadata, best effort like the old per-entry getattr
      std::string child_path = rel_path.empty() ? de.name : rel_path + "/" + de.name;
      struct dwarfs_c_stat st;
      if (dwarfs_c_stat(fs, child_path.c_str(), &st) == 0) {
        de.size = st.size;
        de.mtime = static_cast<time_t>(st.mtime);
      }

      entries_.push_back(std::move(de));
    }

    dwarfs_c_closedir(dir);
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
  std::vector<DirectoryEntry> entries_;  ///< Directory entries
  size_t current_index_ = 0;             ///< Current iteration position
};

// ===================================================================
// DwarfsBackend::Impl - PIMPL Implementation
// ===================================================================

/**
 * @brief PIMPL implementation for DwarfsBackend
 *
 * Holds the libdwarfs_c filesystem handle, keeping all dwarfs details
 * out of the public interface.
 */
class DwarfsBackend::Impl {
 public:
  Impl() = default;

  ~Impl() { unmount(); }

  bool mount_file(const std::string& archive_path)
  {
    if (is_mounted_) {
      return false;
    }

    dwarfs_c_filesystem* fs = dwarfs_c_open(archive_path.c_str());
    if (fs == nullptr) {
      // Preserve the loader error so mount() can report it
      last_error_ = dwarfs_c_error_message();
      return false;
    }

    fs_ = fs;
    is_mounted_ = true;
    return true;
  }

  bool mount_memory(const void* data, size_t size)
  {
    if (is_mounted_) {
      return false;
    }

    if (!data || size == 0) {
      return false;
    }

    // The buffer is borrowed by libdwarfs_c, NOT copied; the caller keeps
    // it valid until unmount (unchanged mount_from_memory contract)
    dwarfs_c_filesystem* fs = dwarfs_c_open_memory(data, size);
    if (fs == nullptr) {
      // Preserve the loader error so mount_from_memory() can report it
      last_error_ = dwarfs_c_error_message();
      return false;
    }

    fs_ = fs;
    is_mounted_ = true;
    return true;
  }

  void unmount()
  {
    if (is_mounted_) {
      dwarfs_c_close(fs_);
      fs_ = nullptr;
      is_mounted_ = false;
    }
  }

  bool is_mounted() const { return is_mounted_; }

  const std::string& last_error() const { return last_error_; }

  dwarfs_c_filesystem* get_fs() { return fs_; }

  /**
   * @brief Stat a path relative to the archive root
   *
   * Wraps dwarfs_c_stat; "" and "/" resolve to the root directory.
   * On failure dwarfs_c_errno() yields the errno-style cause.
   */
  bool stat_path(const std::string& rel_path, struct dwarfs_c_stat* st) const
  {
    if (fs_ == nullptr) {
      return false;
    }
    return dwarfs_c_stat(fs_, rel_path.c_str(), st) == 0;
  }

 private:
  bool is_mounted_ = false;
  std::string last_error_;
  dwarfs_c_filesystem* fs_ = nullptr;
};

// ===================================================================
// DwarfsBackend Implementation
// ===================================================================

DwarfsBackend::DwarfsBackend() : impl_(std::make_unique<Impl>()) {}

DwarfsBackend::~DwarfsBackend()
{
  unmount();
}

Result<void> DwarfsBackend::mount(std::string_view archive_path, std::string_view mount_point)
{
  std::unique_lock lock(mutex_);

  if (impl_->is_mounted()) {
    return Err{ErrorCode::AlreadyMounted, "Filesystem already mounted"};
  }

  std::string archive_path_str(archive_path);
  if (!impl_->mount_file(archive_path_str)) {
    std::string message = "Failed to mount DwarFS archive";
    if (!impl_->last_error().empty()) {
      message += ": " + impl_->last_error();
    }
    return Err{ErrorCode::IOError, message, archive_path};
  }

  archive_path_ = archive_path_str;
  mount_point_ = std::string(mount_point);
  return make_ok();
}

Result<void> DwarfsBackend::mount_from_memory(const void* data, size_t size, std::string_view mount_point)
{
  std::unique_lock lock(mutex_);

  if (impl_->is_mounted()) {
    return Err{ErrorCode::AlreadyMounted, "Filesystem already mounted"};
  }

  if (!data || size == 0) {
    return Err{ErrorCode::InvalidArgument, "Invalid memory buffer parameters"};
  }

  if (!impl_->mount_memory(data, size)) {
    std::string message = "Failed to mount DwarFS archive from memory";
    if (!impl_->last_error().empty()) {
      message += ": " + impl_->last_error();
    }
    return Err{ErrorCode::IOError, message};
  }

  archive_path_ = "";  // Empty for memory mounts
  mount_point_ = std::string(mount_point);
  return make_ok();
}

void DwarfsBackend::unmount()
{
  std::unique_lock lock(mutex_);
  impl_->unmount();
  archive_path_.clear();
  mount_point_.clear();
}

bool DwarfsBackend::is_mounted() const
{
  std::shared_lock lock(mutex_);
  return impl_->is_mounted();
}

Result<std::unique_ptr<FileHandle>> DwarfsBackend::open(std::string_view path, int flags)
{
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  // DwarFS backend is read-only - reject write flags
  if ((flags & O_WRONLY) || (flags & O_RDWR)) {
    return Err{ErrorCode::NotSupported, "Write operations not supported for DwarFS archives"};
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  struct dwarfs_c_stat st;
  if (!impl_->stat_path(rel_path, &st)) {
    if (dwarfs_c_errno() == ENOENT) {
      return Err{ErrorCode::NotFound, "File not found", path};
    }
    return Err{ErrorCode::IOError, "Failed to get file attributes", path};
  }

  // Verify it's a file, not a directory
  if (!S_ISREG(st.mode)) {
    return Err{ErrorCode::NotAFile, "Path is not a regular file", path};
  }

  return Ok<std::unique_ptr<FileHandle>>{std::make_unique<DwarfsFileHandle>(impl_->get_fs(), rel_path, path, st.size)};
}

bool DwarfsBackend::exists(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  struct dwarfs_c_stat st;
  return impl_->stat_path(rel_path, &st);
}

bool DwarfsBackend::is_file(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return false;
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  struct dwarfs_c_stat st;
  return impl_->stat_path(rel_path, &st) && S_ISREG(st.mode);
}

bool DwarfsBackend::is_directory(std::string_view path) const
{
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

  struct dwarfs_c_stat st;
  return impl_->stat_path(rel_path, &st) && S_ISDIR(st.mode);
}

Result<std::unique_ptr<DirectoryIterator>> DwarfsBackend::list_directory(std::string_view path)
{
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  if (!is_directory(path)) {
    return Err{ErrorCode::NotADirectory, "Path is not a directory", path};
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  return Ok<std::unique_ptr<DirectoryIterator>>{std::make_unique<DwarfsDirectoryIterator>(impl_->get_fs(), rel_path)};
}

Result<int64_t> DwarfsBackend::file_size(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  struct dwarfs_c_stat st;
  if (!impl_->stat_path(rel_path, &st)) {
    if (dwarfs_c_errno() == ENOENT) {
      return Err{ErrorCode::NotFound, "File not found", path};
    }
    return Err{ErrorCode::IOError, "Failed to get file attributes", path};
  }

  if (S_ISDIR(st.mode)) {
    return Err{ErrorCode::NotAFile, "Path is a directory", path};
  }

  return Ok<int64_t>{st.size};
}

Result<time_t> DwarfsBackend::modification_time(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  struct dwarfs_c_stat st;
  if (!impl_->stat_path(rel_path, &st)) {
    if (dwarfs_c_errno() == ENOENT) {
      return Err{ErrorCode::NotFound, "Path not found", path};
    }
    return Err{ErrorCode::IOError, "Failed to get file attributes", path};
  }

  return Ok<time_t>{static_cast<time_t>(st.mtime)};
}

Result<mode_t> DwarfsBackend::permissions(std::string_view path) const
{
  std::shared_lock lock(mutex_);

  if (!impl_->is_mounted()) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = strip_mount_point(path);
  rel_path = normalize_path(rel_path);

  struct dwarfs_c_stat st;
  if (!impl_->stat_path(rel_path, &st)) {
    if (dwarfs_c_errno() == ENOENT) {
      return Err{ErrorCode::NotFound, "Path not found", path};
    }
    return Err{ErrorCode::IOError, "Failed to get file attributes", path};
  }

  return Ok<mode_t>{static_cast<mode_t>(st.mode & 0777)};
}

std::string DwarfsBackend::backend_version() const
{
  return dwarfs_c_version_string();
}

// ===================================================================
// Private Helper Methods
// ===================================================================

std::string DwarfsBackend::strip_mount_point(std::string_view path) const
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

std::string DwarfsBackend::normalize_path(std::string_view path) const
{
  std::string result(path);

  // Remove leading slash
  if (!result.empty() && result.front() == '/') {
    result.erase(0, 1);
  }

  // Remove embedded "./" segments
  size_t pos = 0;
  while ((pos = result.find("/./", pos)) != std::string::npos) {
    result.replace(pos, 3, "/");
  }

  // Handle leading "./"
  if (result.size() >= 2 && result[0] == '.' && result[1] == '/') {
    result.erase(0, 2);
  }

  // Remove trailing slash for non-root paths
  if (!result.empty() && result.back() == '/') {
    result.pop_back();
  }

  return result;
}

}  // namespace fs
}  // namespace tebako
