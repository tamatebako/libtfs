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
#include <tebako/fs/io.h>
#include <tebako-io-inner.h>
#include <tebako-io-root.h>
#include <tebako/fs/internal/fd_table.h>
#include <tebako/fs/memfs.h>

using namespace std;

namespace tebako {

sync_tebako_fdtable& sync_tebako_fdtable::get_tebako_fdtable(void)
{
  static sync_tebako_fdtable fd_table{};
  return fd_table;
}

int sync_tebako_fdtable::open(const char* path, int flags, std::string& lnk) noexcept
{
  int ret = DWARFS_IO_ERROR;
  try {
    auto fd = make_shared<tebako_fd>();
    switch (dwarfs_stat(path, &fd->st, lnk, (flags & O_NOFOLLOW) == 0)) {
      case DWARFS_IO_CONTINUE:
        // [EROFS] The named file resides on a read - only file system and either
        // O_WRONLY, O_RDWR, O_CREAT(if the file does not exist), or O_TRUNC is set
        // in the oflag argument.
        if (flags & (O_RDWR | O_WRONLY | O_TRUNC | O_CREAT)) {
          TEBAKO_SET_LAST_ERROR(EROFS);
        }
        else if (!S_ISDIR(fd->st.st_mode) && (flags & O_DIRECTORY)) {
          // [ENOTDIR] ... or O_DIRECTORY was specified and the path argument
          // resolves to a non - directory file.
          TEBAKO_SET_LAST_ERROR(ENOTDIR);
        }
        else if (S_ISLNK(fd->st.st_mode) && (flags & O_NOFOLLOW)) {
          // [O_NOFOLLOW] If the trailing component (i.e., basename) of
          // pathname is a symbolic link, then the open fails, with the
          // error ELOOP.
          TEBAKO_SET_LAST_ERROR(ELOOP);
        }
        else {
          fd->handle = new int;
          if (fd->handle == NULL) {
            TEBAKO_SET_LAST_ERROR(ENOMEM);
          }
          else {
            // get a dummy fd from the system
            ret = ::dup(0);
            if (ret == DWARFS_IO_ERROR) {
              // [EMFILE]  All file descriptors available to the process are
              // currently open.
              TEBAKO_SET_LAST_ERROR(EMFILE);
            }
            else {
              // construct a handle (mainly) for win32
              *fd->handle = ret;
              (*s_tebako_fdtable.wlock())[ret] = std::move(fd);
            }
          }
        }
        break;
      case DWARFS_S_LINK_OUTSIDE:
        ret = DWARFS_S_LINK_OUTSIDE;
        break;
      default:
        /* DWARFS_IO_ERROR   */
        /* DWARFS_INVALID_FD */
        //	[ENOENT] O_CREAT is not set and a component of path does not
        // name an existing file, or O_CREAT is set and a component of the
        // path prefix of path does not name an existing file, or path points
        // to an
        // empty string. 	[EROFS] The named file resides on a read - only
        // file system and either O_WRONLY, O_RDWR, O_CREAT(if the file does
        // not exist), or O_TRUNC is set in the oflag argument.
        TEBAKO_SET_LAST_ERROR((flags & (O_RDWR | O_WRONLY | O_TRUNC | O_CREAT)) ? EROFS : ENOENT);
        break;
    }
  }
  catch (bad_alloc&) {
    if (ret > 0) {
      ::close(ret);
      ret = DWARFS_IO_ERROR;
    }
    TEBAKO_SET_LAST_ERROR(ENOMEM);
  }

  return ret;
}

int sync_tebako_fdtable::openat(int vfd, const char* path, int flags, std::string& lnk) noexcept
{
  struct stat stfd;
  int ret = this->fstat(vfd, &stfd);  // !! this, and not ::
  if (ret == DWARFS_IO_CONTINUE) {
    ret = DWARFS_IO_ERROR;
    if (!S_ISDIR(stfd.st_mode)) {
      // [ENOTDIR] ... or O_DIRECTORY was specified and the path argument
      // resolves to a non - directory file.
      TEBAKO_SET_LAST_ERROR(ENOTDIR);
    }
    else {
      //  ....
      //  If the access mode of the open file description associated with the
      //  file descriptor is not O_SEARCH, the function shall check whether
      //  directory searches are allowed If the access mode is O_SEARCH, the
      //  function shall not perform the check.
      //	....
      //	However, Linux does not support O_SEARCH (
      //  So, We will assume that it is not set
      if (dwarfs_inode_access(stfd.st_ino, X_OK, getuid(), getgid()) == DWARFS_IO_CONTINUE) {
        try {
          auto fd = make_shared<tebako_fd>();
          switch (dwarfs_inode_relative_stat(stfd.st_ino, path, &fd->st, lnk, (flags & O_NOFOLLOW) == 0)) {
            case DWARFS_IO_CONTINUE:
              // [EROFS] The named file resides on a read - only file system and either
              // O_WRONLY, O_RDWR, O_CREAT(if the file does not exist), or O_TRUNC is set
              // in the oflag argument.
              if (flags & (O_RDWR | O_WRONLY | O_TRUNC | O_CREAT)) {
                TEBAKO_SET_LAST_ERROR(EROFS);
              }
              else if (!S_ISDIR(fd->st.st_mode) && (flags & O_DIRECTORY)) {
                // [ENOTDIR] ... or O_DIRECTORY was specified and the path
                // argument resolves to a non - directory file.
                TEBAKO_SET_LAST_ERROR(ENOTDIR);
              }
              else if (S_ISLNK(fd->st.st_mode) && (flags & O_NOFOLLOW)) {
                //    [O_NOFOLLOW] If the trailing component (i.e., basename) of
                //    pathname is a symbolic link, then the open fails, with
                //    the error ELOOP.
                TEBAKO_SET_LAST_ERROR(ELOOP);
              }
              else {
                fd->handle = new int;
                if (fd->handle == NULL) {
                  TEBAKO_SET_LAST_ERROR(ENOMEM);
                }
                else {
                  // get a dummy fd from the system
                  ret = ::dup(0);
                  if (ret == DWARFS_IO_ERROR) {
                    // [EMFILE]  All file descriptors available to the process
                    // are currently open.
                    TEBAKO_SET_LAST_ERROR(EMFILE);
                  }
                  else {
                    // construct a handle (mainly) for win32
                    *fd->handle = ret;
                    (*s_tebako_fdtable.wlock())[ret] = std::move(fd);
                  }
                }
              }
              break;
            case DWARFS_S_LINK_OUTSIDE:
              ret = DWARFS_S_LINK_OUTSIDE;
              break;
            default:
              // [ENOENT] O_CREAT is not set and a component of path does
              // not name an existing file, or O_CREAT is set and a component of
              // the path prefix of path does not name an existing file, or path
              // points to an empty string.
              // [EROFS] The named file resides
              // on a read - only file system and either O_WRONLY, O_RDWR,
              // O_CREAT(if the file does not exist), or O_TRUNC is set in the
              // oflag argument.
              TEBAKO_SET_LAST_ERROR((flags & (O_RDWR | O_WRONLY | O_TRUNC | O_CREAT)) ? EROFS : ENOENT);
              break;
          }
        }
        catch (bad_alloc&) {
          if (ret > 0) {
            ::close(ret);
            ret = DWARFS_IO_ERROR;
          }
          TEBAKO_SET_LAST_ERROR(ENOMEM);
        }
      }
      else {
        TEBAKO_SET_LAST_ERROR(EACCES);
      }
    }
  }
  return ret;
}

int sync_tebako_fdtable::close(int vfd) noexcept
{
  return ((*s_tebako_fdtable.wlock()).erase(vfd) > 0) ? DWARFS_IO_CONTINUE : DWARFS_INVALID_FD;
}

void sync_tebako_fdtable::close_all(void) noexcept
{
  s_tebako_fdtable.wlock()->clear();
}

bool sync_tebako_fdtable::is_valid_file_descriptor(int vfd) noexcept
{
  int ret = false;
  auto p_fdtable = s_tebako_fdtable.rlock();
  auto p_fd = p_fdtable->find(vfd);
  if (p_fd != p_fdtable->end()) {
    ret = true;
  }
  return ret;
}

int sync_tebako_fdtable::fstat(int vfd, struct stat* st) noexcept
{
  int ret = DWARFS_INVALID_FD;
  auto p_fdtable = s_tebako_fdtable.rlock();
  auto p_fd = p_fdtable->find(vfd);
  if (p_fd != p_fdtable->end()) {
    memcpy(st, &p_fd->second->st, sizeof(struct stat));
    ret = DWARFS_IO_CONTINUE;
  }
  return ret;
}

ssize_t sync_tebako_fdtable::read(int vfd, void* buf, size_t nbyte) noexcept
{
  int ret = DWARFS_INVALID_FD;
  auto p_fdtable = s_tebako_fdtable.rlock();
  auto p_fd = p_fdtable->find(vfd);
  if (p_fd != p_fdtable->end()) {
    ret = dwarfs_inode_read(p_fd->second->st.st_ino, buf, nbyte, p_fd->second->pos);
    if (ret > 0) {
      p_fd->second->pos += ret;
    }
  }
  return ret;
}

ssize_t sync_tebako_fdtable::pread(int vfd, void* buf, size_t nbyte, off_t offset) noexcept
{
  auto p_fdtable = s_tebako_fdtable.rlock();
  auto p_fd = p_fdtable->find(vfd);
  return (p_fd != p_fdtable->end()) ? dwarfs_inode_read(p_fd->second->st.st_ino, buf, nbyte, offset)
                                    : DWARFS_INVALID_FD;
}

int sync_tebako_fdtable::readdir(int vfd,
                                 tebako::tebako_dirent* cache,
                                 off_t cache_start,
                                 size_t buffer_size,
                                 size_t& cache_size,
                                 size_t& dir_size) noexcept
{
  auto p_fdtable = s_tebako_fdtable.rlock();
  auto p_fd = p_fdtable->find(vfd);
  return (p_fd != p_fdtable->end())
             ? dwarfs_inode_readdir(p_fd->second->st.st_ino, cache, cache_start, buffer_size, cache_size, dir_size)
             : DWARFS_INVALID_FD;
}

#ifdef TEBAKO_HAS_READV
ssize_t sync_tebako_fdtable::readv(int vfd, const struct ::iovec* iov, int iovcnt) noexcept
{
  // Some specific error conditions:
  // EOVERFLOW - the resulting file offset cannot be represented in an off_t.
  // EINVAL - the sum of the iov_len values overflows an ssize_t value.
  // EINVAL - the vector count, iovcnt, is less than zero or greater
  //          than the permitted maximum.

  ssize_t ret = DWARFS_INVALID_FD;
  if (iovcnt < 0) {
    TEBAKO_SET_LAST_ERROR(EINVAL);
    ret = DWARFS_IO_ERROR;
  }
  else {
    uint32_t ino;
    off_t pos;
    auto p_fdtable = s_tebako_fdtable.rlock();
    auto p_fd = p_fdtable->find(vfd);
    if (p_fd != p_fdtable->end()) {
      ret = 0;
      for (int i = 0; i < iovcnt; ++i) {
        ssize_t ssize = dwarfs_inode_read(p_fd->second->st.st_ino, iov[i].iov_base, iov[i].iov_len, p_fd->second->pos);
        if (ssize > 0) {
          if (p_fd->second->pos > std::numeric_limits<off_t>::max() - ssize) {
            TEBAKO_SET_LAST_ERROR(EOVERFLOW);
            ret = DWARFS_IO_ERROR;
            break;
          }
          if (ret > std::numeric_limits<ssize_t>::max() - ssize) {
            TEBAKO_SET_LAST_ERROR(EINVAL);
            ret = DWARFS_IO_ERROR;
            break;
          }
          p_fd->second->pos += ssize;
          ret += ssize;
        }
        else {
          if (ssize < 0) {
            ret = DWARFS_IO_ERROR;
          }
          break;
        }
      }
    }
  }
  return ret;
}
#endif

off_t sync_tebako_fdtable::lseek(int vfd, off_t offset, int whence) noexcept
{
  ssize_t ret = DWARFS_INVALID_FD;
  auto p_fdtable = s_tebako_fdtable.rlock();
  auto p_fd = p_fdtable->find(vfd);
  if (p_fd != p_fdtable->end()) {
    switch (whence) {
      case SEEK_SET:
        if (offset < 0) {
          // [EINVAL] The resulting file offset would be negative for a regular
          // file, block special file, or directory.
          TEBAKO_SET_LAST_ERROR(EINVAL);
          ret = DWARFS_IO_ERROR;
        }
        else {
          ret = p_fd->second->pos = offset;
        }
        break;
      case SEEK_CUR:
        if (offset < 0 && p_fd->second->pos < -offset) {
          // [EINVAL] The resulting file offset would be negative for a regular
          // file, block special file, or directory.
          TEBAKO_SET_LAST_ERROR(EINVAL);
          ret = DWARFS_IO_ERROR;
        }
        else {
          if (offset > 0 && p_fd->second->pos > std::numeric_limits<off_t>::max() - offset) {
            // [EOVERFLOW] The resulting file offset would be a value which
            // cannot be represented correctly in an object of type off_t.
            TEBAKO_SET_LAST_ERROR(EOVERFLOW);
            ret = DWARFS_IO_ERROR;
          }
          else {
            ret = p_fd->second->pos = p_fd->second->pos + offset;
          }
        }
        break;
      case SEEK_END:
        if (offset < 0 && p_fd->second->st.st_size < -offset) {
          // [EINVAL] The resulting file offset would be negative for a regular
          // file, block special file, or directory.
          TEBAKO_SET_LAST_ERROR(EINVAL);
          ret = DWARFS_IO_ERROR;
        }
        else {
          if (offset > 0 && p_fd->second->st.st_size > std::numeric_limits<off_t>::max() - offset) {
            // [EOVERFLOW] The resulting file offset would be a value which
            // cannot be represented correctly in an object of type off_t.
            TEBAKO_SET_LAST_ERROR(EOVERFLOW);
            ret = DWARFS_IO_ERROR;
          }
          else {
            ret = p_fd->second->pos = p_fd->second->st.st_size + offset;
          }
        }
        break;
      default:
        // [EINVAL] The whence argument is not a proper value, or the resulting
        TEBAKO_SET_LAST_ERROR(EINVAL);
        ret = DWARFS_IO_ERROR;
        break;
    }
  }
  return ret;
}

int sync_tebako_fdtable::fstatat(int vfd, const char* path, struct stat* st, std::string& lnk, bool follow) noexcept
{
  struct stat stfd;
  int ret = fstat(vfd, &stfd);
  if (ret == DWARFS_IO_CONTINUE) {
    ret = DWARFS_IO_ERROR;
    if (!S_ISDIR(stfd.st_mode)) {
      TEBAKO_SET_LAST_ERROR(ENOTDIR);
    }
    else {
      ret = dwarfs_inode_access(stfd.st_ino, X_OK, getuid(), getgid());
      if (ret == DWARFS_IO_CONTINUE) {
        ret = dwarfs_inode_relative_stat(stfd.st_ino, path, st, lnk, follow);
      }
    }
  }
  return ret;
}

int sync_tebako_fdtable::flock(int fd, int operation) noexcept
{
  int ret = DWARFS_INVALID_FD;
  auto p_fdtable = s_tebako_fdtable.rlock();
  auto p_fd = p_fdtable->find(fd);
  if (p_fd != p_fdtable->end()) {
    ret = DWARFS_IO_CONTINUE;
    //  Tebako files are accessible by the package process only, so we do not
    //  need to check anything We store the lock state in the file descriptor
    //  structure for possible future implementation of fcntl
    p_fd->second->lock = operation & ~(LOCK_NB | LOCK_UN);
  }
  return ret;
}
}  // namespace tebako
