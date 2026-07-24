/**
 *
 * Copyright (c) 2021-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project. (dwarfs-wr)
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

#if defined(_WIN32) && !defined(RUBY_WIN32_H) && !defined(_TEBAKO_UID_GID_DEFINED)
#define _TEBAKO_UID_GID_DEFINED
/* MinGW has no uid_t/gid_t (ruby's win32 headers provide them in the ruby
 * build context); int matches both rb_uid_t/rb_gid_t and POSIX. Windows-only:
 * on POSIX sys/types.h already has uid_t/gid_t and a typedef here conflicts. */
typedef int uid_t;
typedef int gid_t;
#endif

namespace tebako {
int mount_root_memfs(const void* data,
                     const unsigned int size,
                     const char* debuglevel,
                     const char* cachesize,
                     const char* workers,
                     const char* mlock,
                     const char* decompress_ratio,
                     const char* image_offset);

int mount_memfs(const void* data, const unsigned int size, const char* image_offset, const char* path);

int mount_memfs(const void* data,
                const unsigned int size,
                const char* image_offset,
                uint32_t parent_inode,
                const char* path);

void unmount_root_memfs(void);

int dwarfs_access(const std::string&, int amode, uid_t uid, gid_t gid, std::string& lnk) noexcept;
int dwarfs_lstat(const std::string&, struct stat* buf, std::string& lnk) noexcept;
int dwarfs_readlink(const std::string& path, std::string& link, std::string& lnk) noexcept;
int dwarfs_stat(const std::string& path, struct stat* buf, std::string& lnk, bool follow) noexcept;

int dwarfs_inode_access(uint32_t inode, int amode, uid_t uid, gid_t gid) noexcept;
int dwarfs_relative_stat(const std::string& path, struct stat* st, std::string& lnk, bool follow) noexcept;
int dwarfs_inode_relative_stat(uint32_t inode,
                               const std::string& path,
                               struct stat* buf,
                               std::string& lnk,
                               bool follow) noexcept;
ssize_t dwarfs_inode_read(uint32_t inode, void* buf, size_t size, off_t offset) noexcept;
int dwarfs_inode_readdir(uint32_t inode,
                         tebako::tebako_dirent* cache,
                         off_t cache_start,
                         size_t buffer_size,
                         size_t& cache_size,
                         size_t& dir_size) noexcept;

}  // namespace tebako
