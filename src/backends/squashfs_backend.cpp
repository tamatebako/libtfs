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

#include <sqfs/io.h>
#include <sqfs/super.h>
#include <sqfs/compressor.h>
#include <sqfs/inode.h>
#include <sqfs/dir.h>
#include <sqfs/dir_reader.h>
#include <sqfs/data_reader.h>
#include <sqfs/error.h>

#include <fcntl.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace tebako {
namespace fs {

// ===================================================================
// Memory-backed sqfs_file_t (for mount_from_memory)
// ===================================================================

/**
 * @brief Read-only in-memory implementation of the libsquashfs file interface
 *
 * libsquashfs ships no memory-backed sqfs_file_t, so mounts from memory
 * (embedded archives) provide one here. The buffer is borrowed: the caller
 * must keep it valid until unmount(). Copies (sqfs_copy) reference the same
 * buffer; only the wrapper object is freed on destroy.
 */
struct sqfs_memory_file_t {
  sqfs_file_t base;   ///< Interface vtable, must be first member
  const sqfs_u8* data;  ///< Borrowed archive bytes
  sqfs_u64 size;      ///< Archive size in bytes
};

static int sqfs_memory_read_at(sqfs_file_t* file, sqfs_u64 offset, void* buffer, size_t size)
{
  auto* mf = reinterpret_cast<sqfs_memory_file_t*>(file);
  if (offset > mf->size || size > mf->size - offset) {
    return SQFS_ERROR_IO;
  }
  std::memcpy(buffer, mf->data + offset, size);
  return 0;
}

static int sqfs_memory_write_at(sqfs_file_t*, sqfs_u64, const void*, size_t)
{
  return SQFS_ERROR_UNSUPPORTED;
}

static sqfs_u64 sqfs_memory_get_size(const sqfs_file_t* file)
{
  return reinterpret_cast<const sqfs_memory_file_t*>(file)->size;
}

static int sqfs_memory_truncate(sqfs_file_t*, sqfs_u64)
{
  return SQFS_ERROR_UNSUPPORTED;
}

static void sqfs_memory_destroy(sqfs_object_t* obj)
{
  std::free(obj);
}

static sqfs_object_t* sqfs_memory_copy(const sqfs_object_t* orig)
{
  auto* dup = static_cast<sqfs_memory_file_t*>(std::malloc(sizeof(sqfs_memory_file_t)));
  if (dup != nullptr) {
    std::memcpy(dup, orig, sizeof(sqfs_memory_file_t));
  }
  return reinterpret_cast<sqfs_object_t*>(dup);
}

static sqfs_file_t* sqfs_memory_file_create(const void* data, size_t size)
{
  auto* mf = static_cast<sqfs_memory_file_t*>(std::malloc(sizeof(sqfs_memory_file_t)));
  if (mf == nullptr) {
    return nullptr;
  }
  mf->base.base.destroy = sqfs_memory_destroy;
  mf->base.base.copy = sqfs_memory_copy;
  mf->base.read_at = sqfs_memory_read_at;
  mf->base.write_at = sqfs_memory_write_at;
  mf->base.get_size = sqfs_memory_get_size;
  mf->base.truncate = sqfs_memory_truncate;
  mf->data = static_cast<const sqfs_u8*>(data);
  mf->size = static_cast<sqfs_u64>(size);
  return &mf->base;
}

// ===================================================================
// SquashFSFileHandle - Implementation of FileHandle for SquashFS archives
// ===================================================================

/**
 * @brief FileHandle implementation for SquashFS archive files
 *
 * Handles file reading from SquashFS archives using squashfs-tools-ng.
 * SquashFS supports native seeking unlike ZIP.
 *
 * Owns the per-handle data reader and inode passed to the constructor.
 */
class SquashFSFileHandle : public FileHandle {
 public:
  /**
   * @brief Construct a file handle for a SquashFS file
   *
   * @param data_reader SquashFS data reader (ownership transferred)
   * @param inode File inode (ownership transferred, freed with sqfs_free)
   * @param path Full path to the file
   * @param size File size in bytes
   */
  SquashFSFileHandle(sqfs_data_reader_t* data_reader, sqfs_inode_generic_t* inode, std::string_view path, int64_t size)
      : data_reader_(data_reader), inode_(inode), path_(path), size_(size), current_pos_(0), eof_(false), closed_(false)
  {
    if (!data_reader_ || !inode_) {
      throw std::invalid_argument("SquashFSFileHandle: invalid parameters");
    }
  }

