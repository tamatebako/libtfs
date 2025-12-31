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
 *CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 **/
#include "tests.h"

#ifdef _WIN32
#undef lseek
#undef close
#undef read
#undef pread

#undef chdir
#undef mkdir
#undef rmdir
#undef unlink
#undef access
#undef fstat
#undef stat
#undef lstat
#undef getcwd
#undef opendir
#undef readdir
#undef telldir
#undef seekdir
#undef rewinddir
#undef closedir
#endif

struct tebako_dirent;

#include <tebako-io-inner.h>
#include <tebako/fs/memfs.h>
#include <tebako/fs/internal/memfs_table.h>
#include <tebako/fs/internal/mount_table.h>

namespace {
class LoadTests2 : public ::testing::Test {
 protected:
  static std::vector<char> buffer;
  static std::streamsize size;

#ifdef _WIN32
  static void invalidParameterHandler(const wchar_t* p1,
                                      const wchar_t* p2,
                                      const wchar_t* p3,
                                      unsigned int p4,
                                      uintptr_t p5)
  {
    // Just return to pass execution to standard library
    // otherwise exception will be thrown by MSVC runtime
    return;
  }
#endif

  static void SetUpTestSuite()
  {
#ifdef _WIN32
    _set_invalid_parameter_handler(invalidParameterHandler);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
    // Load image for the second filesystem
    std::string filename = tests_the_other_memfs_image();
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
      FAIL() << "Failed to open file: " << filename;
    }

    size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize(size);
    if (!file.read(buffer.data(), size)) {
      FAIL() << "Failed to read file: " << filename;
    }
  }

  static void TearDownTestSuite() {}

  void SetUp() override
  {
    mount_root_memfs(&gfsData[0], gfsSize, tests_log_level(), nullptr, nullptr, nullptr, nullptr, nullptr);
  }

  void TearDown() override
  {
    unmount_root_memfs();
  }
};

std::vector<char> LoadTests2::buffer;
std::streamsize LoadTests2::size;

TEST_F(LoadTests2, tebako_load_valid_filesystem)
{
  int ret = mount_memfs(buffer.data(), size, "auto", 0, "dummy");
  EXPECT_LE(1, ret);
}

TEST_F(LoadTests2, tebako_load_invalid_filesystem)
{
  const unsigned char data[] = "This is broken filesystem image";
  int ret = mount_memfs_at_root(&data[0], sizeof(data) / sizeof(data[0]), nullptr, "dummy");
  EXPECT_EQ(-1, ret);

  // Check that root fs is alive
  struct STAT_TYPE buf;
  errno = 0;
  ret = tebako_stat(TEBAKIZE_PATH("file.txt"), &buf);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(0, errno);
}

TEST_F(LoadTests2, tebako_load_with_offset_invalid)
{
  int ret = mount_memfs(buffer.data(), size, "xxx", 0, "dummy");
  EXPECT_EQ(-1, ret);

  // Check that root fs is alive
  struct STAT_TYPE buf;
  errno = 0;
  ret = tebako_stat(TEBAKIZE_PATH("file.txt"), &buf);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(0, errno);
}

TEST_F(LoadTests2, tebako_stat_not_loaded_filesystem)
{
  tebako::sync_tebako_mount_table& mount_table = tebako::sync_tebako_mount_table::get_tebako_mount_table();
  mount_table.insert(0, "m-dir-at-second-memfs", 5);

  struct STAT_TYPE buf;
  int ret = tebako_stat(TEBAKIZE_PATH("m-dir-at-second-memfs/file.txt"), &buf);
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(ENOENT, errno);
}

TEST_F(LoadTests2, tebako_access_not_loaded_filesystem)
{
  tebako::sync_tebako_mount_table& mount_table = tebako::sync_tebako_mount_table::get_tebako_mount_table();
  mount_table.insert(0, "m-dir-at-second-memfs", 5);

  struct STAT_TYPE buf;
  int ret = tebako_access(TEBAKIZE_PATH("m-dir-at-second-memfs/file.txt"), W_OK);
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(ENOENT, errno);
}

TEST_F(LoadTests2, tebako_open_not_loaded_filesystem)
{
  tebako::sync_tebako_mount_table& mount_table = tebako::sync_tebako_mount_table::get_tebako_mount_table();
  mount_table.insert(0, "m-dir-at-second-memfs", 5);

  struct STAT_TYPE buf;
  int ret = tebako_open(2, TEBAKIZE_PATH("m-dir-at-second-memfs/file.txt"), O_RDONLY);
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(ENOENT, errno);
}

}  // namespace
