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

// System headers MUST come first to avoid conflicts
#include <mutex>
#include <memory>
#include <unordered_map>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <functional>
#include <fstream>
#include <filesystem>
#include <chrono>

#include <fcntl.h>
#include <sys/stat.h>
#ifndef _WIN32
// DT_* fallbacks for Windows live in <tebako/fs/c_api.h>; unistd.h is POSIX-only
#include <dirent.h>
#include <unistd.h>
#endif

// Now include tebako headers
#include <tebako/fs/c_api.h>
#include <tebako/fs/backend_factory.h>
#include <tebako/fs/filesystem.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>
#include <tebako/fs/core/error.h>

namespace {

// ===================================================================
// Global State
// ===================================================================

// Global filesystem instance
std::unique_ptr<tebako::fs::FileSystem> g_filesystem;
std::mutex g_init_mutex;
bool g_initialized = false;
std::string g_mount_point;

// FD table: internal FD -> FileHandle
std::unordered_map<int, std::unique_ptr<tebako::fs::FileHandle>> g_fd_table;
std::mutex g_fd_mutex;
int g_next_fd = 1;  // Internal FD counter (starts at 1)

// DIR handle table: opaque pointer -> DirectoryIterator + cached entry
struct DirectoryState {
  std::unique_ptr<tebako::fs::DirectoryIterator> iterator;
  tebako_c_dirent current_entry;  // Cached for tebako_readdir return
  bool has_current;
};
std::unordered_map<void*, std::unique_ptr<DirectoryState>> g_dir_table;
std::mutex g_dir_mutex;
std::uintptr_t g_next_dir_id = 1;  // For generating unique DIR handles

// Thread-local errno
thread_local int g_tebako_errno = 0;

// ===================================================================
// Helper Functions
// ===================================================================

/**
 * @brief Set thread-local errno
 */
inline void set_errno(int err)
{
  g_tebako_errno = err;
  errno = err;
}

/**
 * @brief Map ErrorCode to errno value
 */
void map_error_to_errno(const tebako::fs::Error& error)
{
  using namespace tebako::fs;

  switch (error.code) {
    case ErrorCode::NotFound:
      set_errno(ENOENT);
      break;
    case ErrorCode::NotMounted:
      set_errno(ENODEV);
      break;
    case ErrorCode::AlreadyMounted:
      set_errno(EALREADY);
      break;
    case ErrorCode::InvalidArgument:
      set_errno(EINVAL);
      break;
    case ErrorCode::NotAFile:
      set_errno(EISDIR);
      break;
    case ErrorCode::NotADirectory:
      set_errno(ENOTDIR);
      break;
    case ErrorCode::NotSupported:
      set_errno(ENOTSUP);
      break;
    case ErrorCode::IOError:
    case ErrorCode::CorruptedArchive:
      set_errno(EIO);
      break;
    case ErrorCode::OutOfMemory:
      set_errno(ENOMEM);
      break;
    case ErrorCode::PermissionDenied:
      set_errno(EACCES);
      break;
    case ErrorCode::AlreadyExists:
      set_errno(EEXIST);
      break;
    case ErrorCode::DirectoryNotEmpty:
      set_errno(ENOTEMPTY);
      break;
    case ErrorCode::NameTooLong:
      set_errno(ENAMETOOLONG);
      break;
    case ErrorCode::BadFileDescriptor:
      set_errno(EBADF);
      break;
    case ErrorCode::CrossDeviceLink:
      set_errno(EXDEV);
      break;
    case ErrorCode::TooManySymlinks:
      set_errno(ELOOP);
      break;
    default:
      set_errno(EIO);
      break;
  }
}

/**
 * @brief Check if path starts with mount point
 */
bool path_is_in_mount(const std::string& path, const std::string& mount)
{
  if (mount.empty())
    return false;
  if (path.size() < mount.size())
    return false;

  // Check if path starts with mount point
  if (path.compare(0, mount.size(), mount) != 0)
    return false;

  // If path is exactly the mount point, it's valid
  if (path.size() == mount.size())
    return true;

  // If path is longer, next char must be '/' (unless mount ends with '/')
  if (mount.back() == '/')
    return true;
  return path[mount.size()] == '/';
}

/**
 * @brief Allocate a new internal FD
 */
int allocate_fd()
{
  std::lock_guard<std::mutex> lock(g_fd_mutex);

  // Find next available FD
  while (g_fd_table.find(g_next_fd) != g_fd_table.end()) {
    g_next_fd++;
    if (g_next_fd > TEBAKO_FD_MAX) {
      g_next_fd = 1;  // Wrap around
    }
  }

  return g_next_fd++;
}

/**
 * @brief Store file handle and return external FD
 */
int store_handle(std::unique_ptr<tebako::fs::FileHandle> handle)
{
  int internal_fd = allocate_fd();

  std::lock_guard<std::mutex> lock(g_fd_mutex);
  g_fd_table[internal_fd] = std::move(handle);

  // Return external FD with flag set
  return internal_fd | TEBAKO_FD_FLAG;
}

/**
 * @brief Get file handle from FD
 */
tebako::fs::FileHandle* get_handle(int fd)
{
  if (!tebako_fd_is_embedded(fd)) {
    return nullptr;
  }

  int internal_fd = fd & ~TEBAKO_FD_FLAG;

  std::lock_guard<std::mutex> lock(g_fd_mutex);
  auto it = g_fd_table.find(internal_fd);
  if (it == g_fd_table.end()) {
    return nullptr;
  }

  return it->second.get();
}

/**
 * @brief Remove file handle from table
 */
bool remove_handle(int fd)
{
  if (!tebako_fd_is_embedded(fd)) {
    return false;
  }

  int internal_fd = fd & ~TEBAKO_FD_FLAG;

  std::lock_guard<std::mutex> lock(g_fd_mutex);
  return g_fd_table.erase(internal_fd) > 0;
}

/**
 * @brief Allocate directory handle
 */
void* allocate_dir_handle(std::unique_ptr<tebako::fs::DirectoryIterator> iter)
{
  std::lock_guard<std::mutex> lock(g_dir_mutex);

  auto state = std::make_unique<DirectoryState>();
  state->iterator = std::move(iter);
  state->has_current = false;

  // Generate unique pointer value
  void* handle = reinterpret_cast<void*>(g_next_dir_id++);
  g_dir_table[handle] = std::move(state);

  return handle;
}

/**
 * @brief Get directory state from handle
 */
DirectoryState* get_dir_state(void* dir)
{
  if (dir == nullptr)
    return nullptr;

  std::lock_guard<std::mutex> lock(g_dir_mutex);
  auto it = g_dir_table.find(dir);
  if (it == g_dir_table.end()) {
    return nullptr;
  }

  return it->second.get();
}

/**
 * @brief Remove directory handle
 */
bool remove_dir_handle(void* dir)
{
  if (dir == nullptr)
    return false;

  std::lock_guard<std::mutex> lock(g_dir_mutex);
  return g_dir_table.erase(dir) > 0;
}

/**
 * @brief Convert C++ exception to errno
 */
void handle_exception()
{
  try {
    throw;
  }
  catch (const std::bad_alloc&) {
    set_errno(ENOMEM);
  }
  catch (const std::invalid_argument&) {
    set_errno(EINVAL);
  }
  catch (const std::runtime_error&) {
    set_errno(EIO);
  }
  catch (...) {
    set_errno(EIO);
  }
}

}  // anonymous namespace

