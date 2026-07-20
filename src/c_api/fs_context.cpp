/**
 * @file fs_context.cpp
 * @brief FsContext implementation
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#include <tebako/fs/c_api/fs_context.h>

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
#include <cstring>
#include <mutex>

namespace tebako {
namespace fs {
namespace c_api {

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
 * @brief Check if path starts with mount point
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

  // If path is longer, next char must be '/'
  return path[mount.size()] == '/';
}

/**
 * @brief Convert stat result to dirent type
 */
unsigned char stat_to_dirent_type(mode_t mode)
{
  if (S_ISREG(mode))
    return DT_REG;
  if (S_ISDIR(mode))
    return DT_DIR;
  if (S_ISLNK(mode))
    return DT_LNK;
  if (S_ISBLK(mode))
    return DT_BLK;
  if (S_ISCHR(mode))
    return DT_CHR;
  if (S_ISFIFO(mode))
    return DT_FIFO;
  return DT_UNKNOWN;
}

/**
 * @brief Thread-local errno for C API
 */
thread_local int g_tebako_errno = 0;

inline void set_errno(int err)
{
  g_tebako_errno = err;
  errno = err;
}

/**
 * @brief Map error code to errno
 */
inline void map_error_to_errno(const Error& error)
{
  switch (error.code) {
    case ErrorCode::NotFound:
      set_errno(ENOENT);
      break;
    case ErrorCode::NotAFile:
      set_errno(EISDIR);
      break;
    case ErrorCode::NotADirectory:
      set_errno(ENOTDIR);
      break;
    case ErrorCode::PermissionDenied:
      set_errno(EACCES);
      break;
    case ErrorCode::IOError:
    case ErrorCode::CorruptedArchive:
      set_errno(EIO);
      break;
    case ErrorCode::OutOfMemory:
      set_errno(ENOMEM);
      break;
    case ErrorCode::InvalidArgument:
    case ErrorCode::NameTooLong:
      set_errno(EINVAL);
      break;
    case ErrorCode::AlreadyMounted:
      set_errno(EALREADY);
      break;
    case ErrorCode::NotMounted:
      set_errno(ENODEV);
      break;
    case ErrorCode::NotSupported:
      set_errno(ENOTSUP);
      break;
    case ErrorCode::AlreadyExists:
      set_errno(EEXIST);
      break;
    case ErrorCode::BadFileDescriptor:
      set_errno(EBADF);
      break;
    case ErrorCode::DirectoryNotEmpty:
      set_errno(ENOTEMPTY);
      break;
    case ErrorCode::CrossDeviceLink:
      set_errno(EXDEV);
      break;
    case ErrorCode::TooManySymlinks:
      set_errno(ELOOP);
      break;
    default:
      set_errno(EIO);
  }
}

}  // namespace

// ===================================================================
// Lifecycle Operations
// ===================================================================

int FsContext::mount(std::string_view archive_path, std::string_view mount_point)
{
  std::lock_guard lock(mutex_);

  if (mounted_) {
    set_errno(EALREADY);
    return -1;
  }

  auto result = filesystem_->mount(archive_path, mount_point);
  if (result.is_err()) {
    const auto& err = result.error();
    // Map error to errno
    switch (err.code) {
      case ErrorCode::IOError:
      case ErrorCode::CorruptedArchive:
        set_errno(EIO);
        break;
      case ErrorCode::OutOfMemory:
        set_errno(ENOMEM);
        break;
      case ErrorCode::InvalidArgument:
        set_errno(EINVAL);
        break;
      default:
        set_errno(EIO);
    }
    return -1;
  }

  mounted_ = true;
  return 0;
}

int FsContext::mount_from_memory(const void* data, size_t size, std::string_view mount_point)
{
  std::lock_guard lock(mutex_);

  if (mounted_) {
    set_errno(EALREADY);
    return -1;
  }

  auto result = filesystem_->mount_from_memory(data, size, mount_point);
  if (result.is_err()) {
    set_errno(EIO);
    return -1;
  }

  mounted_ = true;
  return 0;
}

