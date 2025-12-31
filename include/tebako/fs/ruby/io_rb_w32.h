/**
 *
 * Copyright (c) 2022-2025 [Ribose Inc](https://www.ribose.com).
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

#if defined(RB_W32) && !defined(TEBAKO_IO_RB_W32_H)
#define TEBAKO_IO_RB_W32_H

#if !defined(_S_IFLNK)
#define _S_IFLNK 0xA000
#endif

#if !defined(S_IFLNK)
#define S_IFLNK _S_IFLNK
#endif

#if !defined(S_ISLNK)
#define S_ISLNK(mode) (((mode) & (_S_IFLNK)) == (_S_IFLNK) ? 1 : 0)
#endif

#if !defined(_S_ISTYPE)
#define _S_ISTYPE(mode, mask) (((mode) & (_S_IFMT)) == (mask))
#endif

#if !defined(S_ISREG)
#define S_ISREG(mode) _S_ISTYPE((mode), _S_IFREG)
#endif

#if !defined(S_ISDIR)
#define S_ISDIR(mode) _S_ISTYPE((mode), _S_IFDIR)
#endif

#ifndef LOCK_SH
#define LOCK_SH 1
#endif

#ifndef LOCK_EX
#define LOCK_EX 2
#endif

#ifndef LOCK_NB
#define LOCK_NB 4
#endif

#ifndef LOCK_UN
#define LOCK_UN 8
#endif

#if !defined(RUBY_WIN32_DIR_H) && !defined(RB_W32_DIR_DEFINED)
#define RB_W32_DIR_DEFINED

/* This is Ruby replacement for dirent */
#define DT_UNKNOWN 0
#define DT_DIR (S_IFDIR >> 12)
#define DT_REG (S_IFREG >> 12)
#define DT_LNK 10

struct direct {
  long d_namlen;
  ino_t d_ino;
  char* d_name;
  char* d_altname; /* short name */
  short d_altlen;
  uint8_t d_type;
};

typedef struct {
  WCHAR* start;
  WCHAR* curr;
  long size;
  long nfiles;
  long loc; /* [0, nfiles) */
  struct direct dirstr;
  char* bits; /* used for d_isdir and d_isrep */
} DIR;
#endif  // RUBY_WIN32_DIR_H, RB_W32_DIR_DEFINED

#if !defined(RUBY_WIN32_H)
struct stati128 {
  _dev_t st_dev;
  unsigned __int64 st_ino;
  __int64 st_inohigh;
  unsigned short st_mode;
  short st_nlink;
  short st_uid;
  short st_gid;
  _dev_t st_rdev;
  __int64 st_size;
  __time64_t st_atime;
  long st_atimensec;
  __time64_t st_mtime;
  long st_mtimensec;
  __time64_t st_ctime;
  long st_ctimensec;
};

#ifdef __cplusplus
inline stati128* operator<<(stati128* o, struct stat i)
{
  o->st_dev = i.st_dev;
  o->st_ino = i.st_ino;
  o->st_inohigh = 0;
  o->st_mode = i.st_mode;
  o->st_nlink = i.st_nlink;
  o->st_uid = i.st_uid;
  o->st_gid = i.st_gid;
  o->st_rdev = i.st_rdev;
  o->st_size = i.st_size;
  o->st_atime = i.st_atime;
  o->st_atimensec = 0;
  o->st_mtime = i.st_mtime;
  o->st_mtimensec = 0;
  o->st_ctime = i.st_ctime;
  o->st_ctimensec = 0;
  return o;
}

inline struct stat* operator<<(struct stat* o, stati128 i)
{
  o->st_dev = i.st_dev;
  o->st_ino = i.st_ino;
  o->st_mode = i.st_mode;
  o->st_nlink = i.st_nlink;
  o->st_uid = i.st_uid;
  o->st_gid = i.st_gid;
  o->st_rdev = i.st_rdev;
  o->st_size = i.st_size;
  o->st_atime = i.st_atime;
  o->st_mtime = i.st_mtime;
  o->st_ctime = i.st_ctime;
  return o;
}
#endif  // __cplusplus
#endif  // RUBY_WIN32_H

#endif  // RB_W32, TEBAKO_IO_RB_W32_H
