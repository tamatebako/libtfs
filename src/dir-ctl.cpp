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

using namespace tebako;

char* tebako_getcwd(char* buf, size_t size)
{
  tebako_path_t _cwd;
  const char* cwd = tebako_get_cwd(_cwd, true);
  size_t len = strlen(cwd);
  if (len) {
    if (!buf) {
      if (!size) {
        buf = strdup(cwd);
        if (!buf) {
          TEBAKO_SET_LAST_ERROR(ENOMEM);
        }
      }
      else {
        if (len > size - 1) {
          TEBAKO_SET_LAST_ERROR(ERANGE);
          buf = NULL;
        }
        else {
          buf = (char*)malloc(size);
          if (!buf) {
            TEBAKO_SET_LAST_ERROR(ENOMEM);
          }
          else {
            strcpy(buf, cwd);
          }
        }
      }
    }
    else {
      if (!size) {
        TEBAKO_SET_LAST_ERROR(EINVAL);
        buf = NULL;
      }
      else if (len > size - 1) {
        TEBAKO_SET_LAST_ERROR(ERANGE);
        buf = NULL;
      }
      else
        strcpy(buf, cwd);
    }
    return buf;
  }
  else
    return TO_RB_W32_U(getcwd)(buf, size);
}

int tebako_chdir(const char* path)
{
  int ret = DWARFS_IO_ERROR;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);

    if (p_path) {
      struct STAT_TYPE st;
      ret = tebako_stat(p_path, &st);
      if (ret == 0) {
        if (S_ISDIR(st.st_mode)) {
          ret = tebako_set_cwd(p_path) ? 0 : -1;
          if (ret != 0) {
            TEBAKO_SET_LAST_ERROR(ENOMEM);
          }
        }
        else {
          ret = -1;
          TEBAKO_SET_LAST_ERROR(ENOTDIR);
        }
      }
    }
    else {
      ret = TO_RB_W32_U(chdir)(path);
      if (ret == 0) {
        ret = tebako_set_cwd(NULL) ? 0 : -1;
        if (ret != 0) {
          TEBAKO_SET_LAST_ERROR(ENOMEM);
        }
      }
    }
  }

  return ret;
}

#if defined(TEBAKO_HAS_POSIX_MKDIR) || defined(RB_W32)
int tebako_mkdir(const char* path, mode_t mode)
{
#else
int tebako_mkdir(const char* path)
{
#endif
  int ret = DWARFS_IO_ERROR;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
      std::string lnk;
      struct stat st;
      if (dwarfs_stat(p_path, &st, lnk, true) == DWARFS_S_LINK_OUTSIDE) {
#if defined(TEBAKO_HAS_POSIX_MKDIR) || defined(RB_W32)
        ret = TO_RB_W32_U(mkdir)(lnk.c_str(), mode);
#else
        ret = ::mkdir(lnk.c_str());
#endif
      }
      else {
        TEBAKO_SET_LAST_ERROR(EROFS);
      }
    }
    else {
#if defined(TEBAKO_HAS_POSIX_MKDIR) || defined(RB_W32)
      ret = TO_RB_W32_U(mkdir)(path, mode);
#else
      ret = ::mkdir(path);
#endif
    }
  }
  return ret;
}

int tebako_rmdir(const char* path)
{
  int ret = DWARFS_IO_ERROR;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
      std::string lnk;
      struct stat st;
      if (dwarfs_stat(p_path, &st, lnk, true) == DWARFS_S_LINK_OUTSIDE) {
        ret = TO_RB_W32_U(rmdir)(lnk.c_str());
      }
      else {
        TEBAKO_SET_LAST_ERROR(EROFS);
      }
    }
    else {
      ret = TO_RB_W32_U(rmdir)(path);
    }
  }
  return ret;
}

int tebako_unlink(const char* path)
{
  int ret = DWARFS_IO_ERROR;
  if (path == NULL) {
    TEBAKO_SET_LAST_ERROR(ENOENT);
  }
  else {
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
      std::string lnk;
      struct stat st;
      if (dwarfs_stat(p_path, &st, lnk, true) == DWARFS_S_LINK_OUTSIDE) {
        ret = TO_RB_W32_U(unlink)(lnk.c_str());
      }
      else {
        TEBAKO_SET_LAST_ERROR(EROFS);
      }
    }
    else {
      ret = TO_RB_W32_U(unlink)(path);
    }
  }
  return ret;
}
