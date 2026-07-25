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

// Modern C API entry points. These are thin extern "C" wrappers around
// tebako::fs::c_api::FsContext, which holds the mount table, the fd/dir
// tables, and the shared thread-local errno cell.

// System headers MUST come first to avoid conflicts
#include <cerrno>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#ifndef _WIN32
// DT_* fallbacks for Windows live in <tebako/fs/c_api.h>; unistd.h is POSIX-only
#include <dirent.h>
#include <unistd.h>
#endif

// Now include tebako headers
#include <tebako/fs/c_api.h>
#include <tebako/fs/c_api/fs_context.h>

using tebako::fs::c_api::FsContext;
using tebako::fs::c_api::set_errno;

namespace {

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
  return tebako_fs_init_from_file_at(archive_path, 0, 0, mount_point);
}

extern "C" int tebako_fs_init_from_file_at(const char* archive_path,
                                           uint64_t offset,
                                           uint64_t length,
                                           const char* mount_point)
{
  if (archive_path == nullptr || mount_point == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  try {
    return FsContext::instance().init_from_file_at(archive_path, offset, length, mount_point);
  }
  catch (...) {
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

  try {
    return FsContext::instance().init_from_memory(data, size, mount_point);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" void tebako_fs_unmount(void)
{
  FsContext::instance().unmount();
}

extern "C" int tebako_is_initialized(void)
{
  return FsContext::instance().is_mounted() ? 1 : 0;
}

// ===================================================================
// Multi-Mount Management
// ===================================================================

extern "C" int tebako_fs_mount_from_file(const char* archive_path, const char* mount_point, tebako_mount_t* out_handle)
{
  return tebako_fs_mount_from_file_at(archive_path, 0, 0, mount_point, out_handle);
}

extern "C" int tebako_fs_mount_from_file_at(const char* archive_path,
                                            uint64_t offset,
                                            uint64_t length,
                                            const char* mount_point,
                                            tebako_mount_t* out_handle)
{
  if (archive_path == nullptr || mount_point == nullptr || out_handle == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  try {
    return FsContext::instance().mount_from_file_at(archive_path, offset, length, mount_point, out_handle);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" int tebako_fs_mount_from_memory(const void* data,
                                           size_t size,
                                           const char* mount_point,
                                           tebako_mount_t* out_handle)
{
  if (data == nullptr || size == 0 || mount_point == nullptr || out_handle == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  try {
    return FsContext::instance().mount_from_memory(data, size, mount_point, out_handle);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" int tebako_fs_unmount_handle(tebako_mount_t handle)
{
  try {
    return FsContext::instance().unmount_handle(handle);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

// ===================================================================
// File Operations
// ===================================================================

extern "C" int tebako_fs_open(const char* path, int flags)
{
  if (path == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  try {
    return FsContext::instance().open(path, flags);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" ssize_t tebako_fs_read(int fd, void* buf, size_t count)
{
  if (buf == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  try {
    return FsContext::instance().read(fd, buf, count);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" ssize_t tebako_fs_pread(int fd, void* buf, size_t nbyte, off_t offset)
{
  if (buf == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  try {
    return FsContext::instance().pread(fd, buf, nbyte, offset);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" off_t tebako_fs_lseek(int fd, off_t offset, int whence)
{
  try {
    return FsContext::instance().lseek(fd, offset, whence);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" int tebako_fs_close(int fd)
{
  try {
    return FsContext::instance().close(fd);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

// ===================================================================
// Directory Operations
// ===================================================================

extern "C" tebako_dir_t tebako_fs_opendir(const char* path)
{
  if (path == nullptr) {
    set_errno(EINVAL);
    return nullptr;
  }

  try {
    return FsContext::instance().opendir(path);
  }
  catch (...) {
    handle_exception();
    return nullptr;
  }
}

extern "C" struct tebako_c_dirent* tebako_fs_readdir(tebako_dir_t dir)
{
  try {
    return FsContext::instance().readdir(dir);
  }
  catch (...) {
    handle_exception();
    return nullptr;
  }
}

extern "C" int tebako_fs_closedir(tebako_dir_t dir)
{
  try {
    return FsContext::instance().closedir(dir);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" int tebako_fs_dir_is_embedded(tebako_dir_t dir)
{
  try {
    return FsContext::instance().dir_is_embedded(dir);
  }
  catch (...) {
    return 0;
  }
}

extern "C" void tebako_fs_rewinddir(tebako_dir_t dir)
{
  try {
    FsContext::instance().rewinddir(dir);
  }
  catch (...) {
    handle_exception();
  }
}

extern "C" long tebako_fs_telldir(tebako_dir_t dir)
{
  try {
    return FsContext::instance().telldir(dir);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" void tebako_fs_seekdir(tebako_dir_t dir, long pos)
{
  try {
    FsContext::instance().seekdir(dir, pos);
  }
  catch (...) {
    handle_exception();
  }
}

// ===================================================================
// Metadata Operations
// ===================================================================

extern "C" int tebako_fs_stat(const char* path, struct stat* st)
{
  if (path == nullptr || st == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  try {
    return FsContext::instance().file_stat(path, st);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

extern "C" int tebako_fs_fstat(int fd, struct stat* st)
{
  if (st == nullptr) {
    set_errno(EINVAL);
    return -1;
  }

  try {
    return FsContext::instance().fd_stat(fd, st);
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

  return FsContext::instance().path_is_embedded(path);
}

extern "C" int tebako_fd_is_embedded(int fd)
{
  return FsContext::instance().fd_is_embedded(fd);
}

// ===================================================================
// ABI Version
// ===================================================================

extern "C" int tebako_fs_abi_version(void)
{
  return TEBAKO_FS_ABI_VERSION;
}

// ===================================================================
// Error Handling
// ===================================================================

extern "C" int tebako_get_errno(void)
{
  return tebako::fs::c_api::get_errno();
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

  try {
    return FsContext::instance().extract_all(dest_path);
  }
  catch (...) {
    handle_exception();
    return -1;
  }
}

// ===================================================================
// Dynamic Loading Support
// ===================================================================

extern "C" char* tebako_fs_dlmap2file(const char* path)
{
  if (path == nullptr) {
    set_errno(EINVAL);
    return nullptr;
  }

  try {
    return FsContext::instance().dlmap2file(path);
  }
  catch (...) {
    handle_exception();
    return nullptr;
  }
}

// ===================================================================
// Utility Functions
// ===================================================================

extern "C" const char* tebako_get_mount_point(void)
{
  return FsContext::instance().mount_point_c_str();
}

extern "C" const char* tebako_get_archive_path(void)
{
  // Store in static to ensure lifetime; archive_path() returns
  // std::string by value
  static std::string cached_path;
  cached_path = FsContext::instance().archive_path();
  return cached_path.empty() ? nullptr : cached_path.c_str();
}

extern "C" const char* tebako_get_backend_name(void)
{
  // Store in static to ensure lifetime; backend_name() returns
  // std::string by value
  static std::string cached_name;
  cached_name = FsContext::instance().backend_name();
  return cached_name.empty() ? nullptr : cached_name.c_str();
}
