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

#include <tebako/fs/c_api.h>  // For tebako_c_dirent, tebako_mount_t
#include <tebako/fs/filesystem.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>
#include <tebako/fs/core/error.h>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <string_view>

// System headers for C API types
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif

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
 * @brief Set the C API thread-local errno (and the C errno)
 *
 * Shared by fs_context.cpp and c_api.cpp so that all C API entry points
 * report through the single thread-local cell read by tebako_get_errno().
 */
void set_errno(int err);

/**
 * @brief Read the C API thread-local errno
 */
int get_errno();

/**
 * @brief Map a backend Error to the C API thread-local errno
 */
void map_error_to_errno(const Error& error);

/**
 * @brief One mounted archive in the mount table
 */
struct Mount {
  tebako_mount_t handle = -1;
  std::string mount_point;
  std::unique_ptr<FileSystem> fs;
  // Owned image region for offset/length mounts; mount_from_memory()
  // requires the buffer to outlive the mounted filesystem
  std::unique_ptr<char[]> owned_region;
};

/**
 * @brief File descriptor table entry (handle plus owning mount)
 */
struct FdEntry {
  std::unique_ptr<FileHandle> handle;
  tebako_mount_t owner = -1;
};

/**
 * @brief Directory state for C API directory iteration
 */
struct DirectoryState {
  std::unique_ptr<DirectoryIterator> iterator;
  tebako_c_dirent current_entry{};
  bool has_current = false;
  tebako_mount_t owner = -1;
  long position = 0;  // Ordinal of the entry the next readdir returns (telldir cookie)
};

