/**
 *
 * Copyright (c) 2021-2025 [Ribose Inc](https://www.ribose.com).
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

#pragma once

#include <version.h>

#ifdef _WIN32
#define TEBAKO_SET_LAST_ERROR(e)             \
  {                                          \
    errno = e;                               \
    if (e == ENOMEM) {                       \
      SetLastError(ERROR_NOT_ENOUGH_MEMORY); \
      _doserrno = ERROR_NOT_ENOUGH_MEMORY;   \
    }                                        \
    else if (e == ENOENT) {                  \
      SetLastError(ERROR_FILE_NOT_FOUND);    \
      _doserrno = ERROR_FILE_NOT_FOUND;      \
    }                                        \
    else if (e == EBADF) {                   \
      SetLastError(ERROR_INVALID_HANDLE);    \
      _doserrno = ERROR_INVALID_HANDLE;      \
    }                                        \
    else if (e == ENAMETOOLONG) {            \
      SetLastError(ERROR_BUFFER_OVERFLOW);   \
      _doserrno = ERROR_BUFFER_OVERFLOW;     \
    }                                        \
    else if (e == EINVAL) {                  \
      SetLastError(ERROR_BAD_LENGTH);        \
      _doserrno = ERROR_BAD_LENGTH;          \
    }                                        \
    else if (e == ERANGE) {                  \
      SetLastError(ERROR_BUFFER_OVERFLOW);   \
      _doserrno = ERROR_BUFFER_OVERFLOW;     \
    }                                        \
    else {                                   \
      SetLastError(ERROR_INVALID_FUNCTION);  \
      _doserrno = ERROR_INVALID_FUNCTION;    \
    }                                        \
  }
#else
#define TEBAKO_SET_LAST_ERROR(e) errno = (e)
#endif

#ifdef PATH_MAX
#define TEBAKO_PATH_LENGTH ((size_t)PATH_MAX)
#else
#define TEBAKO_PATH_LENGTH ((size_t)2048)
#endif

#ifdef _WIN32
#define TEBAKO_MOUNT_POINT "A:\\__tebako_memfs__"
#define TEBAKO_MOUNT_POINT_S "A:/__tebako_memfs__"
#define TEBAKO_MOUNT_POINT_W L"A:\\__tebako_memfs__"
#define TEBAKO_MOUNT_POINT_WS L"A:/__tebako_memfs__"
#define TEBAKO_MOUNT_POINT_LENGTH 19
#else
#define TEBAKO_MOUNT_POINT "/__tebako_memfs__"
#define TEBAKO_MOUNT_POINT_W L"/__tebako_memfs__"
#define TEBAKO_MOUNT_POINT_LENGTH 17
#endif

typedef char tebako_path_t[TEBAKO_PATH_LENGTH + 1];

const char* tebako_get_cwd(tebako_path_t cwd, bool win_separator = false);
bool is_tebako_path(const char* path);
bool is_valid_system_file_descriptor(int fd);
char* tebako_path_assign(tebako_path_t out, const std::string& in);
bool tebako_set_cwd(const char* path);
const char* to_tebako_path(tebako_path_t t_path, const char* path);

#ifdef RB_W32
#define TO_RB_W32(A) ::rb_w32_##A
#define TO_RB_W32_U(A) ::rb_w32_u##A
#define TO_RB_W32_I128(A) ::rb_w32_##A##i128
#define TO_RB_W32_U_I128(A) ::rb_w32_u##A##i128
#else
#define TO_RB_W32(A) ::A
#define TO_RB_W32_U(A) ::A
#define TO_RB_W32_I128(A) ::A
#define TO_RB_W32_U_I128(A) ::A
#endif
