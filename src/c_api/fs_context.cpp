/**
 * @file fs_context.cpp
 * @brief FsContext implementation
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#include <tebako/fs/c_api/fs_context.h>

#include <tebako/fs/backend_factory.h>

#include <sys/stat.h>
#ifndef _WIN32
// DT_* fallbacks for Windows live in <tebako/fs/c_api/fs_context.h>
#include <dirent.h>
#endif
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>

namespace tebako {
namespace fs {
namespace c_api {

// ===================================================================
// Shared C API errno (single thread-local cell for the whole C API)
// ===================================================================

namespace {

thread_local int g_tebako_errno = 0;

}  // namespace

void set_errno(int err)
{
  g_tebako_errno = err;
  errno = err;
}

int get_errno()
{
  return g_tebako_errno;
}

void map_error_to_errno(const Error& error)
{
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

// ===================================================================
// FsContext Implementation
// ===================================================================

FsContext& FsContext::instance()
{
  static FsContext ctx;
  return ctx;
}

FsContext::~FsContext()
{
  unmount();
}

FsContext::FsContext() = default;

// ===================================================================
// Helper Functions
// ===================================================================

namespace {

/**
 * @brief Check if path starts with mount point (path component boundary)
 */
bool path_is_in_mount(std::string_view path, std::string_view mount)
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
 * @brief Basename of a mount point for per-mount extraction subtrees
 *
 * Strips trailing slashes and returns the last path component;
 * "root" when nothing usable remains (e.g. mount point "/").
 */
std::string mount_point_basename(std::string_view mount_point)
{
  std::string_view mp = mount_point;
  while (mp.size() > 1 && mp.back() == '/') {
    mp.remove_suffix(1);
  }
  size_t pos = mp.find_last_of('/');
  std::string_view base = (pos == std::string_view::npos) ? mp : mp.substr(pos + 1);
  return base.empty() ? std::string("root") : std::string(base);
}

/**
 * @brief Recursively extract a directory tree from a filesystem
 */
bool extract_dir_recursive(FileSystem& filesystem, const std::string& vfs_path, const std::string& disk_path)
{
  // List directory contents
  auto iter_result = filesystem.list_directory(vfs_path);
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

      // Set directory permissions (best effort: platform support for
      // POSIX metadata on extracted files varies, e.g. Windows cannot
      // set mtimes on directories via utime())
      auto perms_result = filesystem.permissions(entry_vfs_path);
      if (perms_result.is_ok()) {
        mode_t perms = perms_result.unwrap();
        std::error_code ec;
        std::filesystem::permissions(entry_disk_path, static_cast<std::filesystem::perms>(perms),
                                     std::filesystem::perm_options::replace, ec);
      }

      // Set directory modification time (best effort, see above)
      auto mtime_result = filesystem.modification_time(entry_vfs_path);
      if (mtime_result.is_ok()) {
        time_t mtime = mtime_result.unwrap();
        auto sys_time = std::chrono::system_clock::from_time_t(mtime);
        auto file_time = std::chrono::file_clock::from_sys(sys_time);
        std::error_code ec;
        std::filesystem::last_write_time(entry_disk_path, file_time, ec);
      }

      // Recursively extract subdirectory
      if (!extract_dir_recursive(filesystem, entry_vfs_path, entry_disk_path)) {
        return false;
      }
    }
    else {
      // Extract file
      auto handle_result = filesystem.open(entry_vfs_path, O_RDONLY);
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

      // Set file permissions (best effort, see directory branch above)
      auto perms_result = filesystem.permissions(entry_vfs_path);
      if (perms_result.is_ok()) {
        mode_t perms = perms_result.unwrap();
        std::error_code ec;
        std::filesystem::permissions(entry_disk_path, static_cast<std::filesystem::perms>(perms),
                                     std::filesystem::perm_options::replace, ec);
      }

      // Set file modification time (best effort, see directory branch above)
      auto mtime_result = filesystem.modification_time(entry_vfs_path);
      if (mtime_result.is_ok()) {
        time_t mtime = mtime_result.unwrap();
        auto sys_time = std::chrono::system_clock::from_time_t(mtime);
        auto file_time = std::chrono::file_clock::from_sys(sys_time);
        std::error_code ec;
        std::filesystem::last_write_time(entry_disk_path, file_time, ec);
      }
    }
  }

  return true;
}

}  // namespace

