/**
 * @file fs_context.h
 * @brief FsContext - Encapsulated filesystem context for C API
 *
 * Provides a clean encapsulation of all filesystem state for the C API,
 * enabling better testability and cleaner architecture.
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#pragma once

#include <tebako/fs/c_api.h>  // For tebako_c_dirent
#include <tebako/fs/filesystem.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <string_view>

// System headers for C API types
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// DT_* constants may not be available on all platforms
#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#endif

namespace tebako {
namespace fs {
namespace c_api {

/**
 * @brief Directory state for C API directory iteration
 */
struct DirectoryState {
    std::unique_ptr<DirectoryIterator> iterator;
    tebako_c_dirent current_entry{};
    bool has_current = false;
};

/**
 * @brief Encapsulated filesystem context
 *
 * Manages all state for a single mounted filesystem including:
 * - Filesystem instance
 * - File descriptor table
 * - Directory handle table
 * - Mount point tracking
 *
 * Thread Safety: All methods are thread-safe using mutex.
 *
 * @note For C API compatibility, use FsContext::instance()
 * @note For testing, can create standalone instances
 */
class FsContext {
 public:
  /**
   * @brief Get singleton instance for C API
   */
  static FsContext& instance();

  /**
   * @brief Destructor - ensures cleanup
   */
  ~FsContext();

  // Non-copyable, non-movable
  FsContext(const FsContext&) = delete;
  FsContext& operator=(const FsContext&) = delete;
  FsContext(FsContext&&) = delete;
  FsContext& operator=(FsContext&&) = delete;

  // ===================================================================
  // Lifecycle Operations
  // ===================================================================

  /**
   * @brief Mount an archive from file
   * @param archive_path Path to archive file
   * @param mount_point Virtual mount point
   * @return 0 on success, -1 on error (errno set)
   */
  int mount(std::string_view archive_path, std::string_view mount_point);

  /**
   * @brief Mount an archive from memory
   * @param data Pointer to archive data
   * @param size Size of archive data
   * @param mount_point Virtual mount point
   * @return 0 on success, -1 on error (errno set)
   */
  int mount_from_memory(const void* data, size_t size, std::string_view mount_point);

  /**
   * @brief Unmount the current filesystem
   */
  void unmount();

  /**
   * @brief Check if filesystem is mounted
   */
  bool is_mounted() const;

  // ===================================================================
  // File Operations
  // ===================================================================

  /**
   * @brief Open a file
   * @param path Absolute path to file
   * @param flags Open flags
   * @return File descriptor on success, -1 on error
   */
  int open(std::string_view path, int flags);

  /**
   * @brief Read from file
   * @param fd File descriptor
   * @param buffer Buffer to read into
   * @param count Maximum bytes to read
   * @return Bytes read, 0 at EOF, -1 on error
   */
  ssize_t read(int fd, void* buffer, size_t count);

  /**
   * @brief Seek on file
   * @param fd File descriptor
   * @param offset Seek offset
   * @param whence Seek origin (SEEK_SET, SEEK_CUR, SEEK_END)
   * @return New position, -1 on error
   */
  off_t lseek(int fd, off_t offset, int whence);

  /**
   * @brief Close file
   * @param fd File descriptor
   * @return 0 on success, -1 on error
   */
  int close(int fd);

  // ===================================================================
  // Directory Operations
  // ===================================================================

  /**
   * @brief Open directory for reading
   * @param path Absolute path to directory
   * @return Directory handle on success, nullptr on error
   */
  void* opendir(std::string_view path);

  /**
   * @brief Read next directory entry
   * @param dir Directory handle from opendir
   * @return Pointer to entry, nullptr at end or error
   */
  tebako_c_dirent* readdir(void* dir);

  /**
   * @brief Close directory
   * @param dir Directory handle from opendir
   * @return 0 on success, -1 on error
   */
  int closedir(void* dir);

  // ===================================================================
  // Metadata Operations
  // ===================================================================

  /**
   * @brief Get file status
   * @param path Absolute path to file
   * @param st Stat buffer to fill
   * @return 0 on success, -1 on error
   */
  int file_stat(std::string_view path, struct ::stat* st);

  /**
   * @brief Get file status by fd
   * @param fd File descriptor
   * @param st Stat buffer to fill
   * @return 0 on success, -1 on error
   */
  int fd_stat(int fd, struct ::stat* st);

  // ===================================================================
  // Utility Operations
  // ===================================================================

  /**
   * @brief Check if path is within mounted filesystem
   * @param path Path to check
   * @return 1 if embedded, 0 if not
   */
  int path_is_embedded(std::string_view path) const;

  /**
   * @brief Check if fd refers to embedded file
   * @param fd File descriptor
   * @return 1 if embedded, 0 if not, -1 on error
   */
  int fd_is_embedded(int fd) const;

  /**
   * @brief Extract all files to destination
   * @param dest_path Destination directory
   * @return 0 on success, -1 on error
   */
  int extract_all(std::string_view dest_path);

  // ===================================================================
  // Accessor Methods
  // ===================================================================

  std::string mount_point() const { return mount_point_; }
  std::string archive_path() const;
  std::string backend_name() const;

 private:
  /**
   * @brief Private constructor - use instance()
   */
  FsContext();

  /**
   * @brief Validate and normalize path
   */
  std::string validate_path(std::string_view path) const;

  /**
   * @brief Allocate new file descriptor
   */
  int allocate_fd();

  /**
   * @brief Store handle and return FD
   */
  int store_handle(std::unique_ptr<FileHandle> handle);

  // Member variables
  std::unique_ptr<FileSystem> filesystem_;
  std::string mount_point_;
  std::unordered_map<int, std::unique_ptr<FileHandle>> fd_table_;
  std::unordered_map<void*, std::unique_ptr<DirectoryState>> dir_table_;
  int next_fd_ = TEBAKO_FD_FLAG;
  std::uintptr_t next_dir_id_ = 1;
  mutable std::mutex mutex_;
  bool mounted_ = false;
};

}  // namespace c_api
}  // namespace fs
}  // namespace tebako
