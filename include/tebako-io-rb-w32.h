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

/* Standalone MinGW (no ruby build context): pull in the CRT types the
 * shims below rely on -- ino_t/_dev_t (sys/types.h), struct stat
 * (sys/stat.h), __time64_t (corecrt.h via sys/types.h). In the ruby build
 * context these come from ruby's win32 headers and RUBY_WIN32_H is set, so
 * this branch stays inert there. */
#if !defined(RUBY_WIN32_H)
#include <sys/types.h>
#include <sys/stat.h>
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

#if !defined(_TEBAKO_UID_GID_DEFINED)
#define _TEBAKO_UID_GID_DEFINED
/* MinGW has no uid_t/gid_t (they come from ruby's win32 headers in the ruby
 * build context); int matches both rb_uid_t/rb_gid_t and POSIX */
typedef int uid_t;
typedef int gid_t;
#endif

#ifdef __cplusplus
/* Template, deliberately: sys/stat.h includes <io.h>, which this header
 * shadows (tebako/fs/io.h), so on MinGW we are routinely evaluated *before*
 * struct stat is defined at sys/stat.h:137. A template declaration needs no
 * complete type here; it instantiates only at call sites (ruby's stat, the
 * CRT's struct stat, or stati128 itself). */
template <typename StatT>
inline stati128* operator<<(stati128* o, StatT i)
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

template <typename StatT>
inline StatT* operator<<(StatT* o, stati128 i)
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

/* -----------------------------------------------------------------------
 * Standalone MinGW POSIX compatibility shims
 *
 * The legacy quartet (file-ctl/file-io, dir-ctl/dir-io), dl-ctl and
 * fs/internal/fd_table.h call a handful of POSIX functions via raw global
 * scope names (::open, ::close, ::write, ::readlink, getuid, ...). In the
 * ruby build context these resolve through ruby's win32 layer; standalone
 * MinGW has neither ruby nor these functions. Moreover the CRT's own
 * <io.h> is unreachable here: include/tebako/fs is on the include path, so
 * every `#include <io.h>` (including the one inside mingw's <sys/stat.h>)
 * resolves to the tebako shadow header tebako/fs/io.h. Declare the UCRT
 * low-I/O primitives locally and map the POSIX names onto them; the shim
 * stays inert in the ruby build context (RUBY_WIN32_H).
 * ----------------------------------------------------------------------- */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UCRT low-I/O primitives (normally declared by the shadowed <io.h>) */
int _open(const char*, int, ...);
int _close(int);
int _read(int, void*, unsigned int);
int _write(int, const void*, unsigned int);
__int64 _lseeki64(int, __int64, int);

static inline int open(const char* path, int flags, ...)
{
  int mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);
  }
  return _open(path, flags, mode);
}

static inline int close(int fd)
{
  return _close(fd);
}

static inline ssize_t read(int fd, void* buf, size_t nbyte)
{
  return (ssize_t)_read(fd, buf, (unsigned int)nbyte);
}

static inline ssize_t write(int fd, const void* buf, size_t nbyte)
{
  return (ssize_t)_write(fd, buf, (unsigned int)nbyte);
}

/* Windows has no uid/gid; 0 matches ruby's win32 stubs (win32.h maps these
 * to 0 there as well) */
static inline uid_t getuid(void)
{
  return 0;
}

static inline uid_t geteuid(void)
{
  return 0;
}

static inline gid_t getgid(void)
{
  return 0;
}

static inline gid_t getegid(void)
{
  return 0;
}

/* tebako_readlink falls back to the host ::readlink only for memfs links
 * pointing outside of the memfs; host symlinks are rare on Windows, so a
 * graceful ENOSYS is acceptable (v1) */
static inline ssize_t readlink(const char* path, char* buf, size_t bufsize)
{
  (void)path;
  (void)buf;
  (void)bufsize;
  errno = ENOSYS;
  return -1;
}

/* Approximation: open() semantics for AT_FDCWD and absolute paths; there is
 * no dirfd table standalone, so a real dirfd gets ENOSYS (v1) */
#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif
static inline int openat(int dirfd, const char* path, int flags, ...)
{
  int mode = 0;
  int absolute = (path[0] == '/' || path[0] == '\\' ||
                  (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':'));
  if (dirfd == AT_FDCWD || absolute) {
    if (flags & O_CREAT) {
      va_list args;
      va_start(args, flags);
      mode = va_arg(args, int);
      va_end(args);
    }
    return _open(path, flags, mode);
  }
  errno = ENOSYS;
  return -1;
}

/* Approximation: seek+read+restore (not atomic; the CRT has no positional
 * read and the fd is not shared across threads in the legacy use) */
static inline ssize_t pread(int fd, void* buf, size_t nbyte, off_t offset)
{
  __int64 prev = _lseeki64(fd, 0, SEEK_CUR);
  int ret;
  if (prev == -1 || _lseeki64(fd, (__int64)offset, SEEK_SET) == -1) {
    return -1;
  }
  ret = _read(fd, buf, (unsigned int)nbyte);
  _lseeki64(fd, prev, SEEK_SET);
  return (ssize_t)ret;
}

#if !defined(_SYS_UIO_H) && !defined(_SYS_UIO_H_)
struct iovec {
  void* iov_base;
  size_t iov_len;
};
#endif

static inline ssize_t readv(int fd, const struct iovec* iov, int iovcnt)
{
  ssize_t total = 0;
  int i;
  for (i = 0; i < iovcnt; i++) {
    int n = _read(fd, iov[i].iov_base, (unsigned int)iov[i].iov_len);
    if (n <= 0) {
      return (total > 0) ? total : (ssize_t)n;
    }
    total += n;
    if ((size_t)n < iov[i].iov_len) {
      break;
    }
  }
  return total;
}

#ifdef __cplusplus
}
#endif

#endif  // RUBY_WIN32_H

#endif  // RB_W32, TEBAKO_IO_RB_W32_H