// ===================================================================
// Multi-Mount Lifecycle Operations
// ===================================================================

int FsContext::mount_from_file(std::string_view archive_path, std::string_view mount_point, tebako_mount_t* out_handle)
{
  if (mount_point.empty()) {
    set_errno(EINVAL);
    return -1;
  }

  std::lock_guard lock(mutex_);
  return mount_from_file_locked(archive_path, mount_point, out_handle);
}

int FsContext::mount_from_file_at(std::string_view archive_path,
                                  uint64_t offset,
                                  uint64_t length,
                                  std::string_view mount_point,
                                  tebako_mount_t* out_handle)
{
  if (mount_point.empty()) {
    set_errno(EINVAL);
    return -1;
  }

  std::lock_guard lock(mutex_);

  if (offset == 0 && length == 0) {
    return mount_from_file_locked(archive_path, mount_point, out_handle);
  }
  return mount_from_file_at_locked(archive_path, offset, length, mount_point, out_handle);
}

int FsContext::mount_from_memory(const void* data, size_t size, std::string_view mount_point, tebako_mount_t* out_handle)
{
  if (mount_point.empty()) {
    set_errno(EINVAL);
    return -1;
  }

  std::lock_guard lock(mutex_);
  return mount_from_memory_locked(data, size, mount_point, out_handle);
}

int FsContext::unmount_handle(tebako_mount_t handle)
{
  std::lock_guard lock(mutex_);

  auto it = mounts_.find(handle);
  if (it == mounts_.end()) {
    set_errno(ENODEV);
    return -1;
  }

  // Force-close and erase only this mount's fds/dirs; subsequent
  // read/readdir on them fails with EBADF
  for (auto fd_it = fd_table_.begin(); fd_it != fd_table_.end();) {
    if (fd_it->second.owner == handle) {
      fd_it = fd_table_.erase(fd_it);
    }
    else {
      ++fd_it;
    }
  }
  for (auto dir_it = dir_table_.begin(); dir_it != dir_table_.end();) {
    if (dir_it->second->owner == handle) {
      dir_it = dir_table_.erase(dir_it);
    }
    else {
      ++dir_it;
    }
  }

  it->second.fs->unmount();
  mounts_.erase(it);

  if (compat_handle_ == handle) {
    compat_handle_ = -1;
  }

  set_errno(0);
  return 0;
}

// ===================================================================
// Compat Single-Mount Lifecycle (tebako_fs_init*)
// ===================================================================

int FsContext::init_from_file(std::string_view archive_path, std::string_view mount_point)
{
  return init_from_file_at(archive_path, 0, 0, mount_point);
}

int FsContext::init_from_file_at(std::string_view archive_path,
                                 uint64_t offset,
                                 uint64_t length,
                                 std::string_view mount_point)
{
  std::lock_guard lock(mutex_);

  // init* stays single-mount: any existing mount makes it fail
  if (!mounts_.empty()) {
    set_errno(EEXIST);
    return -1;
  }

  tebako_mount_t handle = -1;
  int rc;
  if (offset == 0 && length == 0) {
    rc = mount_from_file_locked(archive_path, mount_point, &handle);
  }
  else {
    rc = mount_from_file_at_locked(archive_path, offset, length, mount_point, &handle);
  }
  if (rc == 0) {
    compat_handle_ = handle;
  }
  return rc;
}

