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

/*
 *  Tebako filesystem interface functions
 *  This file shall be included into sources we want to "hack"
 */

#pragma once

#if defined(RB_W32)
#define STAT_TYPE stati128
#include "tebako-io-rb-w32.h"
#else
#define STAT_TYPE stat
#endif

#ifdef __cplusplus
extern "C" {
#endif  // !__cplusplus
int mount_root_memfs(const void* data,
                     const unsigned int size,
                     const char* debuglevel,
                     const char* cachesize,
                     const char* workers,
                     const char* mlock,
                     const char* decompress_ratio,
                     const char* image_offset);

int mount_memfs_at_root(const void* data, const unsigned int size, const char* image_offset, const char* path);

int mount_memfs(const void* data,
                const unsigned int size,
                const char* image_offset,
                unsigned int parent_inode,
                const char* path);

void unmount_root_memfs(void);

char* tebako_getcwd(char* buf, size_t size);
int tebako_chdir(const char* path);

/* Another option -- to be cleaned if 'defined(_SYS_STAT_H)' works
#if defined(__mode_t_defined) || defined(_MODE_T) || defined(__NEED_mode_t)
    __mode_t_defined    -- Ubuntu/GNU
    _MODE_T             -- Darwin
    __NEED_mode_t       -- Alpine/musl
    int   tebako_mkdir(const char* path, mode_t mode);
#endif
*/

int tebako_access(const char* path, int amode);
#ifdef TEBAKO_HAS_EACCESS
int tebako_eaccess(const char* path, int amode);
#endif
int tebako_open(int nargs, const char* path, int flags, ...);
off_t tebako_lseek(int vfd, off_t offset, int whence);

#ifdef TEBAKO_HAS_OPENAT
int tebako_openat(int nargs, int fd, const char* path, int flags, ...);
#endif
ssize_t tebako_read(int vfd, void* buf, size_t nbyte);

/* struct iovec is defined only if sys/uio.h has been included */
#if defined(_SYS_UIO_H) || defined(_SYS_UIO_H_)
#ifdef TEBAKO_HAS_READV
ssize_t tebako_readv(int vfd, const struct iovec* iov, int iovcnt);
#endif
#endif

#if (defined(TEBAKO_HAS_PREAD) && (defined(_UNISTD_H) || defined(_UNISTD_H_))) || defined(RB_W32)
ssize_t tebako_pread(int vfd, void* buf, size_t nbyte, off_t offset);
#endif

#if defined(_UNISTD_H) || defined(_UNISTD_H_)
#ifdef TEBAKO_HAS_GETATTRLIST
int tebako_getattrlist(const char* path,
                       struct attrlist* attrList,
                       void* attrBuf,
                       size_t attrBufSize,
                       unsigned long options);
#endif

#ifdef TEBAKO_HAS_FGETATTRLIST
int tebako_fgetattrlist(int fd, struct attrlist* attrList, void* attrBuf, size_t attrBufSize, unsigned long options);
#endif

#endif

/* struct stat is defined only if sys/stat.h has been included */
#if defined(_SYS_STAT_H) || defined(_SYS_STAT_H_) || defined(_INC_STAT)
#if defined(TEBAKO_HAS_POSIX_MKDIR) || defined(RB_W32)
int tebako_mkdir(const char* path, mode_t mode);
#else
int tebako_mkdir(const char* path);
#endif

int tebako_rmdir(const char* path);
int tebako_unlink(const char* path);

int tebako_stat(const char* path, struct STAT_TYPE* buf);
int tebako_fstat(int vfd, struct STAT_TYPE* buf);
#if defined(TEBAKO_HAS_LSTAT) || defined(RB_W32)
int tebako_lstat(const char* path, struct STAT_TYPE* buf);
#endif
#ifdef TEBAKO_HAS_FSTATAT
int tebako_fstatat(int fd, const char* path, struct stat* buf, int flag);
#endif
#endif

int tebako_close(int vfd);
ssize_t tebako_readlink(const char* path, char* buf, size_t bufsiz);

/* DIR and struct dirent is defined only if dirent.h has been included
    In Ruby eenvironment
        tebako_opendir
        tebako_closedir
        tebako_reeddir
        tebako_telldir
        tebako_seekdir
    are used to shadow rb_w32_xxx counterparts and not API methods
*/
#if defined(_DIRENT_H) || defined(_DIRENT_H_) || defined(RUBY_WIN32_DIR_H) || defined(RB_W32_DIR_DEFINED)

DIR* tebako_opendir(const char* dirname);

#ifdef TEBAKO_HAS_FDOPENDIR
DIR* tebako_fdopendir(int fd);
#endif

#ifdef RB_W32
struct direct* tebako_readdir(DIR* dirp, void* enc);
#else
struct dirent* tebako_readdir(DIR* dirp);
#endif  // RB_W32

long tebako_telldir(DIR* dirp);
void tebako_seekdir(DIR* dirp, long loc);
int tebako_closedir(DIR* dirp);

#ifdef TEBAKO_HAS_DIRFD
int tebako_dirfd(DIR* dirp);
#endif

#ifdef TEBAKO_HAS_SCANDIR
int tebako_scandir(const char* dir,
                   struct dirent*** namelist,
                   int (*sel)(const struct dirent*),
                   int (*compar)(const struct dirent**, const struct dirent**));
#endif

#endif  // defined(_DIRENT_H) ...

void* tebako_dlopen(const char* path, int flags);
char* tebako_dlerror(void);
char* tebako_dlmap2file(const char* path);

#if defined(TEBAKO_HAS_DIRFD) || defined(RB_W32)
int tebako_flock(int vfd, int operation);
#endif

int within_tebako_memfs(const char* path);
int is_tebako_file_descriptor(int fd);
int tebako_file_load_ok(const char* path);
int is_tebako_cwd(void);

#ifdef _WIN32
int is_tebako_path_w(const WCHAR* path);
#endif

#ifdef __cplusplus
}
#endif  // __cplusplus
