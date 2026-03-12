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
#include <tebako/fs/internal/fd_table.h>

using namespace std;

struct tebako_dlerror_data {
  int err;
  string fname;
  tebako_dlerror_data(int e, const char* f) : err(e), fname(f){};
  tebako_dlerror_data(void) : err(0), fname(""){};
};

static tebako::Synchronized<tebako_dlerror_data> tebako_dlerror_stash;
static char tebako_dlerror_msg[TEBAKO_PATH_LENGTH + 1024];

typedef map<string, string> tebako_dltable;

class sync_tebako_dltable : public tebako::Synchronized<tebako_dltable*> {
 private:
  stdfs::path dl_tmpdir;

  void create_temporary_directory(void)
  {
    const uint64_t MAX_TRIES = 1024;
    auto tmp_dir = stdfs::temp_directory_path();
    stdfs::path _dl_tmpdir;
    uint64_t i = 1;
    std::random_device dev;
    std::mt19937 prng(dev());
    std::uniform_int_distribution<uint64_t> rand(0);
    while (true) {
      std::stringstream ss;
      ss << std::hex << rand(prng);
      _dl_tmpdir = tmp_dir / ss.str();
      if (stdfs::create_directory(_dl_tmpdir)) {
        dl_tmpdir = std::move(_dl_tmpdir);
        break;
      }
      if (i == MAX_TRIES) {
        throw std::runtime_error("Could not create temporary directory");
      }
      i++;
    }
  }

  void map_name(const char* path, std::string& mapped)
  {
    const char* adj = path[TEBAKO_MOUNT_POINT_LENGTH] == '\0' ? path + TEBAKO_MOUNT_POINT_LENGTH
                                                              : path + TEBAKO_MOUNT_POINT_LENGTH + 1;
    stdfs::path _mapped = dl_tmpdir / adj;
    stdfs::create_directories(_mapped.parent_path());
    mapped = _mapped.make_preferred().string();
  }

 public:
  sync_tebako_dltable(void) : tebako::Synchronized<tebako_dltable*>(new tebako_dltable)
  {
    create_temporary_directory();
  }
  ~sync_tebako_dltable(void)
  {
    auto p_dltable = *wlock();
    for (auto it = p_dltable->begin(); it != p_dltable->end(); ++it) {
      ::unlink(it->second.c_str());
    }
    p_dltable->clear();
    if (!dl_tmpdir.empty()) {
      std::error_code ec;
      stdfs::remove_all(dl_tmpdir, ec);
    }
  }

  string map2file(const char* path)
  {
    string ret = "";
    auto p_dltable = *wlock();
    auto p_dl = p_dltable->find(path);
    if (p_dl != p_dltable->end()) {
      ret = p_dl->second;
    }
    else {
      try {
        std::string mapped;
        map_name(path, mapped);
        int fh_in = tebako_open(2, path, O_RDONLY);
        size_t f_size = 0;
        if (fh_in < 0) {
          *tebako_dlerror_stash.wlock() = tebako_dlerror_data(ENOENT, path);
          fh_in = -1;
        }
        else {
          struct STAT_TYPE st;
          if (tebako_fstat(fh_in, &st) == -1) {
            *tebako_dlerror_stash.wlock() = tebako_dlerror_data(EIO, path);
          }
          else {
            int fh_out = ::open(mapped.c_str(), O_WRONLY | O_CREAT | O_BINARY, st.st_mode);
            if (fh_out < 0) {
              *tebako_dlerror_stash.wlock() = tebako_dlerror_data(EIO, path);
            }
            else {
              f_size = st.st_size;
              const int bsize = 16 * 1024;
              char buf[bsize];
              ssize_t r_size;
              while (f_size > 0 && (r_size = tebako_read(fh_in, buf, bsize)) > 0) {
                f_size -= r_size;
                if (r_size != ::write(fh_out, buf, r_size))
                  break;
              }
              if (f_size == 0) {
                ret = mapped;
                (*p_dltable)[path] = std::move(mapped);
              }
              else {
                ::unlink(mapped.c_str());
                *tebako_dlerror_stash.wlock() = tebako_dlerror_data(EIO, path);
              }
              ::close(fh_out);
            }
          }
          tebako_close(fh_in);
        }
      }
      catch (...) {
        *tebako_dlerror_stash.wlock() = tebako_dlerror_data(ENOMEM, path);
      }
    }
    return ret;
  }
  static sync_tebako_dltable dltable;
};

sync_tebako_dltable sync_tebako_dltable::dltable;

extern "C" void* tebako_dlopen(const char* path, int flags)
{
  void* ret = NULL;
  if (path == NULL) {
    //	If filename is NULL, then the returned handle is for the main program.
    ret = ::dlopen(path, flags);
  }
  else {
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (!p_path) {
      ret = ::dlopen(path, flags);
    }
    else {
      string mapped_path = sync_tebako_dltable::dltable.map2file(p_path);
      if (!mapped_path.empty()) {
        ret = ::dlopen(mapped_path.c_str(), flags);
      }
    }
  }
  return ret;
}

extern "C" char* tebako_dlmap2file(const char* path)
{
  char* ret = NULL;
  if (path != NULL) {
    tebako_path_t t_path;
    const char* p_path = to_tebako_path(t_path, path);
    if (p_path) {
      string mapped_path = sync_tebako_dltable::dltable.map2file(p_path);
      if (!mapped_path.empty()) {
        ret = strdup(mapped_path.c_str());
      }
    }
  }
  return ret;
}

extern "C" char* tebako_dlerror(void)
{
  string tebako_dlerror_text;
  tebako_dlerror_data last_dl_error = tebako_dlerror_stash.exchange(tebako_dlerror_data());
  if (last_dl_error.err != 0) {
    tebako_dlerror_text = last_dl_error.fname + ": cannot open shared object file: " + strerror(last_dl_error.err);
    strncpy(tebako_dlerror_msg, tebako_dlerror_text.c_str(), sizeof(tebako_dlerror_msg) / sizeof(char) - 1);
    tebako_dlerror_msg[sizeof(tebako_dlerror_msg) / sizeof(char) - 1] = '\0';
  }
  char* native_dlerror_msg = ::dlerror();
  char* ret = last_dl_error.err ? tebako_dlerror_msg : native_dlerror_msg;
  return ret;
}