int FsContext::init_from_memory(const void* data, size_t size, std::string_view mount_point)
{
  std::lock_guard lock(mutex_);

  // init* stays single-mount: any existing mount makes it fail
  if (!mounts_.empty()) {
    set_errno(EEXIST);
    return -1;
  }

  tebako_mount_t handle = -1;
  int rc = mount_from_memory_locked(data, size, mount_point, &handle);
  if (rc == 0) {
    compat_handle_ = handle;
  }
  return rc;
}

void FsContext::unmount()
{
  std::lock_guard lock(mutex_);

  for (auto& [handle, mount] : mounts_) {
    mount.fs->unmount();
  }
  mounts_.clear();

  // Close all open file handles and directory handles
  fd_table_.clear();
  dir_table_.clear();
  next_fd_ = 1;
  next_dir_id_ = 1;
  compat_handle_ = -1;
}

bool FsContext::is_mounted() const
{
  std::lock_guard lock(mutex_);
  return !mounts_.empty();
}

// ===================================================================
// File Operations
// ===================================================================

int FsContext::open(std::string_view path, int flags)
{
  std::lock_guard lock(mutex_);

  if (mounts_.empty()) {
    set_errno(ENODEV);
    return -1;
  }

  // Only O_RDONLY is supported
  if ((flags & O_ACCMODE) != O_RDONLY) {
    set_errno(EROFS);
    return -1;
  }

  const Mount* mount = find_mount(path);
  if (mount == nullptr) {
    set_errno(ENOENT);
    return -1;
  }

  auto result = mount->fs->open(path, flags);
  if (result.is_err()) {
    map_error_to_errno(result.error());
    return -1;
  }

  int internal_fd = allocate_fd();
  FdEntry entry;
  entry.handle = std::move(result).unwrap();
  entry.owner = mount->handle;
  fd_table_.emplace(internal_fd, std::move(entry));

  set_errno(0);
  return internal_fd | TEBAKO_FD_FLAG;
}

ssize_t FsContext::read(int fd, void* buffer, size_t count)
{
  std::lock_guard lock(mutex_);

  FdEntry* entry = lookup_fd(fd);
  if (entry == nullptr) {
    set_errno(EBADF);
    return -1;
  }

  ssize_t bytes_read = entry->handle->read(buffer, count);
  if (bytes_read < 0) {
    set_errno(EIO);
  }
  else {
    set_errno(0);
  }
  return bytes_read;
}

off_t FsContext::lseek(int fd, off_t offset, int whence)
{
  std::lock_guard lock(mutex_);

  FdEntry* entry = lookup_fd(fd);
  if (entry == nullptr) {
    set_errno(EBADF);
    return -1;
  }

  off_t result = entry->handle->seek(offset, whence);
  if (result < 0) {
    set_errno(EINVAL);
    return -1;
  }

  set_errno(0);
  return result;
}

int FsContext::close(int fd)
{
  std::lock_guard lock(mutex_);

  FdEntry* entry = lookup_fd(fd);
  if (entry == nullptr) {
    set_errno(EBADF);
    return -1;
  }

  entry->handle->close();
  fd_table_.erase(fd & ~TEBAKO_FD_FLAG);

  set_errno(0);
  return 0;
}

// ===================================================================
// Directory Operations
// ===================================================================

void* FsContext::opendir(std::string_view path)
{
  std::lock_guard lock(mutex_);

  if (mounts_.empty()) {
    set_errno(ENODEV);
    return nullptr;
  }

  const Mount* mount = find_mount(path);
  if (mount == nullptr) {
    set_errno(ENOENT);
    return nullptr;
  }

  auto result = mount->fs->list_directory(path);
  if (result.is_err()) {
    map_error_to_errno(result.error());
    return nullptr;
  }

  auto state = std::make_unique<DirectoryState>();
  state->iterator = std::move(result).unwrap();
  state->has_current = false;
  state->owner = mount->handle;

  void* handle = reinterpret_cast<void*>(next_dir_id_);
  dir_table_[handle] = std::move(state);
  next_dir_id_++;

  set_errno(0);
  return handle;
}