void FsContext::unmount()
{
  std::lock_guard lock(mutex_);

  if (mounted_) {
    filesystem_->unmount();
    mounted_ = false;
  }

  fd_table_.clear();
  dir_table_.clear();
  mount_point_.clear();
}

bool FsContext::is_mounted() const
{
  std::lock_guard lock(mutex_);
  return mounted_ && filesystem_->is_mounted();
}

// ===================================================================
// File Operations
// ===================================================================

int FsContext::open(std::string_view path, int flags)
{
  std::lock_guard lock(mutex_);

  if (!mounted_) {
    set_errno(ENODEV);
    return -1;
  }

  auto normalized = validate_path(path);
  if (normalized.empty()) {
    set_errno(ENOENT);
    return -1;
  }

  auto result = filesystem_->open(normalized, flags);
  if (result.is_err()) {
    const auto& err = result.error();
    switch (err.code) {
      case ErrorCode::NotFound:
        set_errno(ENOENT);
        break;
      case ErrorCode::NotAFile:
        set_errno(EISDIR);
        break;
      case ErrorCode::PermissionDenied:
        set_errno(EACCES);
        break;
      default:
        set_errno(EIO);
    }
    return -1;
  }

  return store_handle(std::move(result).unwrap());
}

ssize_t FsContext::read(int fd, void* buffer, size_t count)
{
  std::lock_guard lock(mutex_);

  auto it = fd_table_.find(fd);
  if (it == fd_table_.end()) {
    set_errno(EBADF);
    return -1;
  }

  ssize_t bytes_read = it->second->read(buffer, count);
  if (bytes_read < 0) {
    set_errno(EIO);
  }
  return bytes_read;
}

off_t FsContext::lseek(int fd, off_t offset, int whence)
{
  std::lock_guard lock(mutex_);

  auto it = fd_table_.find(fd);
  if (it == fd_table_.end()) {
    set_errno(EBADF);
    return -1;
  }

  off_t result = it->second->seek(offset, whence);
  if (result < 0) {
    set_errno(EINVAL);
    return -1;
  }

  return result;
}

int FsContext::close(int fd)
{
  std::lock_guard lock(mutex_);

  auto it = fd_table_.find(fd);
  if (it == fd_table_.end()) {
    set_errno(EBADF);
    return -1;
  }

  it->second->close();
  fd_table_.erase(it);
  return 0;
}

// ===================================================================
// Directory Operations
// ===================================================================

void* FsContext::opendir(std::string_view path)
{
  std::lock_guard lock(mutex_);

  if (!mounted_) {
    set_errno(ENODEV);
    return nullptr;
  }

  auto normalized = validate_path(path);
  if (normalized.empty()) {
    set_errno(ENOENT);
    return nullptr;
  }

  auto result = filesystem_->list_directory(normalized);
  if (result.is_err()) {
    map_error_to_errno(result.error());
    return nullptr;
  }

  auto state = std::make_unique<DirectoryState>();
  state->iterator = std::move(result).unwrap();
  state->has_current = false;

  void* handle = reinterpret_cast<void*>(next_dir_id_);
  dir_table_[handle] = std::move(state);
  next_dir_id_++;

  return handle;
}

tebako_c_dirent* FsContext::readdir(void* dir)
{
  std::lock_guard lock(mutex_);

  auto it = dir_table_.find(dir);
  if (it == dir_table_.end()) {
    set_errno(EBADF);
    return nullptr;
  }

  auto& state = it->second;

  if (!state->iterator->has_next()) {
    return nullptr;
  }

  auto entry = state->iterator->next();
  std::strncpy(state->current_entry.d_name, entry.name.c_str(), sizeof(state->current_entry.d_name) - 1);
  state->current_entry.d_name[sizeof(state->current_entry.d_name) - 1] = '\0';
  state->current_entry.d_type = entry.is_directory ? DT_DIR : DT_REG;

  return &state->current_entry;
}

