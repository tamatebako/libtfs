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
#include <tebako/fs/memfs.h>
#include <tebako/fs/internal/memfs_table.h>
#include <tebako/fs/internal/mount_table.h>
#include <tebako-io-inner.h>
#include <tebako-io-root.h>
#include <tebako/fs/internal/fd_table.h>
#include <tebako/fs/internal/kfd.h>

using namespace dwarfs;

namespace tebako {

static void release_memfs_resources(void)
{
#if defined(TEBAKO_HAS_OPENDIR) || defined(RB_W32)
  sync_tebako_dstable::get_tebako_dstable().close_all();
#endif
  sync_tebako_fdtable::get_tebako_fdtable().close_all();
  sync_tebako_mount_table::get_tebako_mount_table().clear();
  tebako_drop_cwd();
}

static int load_memfs(const void* data, const unsigned int size, const char* image_offset)
{
  LOG_PROXY(debug_logger_policy, memfs::logger());
  LOG_INFO << PRJ_NAME << " mount memfs ";

  int index = sync_tebako_memfs_table::get_tebako_memfs_table().insert_auto(std::make_shared<memfs>(data, size));
  auto fs = sync_tebako_memfs_table::get_tebako_memfs_table().get(index);
  if (fs != nullptr && fs->load(image_offset) != 0) {
    TEBAKO_SET_LAST_ERROR(ENOMEM);
    sync_tebako_memfs_table::get_tebako_memfs_table().erase(index);
    index = -1;
  }
  return index;
}

int mount_memfs(const void* data, const unsigned int size, const char* image_offset, const char* folder)
{
  int root_index = sync_tebako_memfs_table::get_tebako_memfs_table().get(0)->get_root_inode();
  return mount_memfs(data, size, image_offset, root_index, folder);
}

int mount_memfs(const void* data,
                const unsigned int size,
                const char* image_offset,
                uint32_t parent_inode,
                const char* folder)
{
  bool res = false;
  int index = load_memfs(data, size, image_offset);
  if (index != -1) {
    res = sync_tebako_mount_table::get_tebako_mount_table().insert(parent_inode, folder, index);
  }
  return res ? index : -1;
}

int mount_root_memfs(const void* data,
                     const unsigned int size,
                     const char* debuglevel,
                     const char* cachesize,
                     const char* workers,
                     const char* mlock,
                     const char* decompress_ratio,
                     const char* image_offset)
{
  int ret = -1;
  std::set_terminate([]() {
    std::cerr << "Unhandled exception" << std::endl;
    std::abort();
  });

  // Construct the memfs/mount/fd/kfd/dir tables now, at mount time. These
  // are Meyers singletons whose destructors are registered on first use;
  // the caller (e.g. tebako-main) registers its cleanup via atexit() only
  // *after* mount returns, and atexit handlers and static destructors run
  // in LIFO order. Constructing the tables here guarantees their destructors
  // are registered *before* the caller's atexit handler, so at process exit
  // the caller's unmount_root_memfs()/close_all()/clear() runs while the
  // tables are still alive — and not the other way around, which made
  // Synchronized's shared_mutex throw std::system_error and terminated the
  // process with "Unhandled exception" on exit.
  (void)sync_tebako_memfs_table::get_tebako_memfs_table();
  (void)sync_tebako_mount_table::get_tebako_mount_table();
  (void)sync_tebako_fdtable::get_tebako_fdtable();
  (void)sync_tebako_kfdtable::get_tebako_kfdtable();
  (void)sync_tebako_dstable::get_tebako_dstable();

  try {
    memfs::set_debuglevel(debuglevel);
    LOG_PROXY(debug_logger_policy, memfs::logger());
    LOG_INFO << PRJ_NAME << " version " << PRJ_VERSION_STRING;

    try {
      memfs::set_cachesize(cachesize);
      memfs::set_decompress_ratio(decompress_ratio);
      memfs::set_lock_mode(mlock);
      memfs::set_workers(workers);

      auto fs = sync_tebako_memfs_table::get_tebako_memfs_table().get(0);
      if (fs != nullptr) {
        release_memfs_resources();
        sync_tebako_memfs_table::get_tebako_memfs_table().erase(0);
      }

      sync_tebako_memfs_table::get_tebako_memfs_table().insert(0, std::make_shared<memfs>(data, size));
      fs = sync_tebako_memfs_table::get_tebako_memfs_table().get(0);
      if (fs->load(image_offset) == 0) {
        ret = 0;
        tebako_init_cwd(memfs::logger(), memfs::options().debuglevel >= dwarfs::LOGGER_LEVEL_DEBUG);
      }
      else {
        sync_tebako_memfs_table::get_tebako_memfs_table().erase(0);
      }
    }
    catch (std::exception& e) {
      LOG_ERROR << "Error: " << e.what();
    }
    catch (...) {
      LOG_ERROR << "Unexpected exception";
    }
  }
  catch (...) {
    std::cerr << "Failed to initialize logger in " << __func__ << std::endl;
  }

  return ret;
}

void unmount_root_memfs(void)
{
  release_memfs_resources();
  sync_tebako_memfs_table::get_tebako_memfs_table().clear();
}

int dwarfs_access(const std::string& path, int amode, uid_t uid, gid_t gid, std::string& lnk) noexcept
{
  return root_memfs_call(&tebako::memfs::access, path, amode, uid, gid, lnk);
}

int dwarfs_lstat(const std::string& path, struct stat* buf, std::string& lnk) noexcept
{
  return root_memfs_call(&tebako::memfs::lstat, path, buf, lnk);
}

int dwarfs_readlink(const std::string& path, std::string& link, std::string& lnk) noexcept
{
  return root_memfs_call(&tebako::memfs::readlink, path, link, lnk);
}
int dwarfs_stat(const std::string& path, struct stat* buf, std::string& lnk, bool follow) noexcept
{
  return root_memfs_call(&tebako::memfs::stat, path, buf, lnk, follow);
}

int dwarfs_inode_access(uint32_t inode, int amode, uid_t uid, gid_t gid) noexcept
{
  return inode_memfs_call(&tebako::memfs::inode_access, inode, amode, uid, gid);
}

int dwarfs_relative_stat(const std::string& path, struct stat* st, std::string& lnk, bool follow) noexcept
{
  return root_memfs_call(&tebako::memfs::relative_stat, path, st, lnk, follow);
}

int dwarfs_inode_relative_stat(uint32_t inode,
                               const std::string& path,
                               struct stat* buf,
                               std::string& lnk,
                               bool follow) noexcept
{
  return inode_memfs_call(&tebako::memfs::inode_relative_stat, inode, path, buf, lnk, follow);
}
ssize_t dwarfs_inode_read(uint32_t inode, void* buf, size_t size, off_t offset) noexcept
{
  return inode_memfs_call(&tebako::memfs::inode_read, inode, buf, size, offset);
}
int dwarfs_inode_readdir(uint32_t inode,
                         tebako::tebako_dirent* cache,
                         off_t cache_start,
                         size_t buffer_size,
                         size_t& cache_size,
                         size_t& dir_size) noexcept
{
  return inode_memfs_call(&tebako::memfs::inode_readdir, inode, cache, cache_start, buffer_size, cache_size, dir_size);
}

}  // namespace tebako

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
                     const char* image_offset)
{
  return tebako::mount_root_memfs(data, size, debuglevel, cachesize, workers, mlock, decompress_ratio, image_offset);
}

int mount_memfs_at_root(const void* data, const unsigned int size, const char* image_offset, const char* folder)
{
  return tebako::mount_memfs(data, size, image_offset, folder);
}

int mount_memfs(const void* data,
                const unsigned int size,
                const char* image_offset,
                unsigned int parent_inode,
                const char* folder)
{
  return tebako::mount_memfs(data, size, image_offset, parent_inode, folder);
}

void unmount_root_memfs(void)
{
  tebako::unmount_root_memfs();
}
#ifdef __cplusplus
}
#endif  // !__cplusplus