tebako_c_dirent* FsContext::readdir(void* dir)
{
  std::lock_guard lock(mutex_);

  auto it = dir_table_.find(dir);
  if (it == dir_table_.end() || it->second->iterator == nullptr) {
    set_errno(EBADF);
    return nullptr;
  }

  auto& state = it->second;

  if (!state->iterator->has_next()) {
    // End of directory
    set_errno(0);
    return nullptr;
  }

  auto entry = state->iterator->next();
  std::strncpy(state->current_entry.d_name, entry.name.c_str(), sizeof(state->current_entry.d_name) - 1);
  state->current_entry.d_name[sizeof(state->current_entry.d_name) - 1] = '\0';
  state->current_entry.d_type = entry.is_directory ? DT_DIR : DT_REG;
  state->has_current = true;

  set_errno(0);
  return &state->current_entry;
}

int FsContext::closedir(void* dir)
{
  std::lock_guard lock(mutex_);

  if (dir_table_.erase(dir) == 0) {
    set_errno(EBADF);
    return -1;
  }

  set_errno(0);
  return 0;
}

// ===================================================================
// Metadata Operations
// ===================================================================

int FsContext::file_stat(std::string_view path, struct ::stat* st)
{
  std::lock_guard lock(mutex_);

  if (mounts_.empty()) {
    set_errno(ENODEV);
    return -1;
  }

  return file_stat_locked(path, st);
}

int FsContext::fd_stat(int fd, struct ::stat* st)
{
  std::lock_guard lock(mutex_);

  FdEntry* entry = lookup_fd(fd);
  if (entry == nullptr) {
    set_errno(EBADF);
    return -1;
  }

  // Re-dispatch by the handle's path, like a plain tebako_fs_stat()
  std::string path = entry->handle->path();
  return file_stat_locked(path, st);
}

// ===================================================================
// Utility Operations
// ===================================================================

int FsContext::path_is_embedded(std::string_view path) const
{
  std::lock_guard lock(mutex_);
  return find_mount(path) != nullptr ? 1 : 0;
}

int FsContext::fd_is_embedded(int fd) const
{
  return (fd & TEBAKO_FD_FLAG) != 0 ? 1 : 0;
}

int FsContext::extract_all(std::string_view dest_path)
{
  std::lock_guard lock(mutex_);

  if (mounts_.empty()) {
    set_errno(ENODEV);
    return -1;
  }

  // Create destination directory
  std::string dest_dir(dest_path);
  if (!std::filesystem::exists(dest_dir)) {
    if (!std::filesystem::create_directories(dest_dir)) {
      set_errno(EIO);
      return -1;
    }
  }

  if (mounts_.size() == 1) {
    // Single mount: extract the tree directly into the destination
    // (historic tebako_fs_extract_all behavior)
    Mount& mount = mounts_.begin()->second;
    std::string root_path = mount.mount_point.empty() ? "/" : mount.mount_point;
    if (!extract_dir_recursive(*mount.fs, root_path, dest_dir)) {
      set_errno(EIO);
      return -1;
    }
  }
  else {
    // Multiple mounts: each mount's tree goes into its own
    // "<dest>/<mount-point-basename>" subtree
    for (auto& [handle, mount] : mounts_) {
      std::string subtree = dest_dir + "/" + mount_point_basename(mount.mount_point);
      if (!std::filesystem::exists(subtree)) {
        if (!std::filesystem::create_directories(subtree)) {
          set_errno(EIO);
          return -1;
        }
      }
      std::string root_path = mount.mount_point.empty() ? "/" : mount.mount_point;
      if (!extract_dir_recursive(*mount.fs, root_path, subtree)) {
        set_errno(EIO);
        return -1;
      }
    }
  }

  set_errno(0);
  return 0;
}