  /**
   * @brief Destructor - ensures resources are freed
   */
  ~SquashFSFileHandle() override { close(); }

  /**
   * @brief Read data from the file
   */
  ssize_t read(void* buffer, size_t count) override
  {
    if (closed_) {
      return -1;
    }

    if (count == 0) {
      return 0;
    }

    // Don't read past end of file
    if (current_pos_ >= size_) {
      eof_ = true;
      return 0;
    }

    size_t to_read = std::min(count, static_cast<size_t>(size_ - current_pos_));

    // Read from SquashFS using data reader
    sqfs_s32 bytes_read = sqfs_data_reader_read(data_reader_, inode_, static_cast<sqfs_u64>(current_pos_), buffer,
                                                static_cast<sqfs_u32>(to_read));
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
  off_t seek(off_t offset, int whence) override
  {
    if (closed_) {
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
    if (data_reader_) {
      sqfs_destroy(data_reader_);
      data_reader_ = nullptr;
    }
    if (inode_) {
      sqfs_free(inode_);
      inode_ = nullptr;
    }
    closed_ = true;
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
  sqfs_data_reader_t* data_reader_;  ///< SquashFS data reader (owned)
  sqfs_inode_generic_t* inode_;      ///< File inode (owned)
  std::string path_;                 ///< Full file path
  int64_t size_;                     ///< File size in bytes
  off_t current_pos_;                ///< Current position in file
  bool eof_;                         ///< End-of-file flag
  bool closed_;                      ///< Closed flag
};

// ===================================================================
// SquashFSDirectoryIterator - Implementation of DirectoryIterator for SquashFS
// ===================================================================

/**
 * @brief DirectoryIterator implementation for SquashFS archives
 *
 * Uses the SquashFS dir_reader to iterate directory entries. All entries are
 * read eagerly at construction; the iterator then holds no libsquashfs state,
 * so it stays usable independently of further backend operations.
 */
class SquashFSDirectoryIterator : public DirectoryIterator {
 public:
  /**
   * @brief Construct a directory iterator for a SquashFS directory
   *
   * @param dir_reader SquashFS directory reader (borrowed, not owned)
   * @param inode Directory inode (borrowed, not owned)
   */
  SquashFSDirectoryIterator(sqfs_dir_reader_t* dir_reader, sqfs_inode_generic_t* inode)
      : current_index_(0)
  {
    if (!dir_reader || !inode) {
      return;
    }

    // Open directory for reading
    if (sqfs_dir_reader_open_dir(dir_reader, inode, 0) != 0) {
      return;
    }

    // Read all entries
    sqfs_dir_entry_t* entry = nullptr;
    while (sqfs_dir_reader_read(dir_reader, &entry) == 0) {
      DirectoryEntry dir_entry;
      // Entry name is not null-terminated; its length is stored off-by-one
      dir_entry.name.assign(reinterpret_cast<const char*>(entry->name), static_cast<size_t>(entry->size) + 1);
      dir_entry.is_directory = (entry->type == SQFS_INODE_DIR || entry->type == SQFS_INODE_EXT_DIR);

      // Get inode for metadata (refers to the entry just read)
      sqfs_inode_generic_t* entry_inode = nullptr;
      if (sqfs_dir_reader_get_inode(dir_reader, &entry_inode) == 0 && entry_inode) {
        sqfs_u64 entry_size = 0;
        if (sqfs_inode_get_file_size(entry_inode, &entry_size) == 0) {
          dir_entry.size = static_cast<int64_t>(entry_size);
        }
        dir_entry.mtime = static_cast<time_t>(entry_inode->base.mod_time);
        sqfs_free(entry_inode);
      }

      entries_.push_back(std::move(dir_entry));
      sqfs_free(entry);
      entry = nullptr;
    }
  }

  /**
   * @brief Destructor
   */
  ~SquashFSDirectoryIterator() override = default;

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
  size_t current_index_;                 ///< Current iteration position
};

// ===================================================================
// SquashFSBackend Implementation
// ===================================================================

SquashFSBackend::SquashFSBackend()
    : sqfs_file_(nullptr), sqfs_super_(nullptr), sqfs_cmp_(nullptr), sqfs_dir_reader_(nullptr)
{
}

SquashFSBackend::~SquashFSBackend()
{
  unmount();
}

Result<void> SquashFSBackend::mount(std::string_view archive_path, std::string_view mount_point)
{
  std::unique_lock lock(mutex_);

  if (sqfs_dir_reader_) {
    return Err{ErrorCode::AlreadyMounted, "Filesystem already mounted"};
  }

  std::string archive_path_str(archive_path);
  sqfs_file_t* file = sqfs_open_file(archive_path_str.c_str(), SQFS_FILE_OPEN_READ_ONLY);
  if (!file) {
    return Err{ErrorCode::IOError, "Failed to open SquashFS archive", archive_path};
  }

  return mount_common(file, archive_path, mount_point);
}

Result<void> SquashFSBackend::mount_from_memory(const void* data, size_t size, std::string_view mount_point)
{
  std::unique_lock lock(mutex_);

  if (sqfs_dir_reader_) {
    return Err{ErrorCode::AlreadyMounted, "Filesystem already mounted"};
  }

  if (!data || size == 0) {
    return Err{ErrorCode::InvalidArgument, "Invalid memory buffer parameters"};
  }

  sqfs_file_t* file = sqfs_memory_file_create(data, size);
  if (!file) {
    return Err{ErrorCode::OutOfMemory, "Failed to allocate memory-backed SquashFS file"};
  }

  return mount_common(file, "", mount_point);
}

Result<void> SquashFSBackend::mount_common(sqfs_file_t* file, std::string_view archive_path, std::string_view mount_point)
{
  // Read superblock
  auto* super = static_cast<sqfs_super_t*>(std::malloc(sizeof(sqfs_super_t)));
  if (!super) {
    sqfs_destroy(file);
    return Err{ErrorCode::OutOfMemory, "Failed to allocate SquashFS superblock"};
  }

  if (sqfs_super_read(super, file) != 0) {
    std::free(super);
    sqfs_destroy(file);
    return Err{ErrorCode::CorruptedArchive, "Failed to read SquashFS superblock", archive_path};
  }

  // Create the compressor matching the archive's compression id.
  // SQFS_COMP_FLAG_UNCOMPRESS is required: the backend only reads.
  sqfs_compressor_config_t cfg;
  if (sqfs_compressor_config_init(&cfg, static_cast<SQFS_COMPRESSOR>(super->compression_id), super->block_size,
                                  SQFS_COMP_FLAG_UNCOMPRESS) != 0) {
    std::free(super);
    sqfs_destroy(file);
    return Err{ErrorCode::NotSupported, "Unsupported SquashFS compressor configuration", archive_path};
  }

  sqfs_compressor_t* cmp = nullptr;
  if (sqfs_compressor_create(&cfg, &cmp) != 0 || !cmp) {
    std::free(super);
    sqfs_destroy(file);
    return Err{ErrorCode::NotSupported, "Unsupported SquashFS compressor", archive_path};
  }

  // Create directory reader
  sqfs_dir_reader_t* dir_reader = sqfs_dir_reader_create(super, cmp, file, 0);
  if (!dir_reader) {
    sqfs_destroy(cmp);
    std::free(super);
    sqfs_destroy(file);
    return Err{ErrorCode::OutOfMemory, "Failed to create SquashFS directory reader", archive_path};
  }

  // Prove the archive metadata is readable by resolving the root inode;
  // this catches corrupted images whose damage sits past the superblock
  sqfs_inode_generic_t* root_inode = nullptr;
  if (sqfs_dir_reader_get_root_inode(dir_reader, &root_inode) != 0) {
    sqfs_destroy(dir_reader);
    sqfs_destroy(cmp);
    std::free(super);
    sqfs_destroy(file);
    return Err{ErrorCode::CorruptedArchive, "Failed to read SquashFS root inode", archive_path};
  }
  sqfs_free(root_inode);

  sqfs_file_ = file;
  sqfs_super_ = super;
  sqfs_cmp_ = cmp;
  sqfs_dir_reader_ = dir_reader;
  archive_path_ = std::string(archive_path);
  mount_point_ = std::string(mount_point);
  return make_ok();
}

void SquashFSBackend::unmount()
{
  std::unique_lock lock(mutex_);

  if (sqfs_dir_reader_) {
    sqfs_destroy(sqfs_dir_reader_);
    sqfs_dir_reader_ = nullptr;
  }

  if (sqfs_cmp_) {
    sqfs_destroy(sqfs_cmp_);
    sqfs_cmp_ = nullptr;
  }

  if (sqfs_super_) {
    std::free(sqfs_super_);
    sqfs_super_ = nullptr;
  }

  if (sqfs_file_) {
    sqfs_destroy(sqfs_file_);
    sqfs_file_ = nullptr;
  }

  archive_path_.clear();
  mount_point_.clear();
}

bool SquashFSBackend::is_mounted() const
{
  std::shared_lock lock(mutex_);
  return sqfs_dir_reader_ != nullptr;
}

Result<std::unique_ptr<FileHandle>> SquashFSBackend::open(std::string_view path, int flags)
{
  std::unique_lock lock(mutex_);

  if (!sqfs_dir_reader_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  // SquashFS backend is read-only - reject write flags
  if ((flags & O_WRONLY) || (flags & O_RDWR)) {
    return Err{ErrorCode::NotSupported, "Write operations not supported for SquashFS archives"};
  }

  std::string rel_path = normalize_path(strip_mount_point(path));
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return Err{ErrorCode::NotFound, "File not found", path};
  }

  // Verify it's a regular file, not a directory
  if (inode->base.type == SQFS_INODE_DIR || inode->base.type == SQFS_INODE_EXT_DIR) {
    sqfs_free(inode);
    return Err{ErrorCode::NotAFile, "Path is not a regular file", path};
  }

  sqfs_u64 size = 0;
  int size_ret = sqfs_inode_get_file_size(inode, &size);
  if (size_ret != 0) {
    sqfs_free(inode);
    if (size_ret == SQFS_ERROR_NOT_FILE) {
      return Err{ErrorCode::NotAFile, "Path is not a regular file", path};
    }
    return Err{ErrorCode::IOError, "Failed to get file size", path};
  }

  // Each handle gets its own data reader so concurrent reads on distinct
  // handles never share mutable reader state
  sqfs_data_reader_t* data_reader = sqfs_data_reader_create(sqfs_file_, sqfs_super_->block_size, sqfs_cmp_, 0);
  if (!data_reader) {
    sqfs_free(inode);
    return Err{ErrorCode::OutOfMemory, "Failed to create SquashFS data reader", path};
  }

  // Fragment table is required to read files stored as fragments (small files)
  if (sqfs_data_reader_load_fragment_table(data_reader, sqfs_super_) != 0) {
    sqfs_destroy(data_reader);
    sqfs_free(inode);
    return Err{ErrorCode::CorruptedArchive, "Failed to load SquashFS fragment table", path};
  }

  return Ok<std::unique_ptr<FileHandle>>{std::make_unique<SquashFSFileHandle>(data_reader, inode, path,
                                                                              static_cast<int64_t>(size))};
}

bool SquashFSBackend::exists(std::string_view path) const
{
  std::unique_lock lock(mutex_);

  if (!sqfs_dir_reader_) {
    return false;
  }

  std::string rel_path = normalize_path(strip_mount_point(path));
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (inode) {
    sqfs_free(inode);
    return true;
  }
  return false;
}

bool SquashFSBackend::is_file(std::string_view path) const
{
  std::unique_lock lock(mutex_);

  if (!sqfs_dir_reader_) {
    return false;
  }

  std::string rel_path = normalize_path(strip_mount_point(path));
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return false;
  }

  bool result = (inode->base.type == SQFS_INODE_FILE || inode->base.type == SQFS_INODE_EXT_FILE);
  sqfs_free(inode);
  return result;
}

bool SquashFSBackend::is_directory(std::string_view path) const
{
  std::unique_lock lock(mutex_);

  if (!sqfs_dir_reader_) {
    return false;
  }

  std::string rel_path = normalize_path(strip_mount_point(path));

  // Root directory always exists
  if (rel_path.empty()) {
    return true;
  }

  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return false;
  }

  bool result = (inode->base.type == SQFS_INODE_DIR || inode->base.type == SQFS_INODE_EXT_DIR);
  sqfs_free(inode);
  return result;
}

Result<std::unique_ptr<DirectoryIterator>> SquashFSBackend::list_directory(std::string_view path)
{
  std::unique_lock lock(mutex_);

  if (!sqfs_dir_reader_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = normalize_path(strip_mount_point(path));
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return Err{ErrorCode::NotFound, "Directory not found", path};
  }

  if (inode->base.type != SQFS_INODE_DIR && inode->base.type != SQFS_INODE_EXT_DIR) {
    sqfs_free(inode);
    return Err{ErrorCode::NotADirectory, "Path is not a directory", path};
  }

  // The iterator reads all entries eagerly while the lock is held; the
  // directory inode is only needed during construction
  auto iter = std::make_unique<SquashFSDirectoryIterator>(sqfs_dir_reader_, inode);
  sqfs_free(inode);
  return Ok<std::unique_ptr<DirectoryIterator>>{std::move(iter)};
}

Result<int64_t> SquashFSBackend::file_size(std::string_view path) const
{
  std::unique_lock lock(mutex_);

  if (!sqfs_dir_reader_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = normalize_path(strip_mount_point(path));
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return Err{ErrorCode::NotFound, "File not found", path};
  }

  sqfs_u64 size = 0;
  int size_ret = sqfs_inode_get_file_size(inode, &size);
  sqfs_free(inode);
  if (size_ret != 0) {
    if (size_ret == SQFS_ERROR_NOT_FILE) {
      return Err{ErrorCode::NotAFile, "Path is not a regular file", path};
    }
    return Err{ErrorCode::IOError, "Failed to get file size", path};
  }
  return Ok<int64_t>{static_cast<int64_t>(size)};
}

Result<time_t> SquashFSBackend::modification_time(std::string_view path) const
{
  std::unique_lock lock(mutex_);

  if (!sqfs_dir_reader_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = normalize_path(strip_mount_point(path));
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return Err{ErrorCode::NotFound, "Path not found", path};
  }

  time_t mtime = static_cast<time_t>(inode->base.mod_time);
  sqfs_free(inode);
  return Ok<time_t>{mtime};
}

Result<mode_t> SquashFSBackend::permissions(std::string_view path) const
{
  std::unique_lock lock(mutex_);

  if (!sqfs_dir_reader_) {
    return Err{ErrorCode::NotMounted, "Filesystem not mounted"};
  }

  std::string rel_path = normalize_path(strip_mount_point(path));
  sqfs_inode_generic_t* inode = lookup_inode(rel_path);
  if (!inode) {
    return Err{ErrorCode::NotFound, "Path not found", path};
  }

  mode_t mode = static_cast<mode_t>(inode->base.mode & 0777);
  sqfs_free(inode);
  return Ok<mode_t>{mode};
}

std::string SquashFSBackend::backend_version() const
{
  return "squashfs-tools-ng 1.3.2";
}

// ===================================================================
// Private Helper Methods
// ===================================================================

sqfs_inode_generic_t* SquashFSBackend::lookup_inode(const std::string& path) const
{
  if (!sqfs_dir_reader_) {
    return nullptr;
  }

  // Root directory
  if (path.empty()) {
    sqfs_inode_generic_t* root_inode = nullptr;
    if (sqfs_dir_reader_get_root_inode(sqfs_dir_reader_, &root_inode) == 0) {
      return root_inode;
    }
    return nullptr;
  }

  // Lookup by path (nullptr start = from root)
  sqfs_inode_generic_t* inode = nullptr;
  if (sqfs_dir_reader_find_by_path(sqfs_dir_reader_, nullptr, path.c_str(), &inode) == 0) {
    return inode;
  }

  return nullptr;
}

std::string SquashFSBackend::strip_mount_point(std::string_view path) const
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

std::string SquashFSBackend::normalize_path(std::string_view path) const
{
  std::string result(path);

  // Remove leading slash
  if (!result.empty() && result.front() == '/') {
    result.erase(0, 1);
  }

  // Remove trailing slash (unless root)
  if (result.size() > 1 && result.back() == '/') {
    result.pop_back();
  }

  return result;
}

}  // namespace fs
}  // namespace tebako