// ===================================================================
// Lifecycle Management
// ===================================================================

extern "C" int tebako_fs_init_from_file(const char* archive_path, const char* mount_point)
{
  if (archive_path == nullptr || mount_point == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  std::lock_guard<std::mutex> lock(g_init_mutex);

  if (g_initialized) {
    set_errno(EEXIST);
    return -1;
  }

  try {
    // Auto-detect format and create backend
    g_filesystem = tebako::fs::BackendFactory::create_from_file(archive_path);

    if (!g_filesystem) {
      set_errno(EINVAL);
      return -1;
    }

    // Mount filesystem
    if (!g_filesystem->mount(archive_path, mount_point)) {
      g_filesystem.reset();
      set_errno(EIO);
      return -1;
    }

    g_mount_point = mount_point;
    g_initialized = true;
    set_errno(0);
    return 0;
  }
  catch (...) {
    g_filesystem.reset();
    handle_exception();
    return -1;
  }
}

extern "C" int tebako_fs_init(const void* data, size_t size, const char* mount_point)
{
  if (data == nullptr || size == 0 || mount_point == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  std::lock_guard<std::mutex> lock(g_init_mutex);

  if (g_initialized) {
    set_errno(EEXIST);
    return -1;
  }

  try {
    // Auto-detect format from memory
    g_filesystem = tebako::fs::BackendFactory::create_from_memory(data, size);

    if (!g_filesystem) {
      set_errno(EINVAL);
      return -1;
    }

    // Mount from memory
    if (!g_filesystem->mount_from_memory(data, size, mount_point)) {
      g_filesystem.reset();
      set_errno(EIO);
      return -1;
    }

    g_mount_point = mount_point;
    g_initialized = true;
    set_errno(0);
    return 0;
  }
  catch (...) {
    g_filesystem.reset();
    handle_exception();
    return -1;
  }
}

extern "C" void tebako_fs_unmount(void)
{
  std::lock_guard<std::mutex> lock(g_init_mutex);

  if (!g_initialized) {
    return;
  }

  // Close all open file handles
  {
    std::lock_guard<std::mutex> fd_lock(g_fd_mutex);
    g_fd_table.clear();
    g_next_fd = 1;
  }

  // Close all directory handles
  {
    std::lock_guard<std::mutex> dir_lock(g_dir_mutex);
    g_dir_table.clear();
    g_next_dir_id = 1;
  }

  // Unmount and cleanup
  if (g_filesystem) {
    g_filesystem->unmount();
    g_filesystem.reset();
  }

  g_mount_point.clear();
  g_initialized = false;
}

extern "C" int tebako_is_initialized(void)
{
  std::lock_guard<std::mutex> lock(g_init_mutex);
  return g_initialized ? 1 : 0;
}

// ===================================================================
// File Operations
// ===================================================================

extern "C" int tebako_open(const char* path, int flags)
{
  if (path == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  if (!g_initialized) {
    set_errno(ENODEV);
    return -1;
  }

  // Only O_RDONLY is supported
  if ((flags & O_ACCMODE) != O_RDONLY) {
    set_errno(EROFS);
    return -1;
  }

  try {
    auto result = g_filesystem->open(path, flags);
    if (result.is_err()) {
      map_error_to_errno(result.error());
      return -1;
    }

    set_errno(0);
    return store_handle(std::move(result).unwrap());
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" ssize_t tebako_read(int fd, void* buf, size_t count)
{
  if (buf == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  auto* handle = get_handle(fd);
  if (handle == nullptr) {
    set_errno(EBADF);
    return -1;
  }

  try {
    ssize_t n = handle->read(buf, count);
    if (n < 0) {
      set_errno(EIO);
    }
    else {
      set_errno(0);
    }
    return n;
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" off_t tebako_lseek(int fd, off_t offset, int whence)
{
  auto* handle = get_handle(fd);
  if (handle == nullptr) {
    set_errno(EBADF);
    return -1;
  }

  try {
    off_t pos = handle->seek(offset, whence);
    if (pos < 0) {
      set_errno(EINVAL);
    }
    else {
      set_errno(0);
    }
    return pos;
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" int tebako_close(int fd)
{
  auto* handle = get_handle(fd);
  if (handle == nullptr) {
    set_errno(EBADF);
    return -1;
  }

  try {
    handle->close();
    remove_handle(fd);
    set_errno(0);
    return 0;
  }
  catch (...) {
    // Still remove from table even on error
    remove_handle(fd);
    handle_exception();
    return -1;
  }
}

// ===================================================================
// Directory Operations
// ===================================================================

extern "C" tebako_dir_t tebako_opendir(const char* path)
{
  if (path == nullptr) {
    set_errno(EINVAL);
    return nullptr;
  }

  if (!g_initialized) {
    set_errno(ENODEV);
    return nullptr;
  }

  try {
    auto result = g_filesystem->list_directory(path);
    if (result.is_err()) {
      map_error_to_errno(result.error());
      return nullptr;
    }

    set_errno(0);
    return allocate_dir_handle(std::move(result).unwrap());
  }
  catch (...) {
    handle_exception();
    return nullptr;
  }
}

extern "C" struct tebako_c_dirent* tebako_readdir(tebako_dir_t dir)
{
  auto* state = get_dir_state(dir);
  if (state == nullptr || state->iterator == nullptr) {
    set_errno(EBADF);
    return nullptr;
  }

  try {
    if (!state->iterator->has_next()) {
      // End of directory
      set_errno(0);
      return nullptr;
    }

    auto entry = state->iterator->next();

    // Fill in cached entry
    std::strncpy(state->current_entry.d_name, entry.name.c_str(), sizeof(state->current_entry.d_name) - 1);
    state->current_entry.d_name[sizeof(state->current_entry.d_name) - 1] = '\0';
    state->current_entry.d_type = entry.is_directory ? DT_DIR : DT_REG;
    state->has_current = true;

    set_errno(0);
    return &state->current_entry;
  }
  catch (...) {
    handle_exception();
    return nullptr;
  }
}

extern "C" int tebako_closedir(tebako_dir_t dir)
{
  if (!remove_dir_handle(dir)) {
    set_errno(EBADF);
    return -1;
  }

  set_errno(0);
  return 0;
}

// ===================================================================
// Metadata Operations
// ===================================================================

extern "C" int tebako_stat(const char* path, struct stat* st)
{
  if (path == nullptr || st == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  if (!g_initialized) {
    set_errno(ENODEV);
    return -1;
  }

  try {
    if (!g_filesystem->exists(path)) {
      set_errno(ENOENT);
      return -1;
    }

    // Clear stat structure
    std::memset(st, 0, sizeof(*st));

    // Get permissions
    auto perms_result = g_filesystem->permissions(path);
    if (perms_result.is_err()) {
      map_error_to_errno(perms_result.error());
      return -1;
    }
    mode_t perms = perms_result.unwrap();

    // Get modification time
    auto mtime_result = g_filesystem->modification_time(path);
    if (mtime_result.is_err()) {
      map_error_to_errno(mtime_result.error());
      return -1;
    }
    time_t mtime = mtime_result.unwrap();

    // Fill in fields
    if (g_filesystem->is_file(path)) {
      auto size_result = g_filesystem->file_size(path);
      if (size_result.is_err()) {
        map_error_to_errno(size_result.error());
        return -1;
      }
      st->st_mode = S_IFREG | perms;
      st->st_size = size_result.unwrap();
    }
    else if (g_filesystem->is_directory(path)) {
      st->st_mode = S_IFDIR | perms;
      st->st_size = 0;
    }
    else {
      set_errno(EINVAL);
      return -1;
    }

    st->st_mtime = mtime;
    st->st_nlink = 1;

    set_errno(0);
    return 0;
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" int tebako_fstat(int fd, struct stat* st)
{
  if (st == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  auto* handle = get_handle(fd);
  if (handle == nullptr) {
    set_errno(EBADF);
    return -1;
  }

  try {
    // Use path from handle to call stat
    return tebako_stat(handle->path().c_str(), st);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

// ===================================================================
// Path Detection
// ===================================================================

extern "C" int tebako_path_is_embedded(const char* path)
{
  if (path == nullptr) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(g_init_mutex);

  if (!g_initialized || g_mount_point.empty()) {
    return 0;
  }

  return path_is_in_mount(path, g_mount_point) ? 1 : 0;
}

extern "C" int tebako_fd_is_embedded(int fd)
{
  return (fd & TEBAKO_FD_FLAG) != 0 ? 1 : 0;
}

// ===================================================================
// Error Handling
// ===================================================================

extern "C" int tebako_get_errno(void)
{
  return g_tebako_errno;
}

extern "C" const char* tebako_strerror(int err)
{
  return std::strerror(err);
}

// ===================================================================
// Extraction
// ===================================================================

extern "C" int tebako_fs_extract_all(const char* dest_path)
{
  if (dest_path == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  if (!g_initialized) {
    set_errno(ENODEV);
    return -1;
  }

  try {
    // Create destination directory
    std::string dest_dir(dest_path);
    if (!std::filesystem::exists(dest_dir)) {
      if (!std::filesystem::create_directories(dest_dir)) {
        set_errno(EIO);
        return -1;
      }
    }

    // Helper function to recursively extract directory
    std::function<bool(const std::string&, const std::string&)> extract_dir;
    extract_dir = [&](const std::string& vfs_path, const std::string& disk_path) -> bool {
      // List directory contents
      auto iter_result = g_filesystem->list_directory(vfs_path);
      if (iter_result.is_err()) {
        return false;
      }
      auto iter = std::move(iter_result).unwrap();

      // Process each entry
      while (iter->has_next()) {
        auto entry = iter->next();
        std::string entry_vfs_path = vfs_path + "/" + entry.name;
        std::string entry_disk_path = disk_path + "/" + entry.name;

        if (entry.is_directory) {
          // Create directory if it doesn't exist
          if (!std::filesystem::exists(entry_disk_path)) {
            if (!std::filesystem::create_directories(entry_disk_path)) {
              return false;
            }
          }

          // Set directory permissions
          auto perms_result = g_filesystem->permissions(entry_vfs_path);
          if (perms_result.is_ok()) {
            mode_t perms = perms_result.unwrap();
            std::filesystem::permissions(entry_disk_path, static_cast<std::filesystem::perms>(perms),
                                         std::filesystem::perm_options::replace);
          }

          // Set directory modification time
          auto mtime_result = g_filesystem->modification_time(entry_vfs_path);
          if (mtime_result.is_ok()) {
            time_t mtime = mtime_result.unwrap();
            auto sys_time = std::chrono::system_clock::from_time_t(mtime);
            auto file_time = std::chrono::file_clock::from_sys(sys_time);
            std::filesystem::last_write_time(entry_disk_path, file_time);
          }

          // Recursively extract subdirectory
          if (!extract_dir(entry_vfs_path, entry_disk_path)) {
            return false;
          }
        }
        else {
          // Extract file
          auto handle_result = g_filesystem->open(entry_vfs_path, O_RDONLY);
          if (handle_result.is_err()) {
            return false;
          }
          auto handle = std::move(handle_result).unwrap();

          // Create output file
          std::ofstream out(entry_disk_path, std::ios::binary | std::ios::trunc);
          if (!out.is_open()) {
            return false;
          }

          // Copy data
          char buffer[8192];
          while (true) {
            ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
            if (bytes_read < 0) {
              out.close();
              return false;
            }
            if (bytes_read == 0) {
              break;  // EOF
            }
            out.write(buffer, bytes_read);
            if (!out.good()) {
              out.close();
              return false;
            }
          }
          out.close();

          // Set file permissions
          auto perms_result = g_filesystem->permissions(entry_vfs_path);
          if (perms_result.is_ok()) {
            mode_t perms = perms_result.unwrap();
            std::filesystem::permissions(entry_disk_path, static_cast<std::filesystem::perms>(perms),
                                         std::filesystem::perm_options::replace);
          }

          // Set file modification time
          auto mtime_result = g_filesystem->modification_time(entry_vfs_path);
          if (mtime_result.is_ok()) {
            time_t mtime = mtime_result.unwrap();
            auto sys_time = std::chrono::system_clock::from_time_t(mtime);
            auto file_time = std::chrono::file_clock::from_sys(sys_time);
            std::filesystem::last_write_time(entry_disk_path, file_time);
          }
        }
      }

      return true;
    };

    // Start extraction from root
    std::string root_path = g_mount_point.empty() ? "/" : g_mount_point;
    if (!extract_dir(root_path, dest_dir)) {
      set_errno(EIO);
      return -1;
    }

    set_errno(0);
    return 0;
  }
  catch (const std::filesystem::filesystem_error&) {
    set_errno(EIO);
    return -1;
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

// ===================================================================
// Utility Functions
// ===================================================================

extern "C" const char* tebako_get_mount_point(void)
{
  std::lock_guard<std::mutex> lock(g_init_mutex);

  if (!g_initialized || g_mount_point.empty()) {
    return nullptr;
  }

  return g_mount_point.c_str();
}

extern "C" const char* tebako_get_archive_path(void)
{
  std::lock_guard<std::mutex> lock(g_init_mutex);

  if (!g_initialized || !g_filesystem) {
    return nullptr;
  }

  const std::string& path = g_filesystem->archive_path();
  return path.empty() ? nullptr : path.c_str();
}

extern "C" const char* tebako_get_backend_name(void)
{
  std::lock_guard<std::mutex> lock(g_init_mutex);

  if (!g_initialized || !g_filesystem) {
    return nullptr;
  }

  // Store in static to ensure lifetime (thread-safe with lock)
  static std::string cached_name;
  cached_name = g_filesystem->backend_name();
  return cached_name.empty() ? nullptr : cached_name.c_str();
}