// ===================================================================
// Compat Accessor Methods
// ===================================================================

const char* FsContext::mount_point_c_str() const
{
  std::lock_guard lock(mutex_);

  auto it = mounts_.find(compat_handle_);
  if (it == mounts_.end() || it->second.mount_point.empty()) {
    return nullptr;
  }

  // std::map node storage is stable: valid until the mount is erased
  return it->second.mount_point.c_str();
}

std::string FsContext::archive_path() const
{
  std::lock_guard lock(mutex_);

  auto it = mounts_.find(compat_handle_);
  if (it == mounts_.end() || !it->second.fs) {
    return "";
  }

  return it->second.fs->archive_path();
}

std::string FsContext::backend_name() const
{
  std::lock_guard lock(mutex_);

  auto it = mounts_.find(compat_handle_);
  if (it == mounts_.end() || !it->second.fs) {
    return "";
  }

  return it->second.fs->backend_name();
}

// ===================================================================
// Private Methods
// ===================================================================

const Mount* FsContext::find_mount(std::string_view path) const
{
  const Mount* best = nullptr;
  for (const auto& [handle, mount] : mounts_) {
    if (path_is_in_mount(path, mount.mount_point) &&
        (best == nullptr || mount.mount_point.size() > best->mount_point.size())) {
      best = &mount;
    }
  }
  return best;
}

bool FsContext::mount_point_taken(std::string_view mount_point) const
{
  for (const auto& [handle, mount] : mounts_) {
    if (mount.mount_point == mount_point) {
      return true;
    }
  }
  return false;
}

int FsContext::insert_mount(std::unique_ptr<FileSystem> fs,
                            std::string_view mount_point,
                            std::unique_ptr<char[]> owned_region,
                            tebako_mount_t* out_handle)
{
  tebako_mount_t handle = next_handle_++;

  Mount mount;
  mount.handle = handle;
  mount.mount_point = std::string(mount_point);
  mount.fs = std::move(fs);
  mount.owned_region = std::move(owned_region);
  mounts_.emplace(handle, std::move(mount));

  *out_handle = handle;
  set_errno(0);
  return 0;
}

int FsContext::mount_from_file_locked(std::string_view archive_path,
                                      std::string_view mount_point,
                                      tebako_mount_t* out_handle)
{
  if (mount_point_taken(mount_point)) {
    set_errno(EEXIST);
    return -1;
  }

  // Whole-file mount: zero-copy path (backend reads/mmaps the file itself)
  auto filesystem = BackendFactory::create_from_file(std::string(archive_path));
  if (!filesystem) {
    set_errno(EINVAL);
    return -1;
  }

  auto result = filesystem->mount(archive_path, mount_point);
  if (result.is_err()) {
    set_errno(EIO);
    return -1;
  }

  return insert_mount(std::move(filesystem), mount_point, nullptr, out_handle);
}