/**
 * @brief Encapsulated filesystem context
 *
 * Manages all state for the C API:
 * - Mount table (handle -> Mount), supporting N concurrent mounts
 * - File descriptor table (single namespace, per-mount ownership)
 * - Directory handle table (single namespace, per-mount ownership)
 * - Compat handle for the single-mount tebako_fs_init* API
 *
 * Thread Safety: All methods are thread-safe; one mutex guards everything.
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
  // Multi-Mount Lifecycle Operations
  // ===================================================================

  /**
   * @brief Mount an archive from file
   * @param archive_path Path to archive file
   * @param mount_point Virtual mount point (must be non-empty)
   * @param out_handle Receives the mount handle on success
   * @return 0 on success, -1 on error (errno set)
   */
  int mount_from_file(std::string_view archive_path, std::string_view mount_point, tebako_mount_t* out_handle);

  /**
   * @brief Mount a region of an archive file
   * @param archive_path Path to the file containing the archive
   * @param offset Byte offset of the archive start within the file
   * @param length Length of the archive in bytes; 0 means "to end of file"
   * @param mount_point Virtual mount point (must be non-empty)
   * @param out_handle Receives the mount handle on success
   * @return 0 on success, -1 on error (errno set)
   */
  int mount_from_file_at(std::string_view archive_path,
                         uint64_t offset,
                         uint64_t length,
                         std::string_view mount_point,
                         tebako_mount_t* out_handle);

  /**
   * @brief Mount an archive from memory
   * @param data Pointer to archive data
   * @param size Size of archive data
   * @param mount_point Virtual mount point (must be non-empty)
   * @param out_handle Receives the mount handle on success
   * @return 0 on success, -1 on error (errno set)
   */
  int mount_from_memory(const void* data, size_t size, std::string_view mount_point, tebako_mount_t* out_handle);

  /**
   * @brief Unmount a single mount by handle
   *
   * Force-closes and erases only that mount's fds/dirs, destroys that
   * Filesystem, and erases the Mount. Other mounts are unaffected.
   * @param handle Mount handle
   * @return 0 on success, -1 with errno=ENODEV for unknown handle
   */
  int unmount_handle(tebako_mount_t handle);

  // ===================================================================
  // Compat Single-Mount Lifecycle (tebako_fs_init*)
  // ===================================================================

  /**
   * @brief Compat mount from file; fails with EEXIST if any mount exists
   */
  int init_from_file(std::string_view archive_path, std::string_view mount_point);

  /**
   * @brief Compat mount from a file region; EEXIST if any mount exists
   */
  int init_from_file_at(std::string_view archive_path, uint64_t offset, uint64_t length, std::string_view mount_point);

  /**
   * @brief Compat mount from memory; EEXIST if any mount exists
   */
  int init_from_memory(const void* data, size_t size, std::string_view mount_point);

  /**
   * @brief Unmount ALL mounts and clear the fd/dir tables
   */
  void unmount();

  /**
   * @brief Check if any filesystem is mounted
   */
  bool is_mounted() const;

  // ===================================================================
  // File Operations
  // ===================================================================

  /**
   * @brief Open a file
   * @param path Absolute path to file (dispatched by longest mount-point prefix)
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
   * @brief Read from file at a given offset (like POSIX pread)
   *
   * Dispatches to the owning mount of the fd. The fd's file position
   * is not modified.
   * @param fd File descriptor
   * @param buffer Buffer to read into
   * @param count Maximum bytes to read
   * @param offset Byte offset from the beginning of the file
   * @return Bytes read, 0 when offset is at/past EOF, -1 on error
   */
  ssize_t pread(int fd, void* buffer, size_t count, off_t offset);

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
   * @param path Absolute path to directory (dispatched by longest prefix)
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

  /**
   * @brief Registry-membership test for directory handles
   * @param dir Directory handle to check
   * @return 1 if dir is a live handle from opendir, 0 otherwise
   */
  int dir_is_embedded(void* dir) const;

  /**
   * @brief Reset a directory stream to the beginning (like rewinddir)
   * @param dir Directory handle from opendir
   */
  void rewinddir(void* dir);

  /**
   * @brief Current position cookie of a directory stream (like telldir)
   *
   * Index-based: ordinal of the entry the next readdir returns.
   * @param dir Directory handle from opendir
   * @return Position cookie, -1 on error (errno=EBADF)
   */
  long telldir(void* dir);

  /**
   * @brief Set a directory stream's position (like seekdir)
   *
   * Backward seeks reset the underlying iterator and advance; seeking
   * past the end leaves the stream at end-of-directory.
   * @param dir Directory handle from opendir
   * @param pos Position cookie from telldir (0 rewinds)
   */
  void seekdir(void* dir, long pos);

  // ===================================================================
  // Metadata Operations
  // ===================================================================

  /**
   * @brief Get file status
   * @param path Absolute path to file (dispatched by longest prefix)
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
   * @brief Check if path is within any mounted filesystem
   * @param path Path to check
   * @return 1 if embedded, 0 if not
   */
  int path_is_embedded(std::string_view path) const;

  /**
   * @brief Check if fd refers to embedded file
   * @param fd File descriptor
   * @return 1 if embedded, 0 if not
   */
  int fd_is_embedded(int fd) const;

  /**
   * @brief Extract all files to destination
   *
   * Single mount: the mount's tree is extracted directly into dest_path
   * (historic behavior). Multiple mounts: each mount's tree is extracted
   * into its own "<dest_path>/<mount-point-basename>" subtree.
   * @param dest_path Destination directory
   * @return 0 on success, -1 on error
   */
  int extract_all(std::string_view dest_path);

  /**
   * @brief Extract a memfs file to a host path for dlopen (dlmap2file)
   *
   * Modern variant of the legacy tebako_dlmap2file mechanism with the same
   * extraction/cache/lifetime semantics: the file is dispatched to its
   * owning mount (longest prefix), streamed to a per-process temporary
   * directory, and cached by memfs path until context teardown.
   * @param path Absolute path within a mounted filesystem
   * @return Newly allocated host path (caller frees with free()), nullptr
   *         on error (errno set)
   */
  char* dlmap2file(std::string_view path);

  // ===================================================================
  // Compat Accessor Methods (report the compat/first mount)
  // ===================================================================

  /**
   * @brief Mount point of the compat mount, nullptr when not compat-mounted
   *
   * The returned pointer is valid until the compat mount is unmounted.
   */
  const char* mount_point_c_str() const;

  /**
   * @brief Archive path of the compat mount ("" when none or memory-mounted)
   */
  std::string archive_path() const;

  /**
   * @brief Backend name of the compat mount ("" when not compat-mounted)
   */
  std::string backend_name() const;

 private:
  /**
   * @brief Private constructor - use instance()
   */
  FsContext();

  /**
   * @brief Find the mount owning a path (longest mount-point prefix wins)
   *
   * Caller must hold mutex_.
   */
  const Mount* find_mount(std::string_view path) const;

  /**
   * @brief Check if a mount point is already mounted
   *
   * Caller must hold mutex_.
   */
  bool mount_point_taken(std::string_view mount_point) const;

  /**
   * @brief Insert a fully mounted filesystem into the mount table
   *
   * Caller must hold mutex_.
   */
  int insert_mount(std::unique_ptr<FileSystem> fs,
                   std::string_view mount_point,
                   std::unique_ptr<char[]> owned_region,
                   tebako_mount_t* out_handle);

  /**
   * @brief Locked bodies shared by the multi-mount and compat entry points
   *
   * Caller must hold mutex_.
   */
  int mount_from_file_locked(std::string_view archive_path, std::string_view mount_point, tebako_mount_t* out_handle);
  int mount_from_file_at_locked(std::string_view archive_path,
                                uint64_t offset,
                                uint64_t length,
                                std::string_view mount_point,
                                tebako_mount_t* out_handle);
  int mount_from_memory_locked(const void* data, size_t size, std::string_view mount_point, tebako_mount_t* out_handle);

  /**
   * @brief Locked body of file_stat (fd_stat re-dispatches by path)
   *
   * Caller must hold mutex_.
   */
  int file_stat_locked(std::string_view path, struct ::stat* st);

  /**
   * @brief Look up a table entry for an external fd
   *
   * Caller must hold mutex_.
   */
  FdEntry* lookup_fd(int fd);

  /**
   * @brief Allocate new internal file descriptor
   *
   * Caller must hold mutex_.
   */
  int allocate_fd();

  // Member variables
  std::map<tebako_mount_t, Mount> mounts_;
  std::unordered_map<int, FdEntry> fd_table_;
  std::unordered_map<void*, std::unique_ptr<DirectoryState>> dir_table_;
  std::map<std::string, std::string> dl_cache_;  // memfs path -> extracted host file (dlmap2file)
  std::string dl_tmpdir_;                        // lazy per-process extraction directory
  tebako_mount_t compat_handle_ = -1;            // Mount created by tebako_fs_init* (-1 = none)
  tebako_mount_t next_handle_ = 0;               // Never reused within a process run
  int next_fd_ = 1;                              // Internal FD counter (external FDs OR TEBAKO_FD_FLAG)
  std::uintptr_t next_dir_id_ = 1;
  mutable std::mutex mutex_;
};

}  // namespace c_api
}  // namespace fs
}  // namespace tebako
