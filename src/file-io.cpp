/**
 *
 * Copyright (c) 2021-2025, [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project. (libdwarfs-wr)
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

#include <tebako-pch.h>
#include <tebako-pch-pp.h>
#include <tebako/fs/common.h>
#include <tebako/fs/dirent.h>
#include <tebako/fs/io.h>
#include <tebako-io-inner.h>
#include <tebako/fs/ruby/io_rb_w32_inner.h>
#include <tebako-io-root.h>
#include <tebako/fs/internal/fd_table.h>

using namespace tebako;

int tebako_open(int nargs, const char* path, int flags, ...)
{
  int ret = -1;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    tebako_path_t t_path;
    std::string r_path = path;
    const char* p_path = to_tebako_path(t_path, path);

    if (p_path) {
      // This call will change r_path value if it is a link inside memfs
      // pointing outside of memfs
      ret = sync_tebako_fdtable::get_tebako_fdtable().open(p_path, flags, r_path);
    }
    if (!p_path || ret == DWARFS_S_LINK_OUTSIDE) {
      if (nargs == 2) {
        ret = TO_RB_W32_U(open)(r_path.c_str(), flags);
      }
      else {
        va_list args;
        mode_t mode;
        va_start(args, flags);
        // https://stackoverflow.com/questions/28054194/char-type-in-va-arg
        // second argument to 'va_arg' is of promotable type 'mode_t' (aka
        // 'unsigned short'); this va_arg has undefined behavior because
        // arguments will be promoted to 'int'
        mode = (mode_t)va_arg(args, int);
        va_end(args);
        ret = TO_RB_W32_U(open)(r_path.c_str(), flags, mode);
      }
    }
  }
  return ret;
}

#ifdef TEBAKO_HAS_OPENAT
int tebako_openat(int nargs, int vfd, const char* path, int flags, ...)
{
  int ret = -1;
  va_list args;
  mode_t mode;
  //	The openat() function shall be equivalent to the open() function except
  // in the case where path specifies a relative path.
  //  In this case the file to be opened is determined relative to the directory
  //  associated with the file descriptor fd instead of the current working
  //  directory.
  //  ...
  //	If openat() is passed the special value AT_FDCWD in the fd parameter,
  // the current working directory shall be used and the behavior shall be
  // identical to a call to open().
  try {
    std::string r_path;
    std::filesystem::path std_path(path);
    if (std_path.is_relative() && vfd != AT_FDCWD) {
      ret = sync_tebako_fdtable::get_tebako_fdtable().openat(vfd, path, flags, r_path);
      switch (ret) {
        case DWARFS_INVALID_FD:
          if (nargs == 3) {
            ret = ::openat(vfd, path, flags);
          }
          else {
            va_start(args, flags);
            mode = (mode_t)va_arg(args, int);
            va_end(args);
            ret = ::openat(vfd, path, flags, mode);
          }
          break;
        case DWARFS_S_LINK_OUTSIDE:
          ret = TO_RB_W32_U(openat)(vfd, r_path.c_str(), flags);
          break;
        default:
          break;
      }
    }
    else {
      if (nargs == 3) {
        ret = tebako_open(2, path, flags);
      }
      else {
        va_start(args, flags);
        mode = (mode_t)va_arg(args, int);
        va_end(args);
        ret = tebako_open(3, path, flags, mode);
      }
    }
  }
  catch (...) {
    ret = -1;
    TEBAKO_SET_LAST_ERROR(ENOMEM);
  }
  return ret;
}
#endif

off_t tebako_lseek(int vfd, off_t offset, int whence)
{
  off_t ret = sync_tebako_fdtable::get_tebako_fdtable().lseek(vfd, offset, whence);
  if (ret == DWARFS_INVALID_FD) {
    ret = is_valid_system_file_descriptor(vfd) ? TO_RB_W32(lseek)(vfd, offset, whence) : DWARFS_IO_ERROR;
  }
  return ret;
}

ssize_t tebako_read(int vfd, void* buf, size_t nbyte)
{
  ssize_t ret = sync_tebako_fdtable::get_tebako_fdtable().read(vfd, buf, nbyte);
  if (ret == DWARFS_INVALID_FD) {
    ret = is_valid_system_file_descriptor(vfd) ? TO_RB_W32(read)(vfd, buf, nbyte) : DWARFS_IO_ERROR;
  }
  return ret;
}

#ifdef TEBAKO_HAS_READV
ssize_t tebako_readv(int vfd, const struct ::iovec* iov, int iovcnt)
{
  ssize_t ret = sync_tebako_fdtable::get_tebako_fdtable().readv(vfd, iov, iovcnt);
  if (ret == DWARFS_INVALID_FD) {
    ret = is_valid_system_file_descriptor(vfd) ? ::readv(vfd, iov, iovcnt) : DWARFS_IO_ERROR;
  }
  return ret;
}
#endif

#if defined(TEBAKO_HAS_PREAD) || defined(RB_W32)
ssize_t tebako_pread(int vfd, void* buf, size_t nbyte, off_t offset)
{
  ssize_t ret = sync_tebako_fdtable::get_tebako_fdtable().pread(vfd, buf, nbyte, offset);
  if (ret == DWARFS_INVALID_FD) {
    ret = is_valid_system_file_descriptor(vfd) ? TO_RB_W32(pread)(vfd, buf, nbyte, offset) : DWARFS_IO_ERROR;
  }
  return ret;
}
#endif

int tebako_close(int vfd)
{
  int ret = sync_tebako_fdtable::get_tebako_fdtable().close(vfd);
  if (ret == DWARFS_INVALID_FD) {
    ret = is_valid_system_file_descriptor(vfd) ? TO_RB_W32(close)(vfd) : DWARFS_IO_ERROR;
  }
  return ret;
}

int tebako_fstat(int vfd, struct STAT_TYPE* buf)
{
#if defined(RB_W32)
  struct stat _buf;
  int ret = sync_tebako_fdtable::get_tebako_fdtable().fstat(vfd, &_buf);
  buf << _buf;
#else
  int ret = sync_tebako_fdtable::get_tebako_fdtable().fstat(vfd, buf);
#endif
  if (ret == DWARFS_INVALID_FD) {
    ret = is_valid_system_file_descriptor(vfd) ? TO_RB_W32_I128(fstat)(vfd, buf) : DWARFS_IO_ERROR;
  }
  return ret;
}

#ifdef TEBAKO_HAS_FSTATAT
int tebako_fstatat(int vfd, const char* path, struct stat* st, int flag)
{
  int ret = -1;
  try {
    std::filesystem::path std_path(path);
    if (std_path.is_absolute() || vfd == AT_FDCWD) {
      ret = (flag & AT_SYMLINK_NOFOLLOW) ? tebako_lstat(path, st) : tebako_stat(path, st);
    }
    else {
      std::string r_path;
      ret = sync_tebako_fdtable::get_tebako_fdtable().fstatat(vfd, path, st, r_path, (flag & AT_SYMLINK_NOFOLLOW) == 0);
      switch (ret) {
        case DWARFS_INVALID_FD:
          ret = is_valid_system_file_descriptor(vfd) ? ::fstatat(vfd, path, st, flag) : DWARFS_IO_ERROR;
          break;
        case DWARFS_S_LINK_OUTSIDE:
          ret = tebako_stat(r_path.c_str(), st);
          break;
        default:
          break;
      }
    }
  }
  catch (...) {
    ret = -1;
    TEBAKO_SET_LAST_ERROR(ENOMEM);
  }
  return ret;
}
#endif

#ifdef TEBAKO_HAS_FGETATTRLIST
int tebako_fgetattrlist(int vfd, struct attrlist* attrList, void* attrBuf, size_t attrBufSize, unsigned long options)
{
  struct stat stfd;
  int ret = sync_tebako_fdtable::get_tebako_fdtable().fstat(vfd, &stfd);
  if (ret == DWARFS_INVALID_FD) {
    ret = is_valid_system_file_descriptor(vfd) ? ::fgetattrlist(vfd, attrList, attrBuf, attrBufSize, options)
                                               : DWARFS_IO_ERROR;
  }
  else {
    ret = -1;
    TEBAKO_SET_LAST_ERROR(ENOTSUP);
  }
  return ret;
}
#endif

int tebako_flock(int vfd, int operation)
{
  int ret = sync_tebako_fdtable::get_tebako_fdtable().flock(vfd, operation);
  if (ret == DWARFS_INVALID_FD) {
    ret = is_valid_system_file_descriptor(vfd) ? ::flock(vfd, operation) : DWARFS_IO_ERROR;
  }
  return ret;
}

int is_tebako_file_descriptor(int vfd)
{
  return sync_tebako_fdtable::get_tebako_fdtable().is_valid_file_descriptor(vfd) ? -1 : 0;
}
