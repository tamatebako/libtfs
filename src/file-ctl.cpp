/**
 *
 *  Copyright (c) 2021-2025, [Ribose Inc](https://www.ribose.com).
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

using namespace tebako;

int tebako_access(const char* path, int amode)
{
  int ret = -1;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    std::string lnk;
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
      ret = dwarfs_access(p_path, amode, getuid(), getgid(), lnk);
      if (ret == DWARFS_S_LINK_OUTSIDE) {
        ret = TO_RB_W32(access)(lnk.c_str(), amode);
      }
    }
    else {
      ret = TO_RB_W32(access)(path, amode);
    }
  }
  return ret;
}

#if defined(TEBAKO_HAS_EACCESS)
int tebako_eaccess(const char* path, int amode)
{
  int ret = -1;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    std::string lnk;
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
      ret = dwarfs_access(p_path, amode, geteuid(), getegid(), lnk);
      if (ret == DWARFS_S_LINK_OUTSIDE) {
        ret = TO_RB_W32(eaccess)(lnk.c_str(), amode);
      }
    }
    else {
      ret = TO_RB_W32(eaccess)(path, amode);
    }
  }
  return ret;
}
#endif

#if defined(TEBAKO_HAS_LSTAT) || defined(RB_W32)
int tebako_lstat(const char* path, struct STAT_TYPE* buf)
{
  int ret = -1;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    std::string r_path;
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
#ifdef _WIN32
      struct stat _buf;
      ret = dwarfs_lstat(p_path, &_buf, r_path);
      buf << _buf;
#else
      ret = dwarfs_lstat(p_path, buf, r_path);
#endif
      if (ret == DWARFS_S_LINK_OUTSIDE) {
        ret = TO_RB_W32_I128(lstat)(r_path.c_str(), buf);
      }
    }
    else {
      ret = TO_RB_W32_I128(lstat)(path, buf);
    }
  }
  return ret;
}
#endif

ssize_t tebako_readlink(const char* path, char* buf, size_t bufsize)
{
  ssize_t ret = -1;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
      std::string link;
      std::string lnk;

      ret = dwarfs_readlink(p_path, link, lnk);
      if (ret >= 0) {
        strncpy(buf, link.c_str(), bufsize);
        ret = std::min(link.length(), bufsize);
      }
      else if (ret == DWARFS_S_LINK_OUTSIDE) {
        ret = ::readlink(lnk.c_str(), buf, bufsize);
      }
    }
    else {
      ret = ::readlink(path, buf, bufsize);
    }
  }
  return ret;
}

int tebako_stat(const char* path, struct STAT_TYPE* buf)
{
  int ret = -1;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    std::string lnk;
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
#ifdef RB_W32
      struct stat _buf;
      ret = dwarfs_stat(p_path, &_buf, lnk, true);
      buf << _buf;
#else
      ret = dwarfs_stat(p_path, buf, lnk, true);
#endif
      if (ret == DWARFS_S_LINK_OUTSIDE)
        ret = TO_RB_W32_I128(stat)(lnk.c_str(), buf);
    }
    else {
      ret = TO_RB_W32_I128(stat)(path, buf);
    }
  }
  return ret;
}

#ifdef TEBAKO_HAS_GETATTRLIST
int tebako_getattrlist(const char* path,
                       struct attrlist* attrList,
                       void* attrBuf,
                       size_t attrBufSize,
                       unsigned long options)
{
  int ret = DWARFS_IO_ERROR;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(EFAULT);
  }
  else {
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
      TEBAKO_SET_LAST_ERROR(ENOTSUP);
    }
    else {
      ret = ::getattrlist(path, attrList, attrBuf, attrBufSize, options);
    }
  }
  return ret;
}
#endif

int within_tebako_memfs(const char* path)
{
  return is_tebako_path(path) ? -1 : 0;
}

int tebako_file_load_ok(const char* path)
{
  int ret = 0;

  if (within_tebako_memfs(path)) {
    int fd = tebako_open(2, path, O_RDONLY);
    if (fd != -1) {
      struct STAT_TYPE st;
      ret = (tebako_fstat(fd, &st) == 0) ? -1 : 0;
      tebako_close(fd);
    }
  }
  return ret;
}
