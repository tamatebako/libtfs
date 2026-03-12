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

#include <dwarfs/error.h>
#include <dwarfs/file_stat.h>
#include <dwarfs/logger_level.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/fstypes.h>
#include <dwarfs/reader/mlock_mode.h>
#include <dwarfs/logger.h>
#include <dwarfs/reader/filesystem_options.h>

void tebako_init_cwd(dwarfs::stream_logger& lgr, bool need_debug_policy);
void tebako_drop_cwd(void);

namespace tebako {

struct memfs_options {
  int readonly{0};
  int cache_image{0};
  int enable_nlink{0};
  size_t cachesize{(static_cast<size_t>(512) << 20)};
  size_t workers{2};
  dwarfs::reader::mlock_mode lock_mode{dwarfs::reader::mlock_mode::NONE};
  double decompress_ratio{0.8};
  dwarfs::logger::level_type debuglevel{dwarfs::LOGGER_LEVEL_INFO};
};

class memfs {
 private:
  const void* data;
  const unsigned int size;
  uint32_t dwarfs_root_inode;

  dwarfs::reader::filesystem_options fsopts;
  dwarfs::reader::filesystem_v2 fs;

  //  std::shared_ptr<dwarfs::performance_monitor> perfmon;

 public:
  static void set_cachesize(const char* cachesize);
  static void set_debuglevel(const char* debuglevel);
  static void set_decompress_ratio(const char* decompress_ratio);
  static void set_lock_mode(const char* mlock);
  static void set_workers(const char* workers);

  static dwarfs::stream_logger& logger();
  static memfs_options& options();

  memfs(const void* dt, const unsigned int sz, uint32_t df_root = 0);

  int load(const char* image_offset = "auto");
  void set_image_offset_str(const char* image_offset = "auto");
  uint32_t get_root_inode(void) { return dwarfs_root_inode; }
  void set_root_inode(uint32_t df_root_inode) { dwarfs_root_inode = df_root_inode; }

  int access(const std::string& path, int amode, uid_t uid, gid_t gid, std::string& lnk) noexcept;
  int inode_access(uint32_t inode, int amode, uid_t uid, gid_t gid) noexcept;
  ssize_t inode_read(uint32_t inode, void* buf, size_t size, off_t offset) noexcept;
  int inode_readdir(uint32_t inode,
                    tebako_dirent* cache,
                    off_t cache_start,
                    size_t buffer_size,
                    size_t& cache_size,
                    size_t& dir_size) noexcept;
  int inode_readlink(uint32_t inode, std::string& lnk) noexcept;

  int stat(const std::string& path, struct stat* st, std::string& lnk, bool follow) noexcept
  {
    return find_inode_root(path, follow, lnk, st);
  }
  int inode_relative_stat(uint32_t inode,
                          const std::string& path,
                          struct stat* st,
                          std::string& lnk,
                          bool follow) noexcept
  {
    return find_inode_abs(inode, path, follow, lnk, st);
  }
  int relative_stat(const std::string& path, struct stat* st, std::string& lnk, bool follow) noexcept
  {
    return find_inode_abs(dwarfs_root_inode, path, follow, lnk, st);
  }

#if defined(TEBAKO_HAS_LSTAT) || defined(RB_W32) || defined(_WIN32)
  int lstat(const std::string& path, struct stat* st, std::string& lnk) noexcept
  {
    return find_inode_root(path, false, lnk, st);
  }
#endif

  int readlink(const std::string& path, std::string& link, std::string& lnk) noexcept;

 private:
  int i_access(int amode, struct stat* st);

  int dwarfs_file_stat(dwarfs::reader::inode_view& inode, struct stat* st);

  int find_inode(uint32_t start_from,
                 const stdfs::path& path,
                 bool follow_last,
                 std::string& lnk,
                 struct stat* st) noexcept;
  int find_inode_abs(uint32_t start_from,
                     const stdfs::path& path,
                     bool follow,
                     std::string& lnk,
                     struct stat* st) noexcept;
  int find_inode_root(const std::string& path, bool follow, std::string& lnk, struct stat* st) noexcept;

  int process_inode(dwarfs::reader::inode_view& pi,
                    dwarfs::file_stat* st,
                    bool follow,
                    std::string& lnk,
                    stdfs::path::iterator& p_iterator,
                    stdfs::path& p_path);
  int process_link(std::string& lnk, stdfs::path::iterator& p_iterator, stdfs::path& p_path);

  template <typename Functor, class... Args>
  int safe_dwarfs_call(Functor&& fn, const char* caller, uint32_t inode, Args&&... args);
};

}  // namespace tebako
