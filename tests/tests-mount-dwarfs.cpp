/**
 *
 * Copyright (c) 2024, [Ribose Inc](https://www.ribose.com).
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

#include <tebako/fs/internal/mount_table.h>

namespace tebako {

class DwarfsMountTests : public ::testing::Test {
 protected:
  static std::vector<char> buffer;
  static std::streamsize size;
  static bool cross_test;

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
    cross_test = (std::getenv("TEBAKO_CROSS_TEST") != NULL);
#ifdef _WIN32
    _set_invalid_parameter_handler(invalidParameterHandler);
#endif
    mount_root_memfs(&gfsData[0], gfsSize, tests_log_level(), nullptr, nullptr, nullptr, nullptr, nullptr);
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

    int ret = mount_memfs_at_root(buffer.data(), size, "auto", "m-dir-at-second-memfs");
  }

  static void TearDownTestSuite()
  {
    unmount_root_memfs();
  }
};

bool DwarfsMountTests::cross_test = false;
std::vector<char> DwarfsMountTests::buffer;
std::streamsize DwarfsMountTests::size;

TEST_F(DwarfsMountTests, mount_directory)
{
  if (!cross_test) {
    DIR* dirp = tebako_opendir(TEBAKIZE_PATH("m-dir-at-second-memfs"));
    EXPECT_TRUE(dirp != NULL);
    if (dirp != NULL) {
#ifdef _WIN32
      struct direct* entry;
#else
      struct dirent* entry;
#endif
      std::string fname = "fs2-file.txt";
      bool found = false;

#ifdef _WIN32
      while ((entry = tebako_readdir(dirp, NULL)) != NULL) {
#else
      while ((entry = tebako_readdir(dirp)) != NULL) {
#endif
        if (fname == entry->d_name)
          found = true;
      }
      EXPECT_TRUE(found);
      EXPECT_EQ(0, tebako_closedir(dirp));
    }
  }
  else {
    GTEST_SKIP();
  }
}

TEST_F(DwarfsMountTests, mount_open_read)
{
  if (!cross_test) {
    int fh = tebako_open(2, TEBAKIZE_PATH("m-dir-at-second-memfs/fs2-file2.txt"), O_RDONLY);
    EXPECT_LT(0, fh);

    char readbuf[128];
    const char* pattern = "Second file at the seconf memfs";
    const int num2read = strlen(pattern);
    EXPECT_EQ(num2read, tebako_read(fh, readbuf, sizeof(readbuf) / sizeof(readbuf[0])));
    EXPECT_EQ(0, strncmp(readbuf, pattern, num2read));
    EXPECT_EQ(0, tebako_close(fh));
  }
  else {
    GTEST_SKIP();
  }
}

TEST_F(DwarfsMountTests, mount_open_file_no_file)
{
  if (!cross_test) {
    int fh = tebako_open(2, TEBAKIZE_PATH("m-dir-at-second-memfs/fs2-no-file.txt"), O_RDONLY);
    EXPECT_EQ(-1, fh);
    EXPECT_EQ(ENOENT, errno);
  }
  else {
    GTEST_SKIP();
  }
}

#ifdef WITH_LINK_TESTS
TEST_F(DwarfsMountTests, mount_symlink)
{
  if (!cross_test) {
    int fh = tebako_open(2, TEBAKIZE_PATH("m-dir-at-second-memfs/s-link-outside-of-memfs"), O_RDONLY);
    EXPECT_LT(0, fh);

    char readbuf[128];
    const char* pattern = "This is just a file outside of memfs that will be symlinked from inside memfs";
    const int num2read = strlen(pattern);
    EXPECT_EQ(num2read, tebako_read(fh, readbuf, sizeof(readbuf) / sizeof(readbuf[0])));
    EXPECT_EQ(0, strncmp(readbuf, pattern, num2read));
    EXPECT_EQ(0, tebako_close(fh));
  }
  else {
    GTEST_SKIP();
  }
}

TEST_F(DwarfsMountTests, mount_symlink_readlink)
{
  if (!cross_test) {
    char readbuf[128];
    const char* pattern = "a-file-outside-of-memfs.txt";

    int ret = tebako_readlink(TEBAKIZE_PATH("m-dir-at-second-memfs/s-link-outside-of-memfs"), readbuf,
                              sizeof(readbuf) / sizeof(readbuf[0]));
    EXPECT_LT(0, ret);
    EXPECT_TRUE(strstr(readbuf, pattern) != 0);
  }
  else {
    GTEST_SKIP();
  }
}

TEST_F(DwarfsMountTests, mount_symlink_lstat)
{
  if (!cross_test) {
    struct STAT_TYPE st;
    int ret = tebako_lstat(TEBAKIZE_PATH("m-dir-at-second-memfs/s-link-outside-of-memfs"), &st);
    EXPECT_EQ(0, ret);
  }
  else {
    GTEST_SKIP();
  }
}
#endif

#ifdef TEBAKO_HAS_OPENAT
TEST_F(DwarfsMountTests, mount_and_openat)
{
  int fh1 = tebako_open(2, TEBAKIZE_PATH(""), O_RDONLY);
  EXPECT_LT(0, fh1);

  int fh2 = tebako_openat(3, fh1, "m-dir-at-second-memfs/fs2-file2.txt", O_RDONLY);
  EXPECT_LT(0, fh2);

  EXPECT_EQ(0, tebako_close(fh1));
  EXPECT_EQ(0, tebako_close(fh2));
}

TEST_F(DwarfsMountTests, mount_and_openat_l1)
{
  int fh1 = tebako_open(2, TEBAKIZE_PATH("m-dir-at-second-memfs"), O_RDONLY);
  EXPECT_LT(0, fh1);

  int fh2 = tebako_openat(3, fh1, "fs2-directory-1/fs2-file-in-directory-1.txt", O_RDONLY);
  EXPECT_LT(0, fh2);

  EXPECT_EQ(0, tebako_close(fh1));
  EXPECT_EQ(0, tebako_close(fh2));
}

TEST_F(DwarfsMountTests, mount_and_openat_l2)
{
  int fh1 = tebako_open(2, TEBAKIZE_PATH("m-dir-at-second-memfs/fs2-directory-1"), O_RDONLY);
  EXPECT_LT(0, fh1);

  int fh2 = tebako_openat(3, fh1, "fs2-file-in-directory-1.txt", O_RDONLY);
  EXPECT_LT(0, fh2);

  EXPECT_EQ(0, tebako_close(fh1));
  EXPECT_EQ(0, tebako_close(fh2));
}
#endif

#ifdef TEBAKO_HAS_FSTATAT
TEST_F(DwarfsMountTests, mount_and_fstatat)
{
  int fh = tebako_open(2, TEBAKIZE_PATH("m-dir-at-second-memfs"), O_RDONLY);
  EXPECT_LT(0, fh);

  struct STAT_TYPE st;
  int ret = tebako_fstatat(fh, "fs2-directory-2/fs2-file-in-directory-2.txt", &st, 0);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, tebako_close(fh));
}
#endif

TEST_F(DwarfsMountTests, tebako_access_relative_path)
{
  int ret = tebako_chdir(TEBAKIZE_PATH("m-dir-at-second-memfs/fs2-directory-1"));
  EXPECT_EQ(0, ret);
  ret = tebako_access("fs2-file-in-directory-1.txt", F_OK);
  EXPECT_EQ(0, ret);
}

TEST_F(DwarfsMountTests, tebako_access_absolute_path_no_file)
{
  int ret = tebako_access(TEBAKIZE_PATH("m-dir-at-second-memfs/fs2-directory-1/no-file"), F_OK);
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(ENOENT, errno);
}

}  // namespace tebako
