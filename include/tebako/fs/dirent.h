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

// Include system headers FIRST before any namespace declarations
#include <sys/types.h>
#include <stddef.h>

// C++ standard library
#include <map>
#include <memory>

// Tebako project headers
#include <tebako/fs/common.h>             // For tebako_path_t
#include <tebako/fs/util/synchronized.h>  // For Synchronized template

/* The d_name field
    Warning: applications should avoid any dependence on the size of
    the d_name field. POSIX defines it as char d_name[], a character
    array of unspecified size, with at most NAME_MAX characters
    preceding the terminating null byte('\0').

    This implementation uses a pure buffer approach to avoid depending
    on system struct dirent layout, following POSIX recommendations.
*/

#ifdef RB_W32
#include <tebako-io-rb-w32.h>

namespace tebako {
typedef struct tebako_dirent {
  struct direct e;
  tebako_path_t d_name;
} tebako_dirent;
}  // namespace tebako
#else
namespace tebako {

// Pure buffer approach - no dependency on struct dirent layout
// Access system dirent through reinterpret_cast when needed
// This provides clean separation and maximum portability
struct tebako_dirent {
  // Buffer sized generously to hold any system dirent structure
  // Typical struct dirent is ~280 bytes, we allocate 1KB for safety
  alignas(8) char buffer[1024];

  // Accessor to system dirent (returns void* to avoid header dependency)
  // Use tebako_system_dirent_t typedef from PCH in implementation files
  void* as_dirent() { return static_cast<void*>(buffer); }
  const void* as_dirent() const { return static_cast<const void*>(buffer); }

  // For backward compatibility with old union interface
  // Returns void* to avoid struct dirent dependency in header
  void* e() { return as_dirent(); }
  const void* e() const { return as_dirent(); }
};
}  // namespace tebako
#endif

namespace tebako {
const size_t TEBAKO_DIR_CACHE_SIZE = 50;

struct tebako_ds {
  tebako_dirent cache[TEBAKO_DIR_CACHE_SIZE];
  size_t dir_size;
  long dir_position;
  off_t cache_start;
  size_t cache_size;
  int vfd;

  tebako_ds(int fd) : cache_size(0), cache_start(0), dir_position(-1), dir_size(0), vfd(fd) {}

  int load_cache(int new_cache_start, bool set_pos = false) noexcept;
};

// sync_tebako_dstable
// This class manages dwarfs directories opened with opendir (tebako_opendir)
// Each opened directory is mapped to tebako_ds structure that can be traversed
// by functions like readdir or seekdir
// File handler is supposed to be managed by sync_tebako_fdtable (tebako-fd)

typedef std::map<uintptr_t, std::shared_ptr<tebako_ds>> tebako_dstable;

class sync_tebako_dstable {
 private:
  tebako::Synchronized<tebako_dstable> s_tebako_dstable;

 public:
  static sync_tebako_dstable& get_tebako_dstable(void);
  uintptr_t opendir(int vfd, size_t& size) noexcept;
  uintptr_t opendir(int vfd) noexcept
  {
    size_t size;
    return opendir(vfd, size);
  }
  int closedir(uintptr_t dirp) noexcept;
  void close_all(void) noexcept;
  long telldir(uintptr_t dirp) noexcept;
  int seekdir(uintptr_t dirp, long pos) noexcept;
  long dirfd(uintptr_t dirp) noexcept;
  int readdir(uintptr_t dirp, tebako_dirent*& entry) noexcept;
};

// Helper function to populate tebako_dirent structure
// Implemented in tebako-dirent.cpp where struct dirent is properly accessible
void populate_tebako_dirent(tebako_dirent& entry,
                            ino_t ino,
                            off_t offset,
                            mode_t mode,
                            const char* name,
                            size_t name_len);

}  // namespace tebako