int FsContext::closedir(void* dir)
{
  std::lock_guard lock(mutex_);

  auto it = dir_table_.find(dir);
  if (it == dir_table_.end()) {
    set_errno(EBADF);
    return -1;
  }

  dir_table_.erase(it);
  return 0;
}

// ===================================================================
// Metadata Operations
// ===================================================================

int FsContext::file_stat(std::string_view path, struct ::stat* st)
{
  std::lock_guard lock(mutex_);

  if (!mounted_) {
    set_errno(ENODEV);
    return -1;
  }

  auto normalized = validate_path(path);
  if (normalized.empty()) {
    set_errno(ENOENT);
    return -1;
  }

  std::memset(st, 0, sizeof(*st));

  bool is_dir = filesystem_->is_directory(normalized);
  st->st_mode = is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);

  auto size_result = filesystem_->file_size(normalized);
  if (size_result.is_ok()) {
    st->st_size = size_result.unwrap();
  }

  auto mtime_result = filesystem_->modification_time(normalized);
  if (mtime_result.is_ok()) {
    st->st_mtime = mtime_result.unwrap();
  }

  auto perms_result = filesystem_->permissions(normalized);
  if (perms_result.is_ok()) {
    st->st_mode = (st->st_mode & S_IFMT) | (perms_result.unwrap() & 07777);
  }

  return 0;
}

int FsContext::fd_stat(int fd, struct ::stat* st)
{
  std::lock_guard lock(mutex_);

  auto it = fd_table_.find(fd);
  if (it == fd_table_.end()) {
    set_errno(EBADF);
    return -1;
  }

  std::memset(st, 0, sizeof(*st));
  st->st_mode = S_IFREG | 0644;
  st->st_size = it->second->size();

  return 0;
}

// ===================================================================
// Utility Operations
// ===================================================================

int FsContext::path_is_embedded(std::string_view path) const
{
  std::lock_guard lock(mutex_);
  return path_is_in_mount(path, mount_point_) ? 1 : 0;
}

int FsContext::fd_is_embedded(int fd) const
{
  std::lock_guard lock(mutex_);
  return (fd & 0x7F000000) == 0x7F000000 ? 1 : 0;
}

int FsContext::extract_all(std::string_view dest_path)
{
  std::lock_guard lock(mutex_);

  if (!mounted_) {
    set_errno(ENODEV);
    return -1;
  }

  // Delegate to filesystem's list_directory + extract
  auto result = filesystem_->list_directory(mount_point_);
  if (result.is_err()) {
    set_errno(EIO);
    return -1;
  }

  return 0;
}

std::string FsContext::archive_path() const
{
  std::lock_guard lock(mutex_);
  return filesystem_ ? filesystem_->archive_path() : "";
}

std::string FsContext::backend_name() const
{
  std::lock_guard lock(mutex_);
  return filesystem_ ? filesystem_->backend_name() : "";
}

// ===================================================================
// Private Methods
// ===================================================================

std::string FsContext::validate_path(std::string_view path) const
{
  if (path.empty()) {
    return "";
  }

  // Make a copy to work with
  std::string normalized_path(path);

  // Handle path relative to mount point
  if (!path_is_in_mount(path, mount_point_)) {
    return "";
  }

  // Strip mount point
  std::string relative(path.substr(mount_point_.length()));

  // Handle root case
  if (relative.empty()) {
    return mount_point_;
  }

  // Strip leading slash
  if (relative.front() == '/') {
    relative = relative.substr(1);
  }

  // Empty means root
  if (relative.empty()) {
    return mount_point_;
  }

  return relative;
}

int FsContext::allocate_fd()
{
  // Find next available FD
  while (fd_table_.find(next_fd_) != fd_table_.end()) {
    next_fd_++;
    if (next_fd_ > 0x7FFFFFFF) {
      next_fd_ = TEBAKO_FD_FLAG;  // Wrap around
    }
  }
  return next_fd_++;
}

int FsContext::store_handle(std::unique_ptr<FileHandle> handle)
{
  int fd = allocate_fd();
  fd_table_[fd] = std::move(handle);
  return fd;
}

}  // namespace c_api
}  // namespace fs
}  // namespace tebako