int FsContext::mount_from_file_at_locked(std::string_view archive_path,
                                         uint64_t offset,
                                         uint64_t length,
                                         std::string_view mount_point,
                                         tebako_mount_t* out_handle)
{
  if (mount_point_taken(mount_point)) {
    set_errno(EEXIST);
    return -1;
  }

  // Region mount: read [offset, offset+length) into an owned buffer and
  // mount it from memory
  std::error_code ec;
  const uint64_t file_size = std::filesystem::file_size(std::string(archive_path), ec);
  if (ec) {
    set_errno(ENOENT);
    return -1;
  }

  if (offset > file_size) {
    set_errno(EINVAL);  // offset past end of file
    return -1;
  }

  uint64_t region = length;
  if (region == 0) {
    region = file_size - offset;  // 0 length = to end of file
  }
  else if (region > file_size - offset) {
    set_errno(EINVAL);  // offset+length extends past end of file
    return -1;
  }

  if (region == 0 || region > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
    set_errno(EINVAL);
    return -1;
  }

  auto buffer = std::make_unique<char[]>(static_cast<size_t>(region));

  std::ifstream ifs(std::string(archive_path), std::ios::binary);
  if (!ifs.is_open()) {
    set_errno(ENOENT);
    return -1;
  }
  ifs.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!ifs.good()) {
    set_errno(EIO);
    return -1;
  }
  ifs.read(buffer.get(), static_cast<std::streamsize>(region));
  if (ifs.gcount() != static_cast<std::streamsize>(region)) {
    set_errno(EIO);
    return -1;
  }
  ifs.close();

  auto filesystem = BackendFactory::create_from_memory(buffer.get(), static_cast<size_t>(region));
  if (!filesystem) {
    set_errno(EINVAL);
    return -1;
  }

  auto result = filesystem->mount_from_memory(buffer.get(), static_cast<size_t>(region), mount_point);
  if (result.is_err()) {
    set_errno(EIO);
    return -1;
  }

  // Backend mounted from memory: buffer must stay alive until unmount
  return insert_mount(std::move(filesystem), mount_point, std::move(buffer), out_handle);
}

int FsContext::mount_from_memory_locked(const void* data,
                                        size_t size,
                                        std::string_view mount_point,
                                        tebako_mount_t* out_handle)
{
  if (mount_point_taken(mount_point)) {
    set_errno(EEXIST);
    return -1;
  }

  // Auto-detect format from memory
  auto filesystem = BackendFactory::create_from_memory(data, size);
  if (!filesystem) {
    set_errno(EINVAL);
    return -1;
  }

  auto result = filesystem->mount_from_memory(data, size, mount_point);
  if (result.is_err()) {
    set_errno(EIO);
    return -1;
  }

  return insert_mount(std::move(filesystem), mount_point, nullptr, out_handle);
}

int FsContext::file_stat_locked(std::string_view path, struct ::stat* st)
{
  const Mount* mount = find_mount(path);
  if (mount == nullptr) {
    set_errno(ENOENT);
    return -1;
  }

  FileSystem* filesystem = mount->fs.get();

  if (!filesystem->exists(path)) {
    set_errno(ENOENT);
    return -1;
  }

  // Clear stat structure
  std::memset(st, 0, sizeof(*st));

  // Get permissions
  auto perms_result = filesystem->permissions(path);
  if (perms_result.is_err()) {
    map_error_to_errno(perms_result.error());
    return -1;
  }
  mode_t perms = perms_result.unwrap();

  // Get modification time
  auto mtime_result = filesystem->modification_time(path);
  if (mtime_result.is_err()) {
    map_error_to_errno(mtime_result.error());
    return -1;
  }
  time_t mtime = mtime_result.unwrap();

  // Fill in fields
  if (filesystem->is_file(path)) {
    auto size_result = filesystem->file_size(path);
    if (size_result.is_err()) {
      map_error_to_errno(size_result.error());
      return -1;
    }
    st->st_mode = S_IFREG | perms;
    st->st_size = size_result.unwrap();
  }
  else if (filesystem->is_directory(path)) {
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

FdEntry* FsContext::lookup_fd(int fd)
{
  if ((fd & TEBAKO_FD_FLAG) == 0) {
    return nullptr;
  }

  auto it = fd_table_.find(fd & ~TEBAKO_FD_FLAG);
  if (it == fd_table_.end()) {
    return nullptr;
  }

  return &it->second;
}

int FsContext::allocate_fd()
{
  // Find next available FD
  while (fd_table_.find(next_fd_) != fd_table_.end()) {
    next_fd_++;
    if (next_fd_ > TEBAKO_FD_MAX) {
      next_fd_ = 1;  // Wrap around
    }
  }
  return next_fd_++;
}

}  // namespace c_api
}  // namespace fs
}  // namespace tebako
