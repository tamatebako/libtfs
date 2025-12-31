/**
 *
 * Copyright (c) 2021-2025[Ribose Inc](https://www.ribose.com).
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

/* The d_name field
    The dirent structure definition is taken from the
    glibc headers, and shows the d_name field with a fixed size.

    Warning: applications should avoid any dependence on the size of
    the d_name field.POSIX defines it as char d_name[], a character
    array of unspecified size, with at most NAME_MAX characters
    preceding the terminating null byte('\0').

    POSIX.1 explicitly notes that this field should not be used as an
    lvalue.The standard also notes that the use of sizeof(d_name)
    is incorrect; use strlen(d_name) instead.  (On some systems, this
    field is defined as char d_name[1]!)  By implication, the use
    sizeof(struct dirent) to capture the size of the record including
    the size of d_name is also incorrect.

    Note that while the call

    fpathconf(fd, _PC_NAME_MAX)

    returns the value 255 for most filesystems, on some filesystems
    (e.g., CIFS, Windows SMB servers), the null - terminated filename
    that is(correctly) returned in d_name can actually exceed this
    size.In such cases, the d_reclen field will contain a value
    that exceeds the size of the glibc dirent structure shown above.
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
// Matches the implementation in tebako/fs/dirent.h
struct tebako_dirent {
  alignas(8) char buffer[1024];

  struct dirent* as_dirent() {
    return reinterpret_cast<struct dirent*>(buffer);
  }
  const struct dirent* as_dirent() const {
    return reinterpret_cast<const struct dirent*>(buffer);
  }

  struct dirent* e() { return as_dirent(); }
  const struct dirent* e() const { return as_dirent(); }
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
}  // namespace tebako
