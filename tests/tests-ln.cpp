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

#include "tests.h"
#include <filesystem>
namespace fs = std::filesystem;

namespace {
class LnTests : public testing::Test {
 protected:
  static stdfs::path tmp_path;
  static bool cross_test;
  static bool path_initialized;

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
#endif
    mount_root_memfs(&gfsData[0], gfsSize, tests_log_level(), NULL /* cachesize*/, NULL /* workers */, NULL /* mlock */,
                     NULL /* decompress_ratio*/, NULL /* image_offset */
    );
#ifdef WITH_LINK_TESTS
    std::string tdp_template = (stdfs::temp_directory_path() / "libdwarfs.tests.XXXXXX").generic_string();
    size_t l = tdp_template.length();
    char* dir_name = new char[l + 1];
    if (dir_name) {
      strcpy(dir_name, tdp_template.c_str());
      // *** need different approach on Windows
      dir_name = mkdtemp(dir_name);
      tmp_path = dir_name;
      stdfs::create_directories(tmp_path);
      path_initialized = true;

      stdfs::create_symlink(__AT_BIN__(__SHELL__), tmp_path / "link2shell");

      delete[] dir_name;
    }
#endif
    cross_test = (std::getenv("TEBAKO_CROSS_TEST") != NULL);
  }

  static void TearDownTestSuite()
  {
#ifdef WITH_LINK_TESTS
    if (path_initialized) {
      stdfs::remove_all(tmp_path);
    }
    path_initialized = false;
#endif
    unmount_root_memfs();
  }
};
bool LnTests::cross_test = false;
bool LnTests::path_initialized = false;
stdfs::path LnTests::tmp_path;

#ifdef WITH_LINK_TESTS
TEST_F(LnTests, tebako_softlink)
{
  int fh = tebako_open(2, TEBAKIZE_PATH("s-link-to-file-1"), O_RDONLY);
  EXPECT_LT(0, fh);

  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);

  int ret = tebako_read(fh, readbuf, num2read);
  EXPECT_EQ(num2read, ret);
  EXPECT_EQ(0, strncmp(readbuf, "This is a file in the first directory", num2read));

  ret = tebako_close(fh);
  EXPECT_EQ(0, ret);
}

TEST_F(LnTests, tebako_softlink_not_file)
{
  int fh = tebako_open(2, TEBAKIZE_PATH("s-link-to-dir-1"), O_RDONLY);
  EXPECT_LT(0, fh);

  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);

  int ret = tebako_read(fh, readbuf, num2read);
  EXPECT_EQ(-1, ret);
  EXPECT_EQ(EBADF, errno);

  ret = tebako_close(fh);
  EXPECT_EQ(0, ret);
}

/*
        TEST_F(LnTests, tebako_softlink_to_dir_open) {
                int fh = tebako_open(2,
   TEBAKIZE_PATH("s-link-to-dir-1/file-in-directory-2.txt"), O_RDONLY);
                EXPECT_LT(0, fh);

                char readbuf[32];
                const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);

                int ret = tebako_read(fh, readbuf, num2read);
                EXPECT_EQ(num2read, ret);
                EXPECT_EQ(0, strncmp(readbuf, "This is a file in the second
   directory", num2read));

                ret = tebako_close(fh);
                EXPECT_EQ(0, ret);
        }
*/

TEST_F(LnTests, tebako_hardlink)
{
  int fh = tebako_open(2, TEBAKIZE_PATH("h-link-to-file-2"), O_RDONLY);
  EXPECT_LT(0, fh);

  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);

  int ret = tebako_read(fh, readbuf, num2read);
  EXPECT_EQ(num2read, ret);
  EXPECT_TRUE(strncmp(readbuf, "This is a file in the second directory", num2read) == 0);

  ret = tebako_close(fh);
  EXPECT_EQ(0, ret);
}

TEST_F(LnTests, tebako_readlink_absolute_path_no_file)
{
  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);
  int ret = tebako_readlink(TEBAKIZE_PATH("no_file"), readbuf, num2read);
  EXPECT_EQ(ENOENT, errno);
  EXPECT_EQ(-1, ret);
}

TEST_F(LnTests, tebako_readlink_null)
{
  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);
  int ret = tebako_readlink(NULL, readbuf, num2read);
  EXPECT_EQ(ENOENT, errno);
  EXPECT_EQ(-1, ret);
}

TEST_F(LnTests, tebako_readlink_not_link)
{
  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);
  int ret = tebako_readlink(TEBAKIZE_PATH("file.txt"), readbuf, num2read);
  EXPECT_EQ(EINVAL, errno);
  EXPECT_EQ(-1, ret);
}

TEST_F(LnTests, tebako_readlink_relative_path_no_file)
{
  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);
  int ret = tebako_chdir(TEBAKIZE_PATH("directory-2"));
  EXPECT_EQ(0, ret);
  ret = tebako_readlink("no_file", readbuf, num2read);
  EXPECT_EQ(ENOENT, errno);
  EXPECT_EQ(-1, ret);
}

TEST_F(LnTests, tebako_readlink_absolute_path)
{
  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);
  int ret = tebako_readlink(TEBAKIZE_PATH("s-link-to-file-1"), readbuf, num2read);
  EXPECT_EQ(num2read, ret);
  EXPECT_TRUE(strncmp(readbuf, "directory-1/file-in-directory-1.txt", num2read) == 0);
}

TEST_F(LnTests, tebako_readlink_relative_path)
{
  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);
  EXPECT_EQ(0, tebako_chdir(TEBAKIZE_PATH("")));
  int ret = tebako_readlink("s-link-to-file-1", readbuf, num2read);
  EXPECT_EQ(num2read, ret);
  EXPECT_TRUE(strncmp(readbuf, "directory-1/file-in-directory-1.txt", num2read) == 0);
}

#ifndef _WIN32
TEST_F(LnTests, tebako_readlink_absolute_path_pass_through)
{
  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);
  int ret = tebako_readlink((tmp_path / "link2shell").generic_string().c_str(), readbuf, num2read);
  EXPECT_EQ(strlen(__AT_BIN__(__SHELL__)), ret);
  EXPECT_TRUE(strncmp(readbuf, __AT_BIN__(__SHELL__), ret) == 0);
}

TEST_F(LnTests, tebako_readlink_relative_path_pass_through)
{
  char readbuf[32];
  const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);
  EXPECT_EQ(0, tebako_chdir(tmp_path.generic_string().c_str()));
  int ret = tebako_readlink("link2shell", readbuf, num2read);
  EXPECT_EQ(strlen(__AT_BIN__(__SHELL__)), ret);
  EXPECT_TRUE(strncmp(readbuf, __AT_BIN__(__SHELL__), ret) == 0);
}
#endif

TEST_F(LnTests, tebako_lstat_absolute_path)
{
  struct STAT_TYPE st;
  int ret = tebako_lstat(TEBAKIZE_PATH("s-link-to-file-1"), &st);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(35, st.st_size);

  ret = tebako_stat(TEBAKIZE_PATH("s-link-to-file-1"), &st);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(37, st.st_size);
}

TEST_F(LnTests, tebako_lstat_level_2)
{
  struct STAT_TYPE st;
  int ret = tebako_lstat(TEBAKIZE_PATH("directory-1/s-link-to-level-2"), &st);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(27, st.st_size);

  ret = tebako_stat(TEBAKIZE_PATH("directory-1/s-link-to-level-2"), &st);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(25, st.st_size);
}

TEST_F(LnTests, tebako_lstat_absolute_path_no_file)
{
  struct STAT_TYPE st;
  int ret = tebako_stat(TEBAKIZE_PATH("no_file"), &st);
  EXPECT_EQ(ENOENT, errno);
  EXPECT_EQ(-1, ret);
}

TEST_F(LnTests, tebako_lstat_null)
{
  struct STAT_TYPE st;
  int ret = tebako_lstat(NULL, &st);
  EXPECT_EQ(ENOENT, errno);
  EXPECT_EQ(-1, ret);
}

TEST_F(LnTests, tebako_lstat_relative_path)
{
  struct STAT_TYPE st;
  int ret = tebako_chdir(TEBAKIZE_PATH(""));
  EXPECT_EQ(0, ret);
  ret = tebako_stat("s-link-to-file-1", &st);
  EXPECT_EQ(0, ret);
}

TEST_F(LnTests, tebako_lstat_relative_path_no_file)
{
  struct STAT_TYPE st;
  int ret = tebako_chdir(TEBAKIZE_PATH("directory-2"));
  EXPECT_EQ(0, ret);
  ret = tebako_lstat("no_file", &st);
  EXPECT_EQ(ENOENT, errno);
  EXPECT_EQ(-1, ret);
}

TEST_F(LnTests, tebako_lstat_absolute_path_pass_through)
{
  struct STAT_TYPE st;
  int ret = tebako_lstat((tmp_path / "link2shell").generic_string().c_str(), &st);
  EXPECT_EQ(0, ret);
}

TEST_F(LnTests, tebako_lstat_relative_path_pass_through)
{
  struct STAT_TYPE st;
  int ret = tebako_chdir(tmp_path.generic_string().c_str());
  EXPECT_EQ(0, ret);
  ret = tebako_lstat("link2shell", &st);
  EXPECT_EQ(0, ret);
}

TEST_F(LnTests, tebako_access_link_outside_of_memfs)
{
  if (!cross_test) {
    struct STAT_TYPE st;
    int ret = tebako_access(TEBAKIZE_PATH("s-link-outside-of-memfs"), F_OK);
    EXPECT_EQ(0, ret);
  }
  else {
    GTEST_SKIP();
  }
}

TEST_F(LnTests, tebako_stat_link_outside_of_memfs)
{
  if (!cross_test) {
    struct STAT_TYPE st;
    int ret = tebako_stat(TEBAKIZE_PATH("s-link-outside-of-memfs"), &st);
    EXPECT_EQ(0, ret);
  }
  else {
    GTEST_SKIP();
  }
}

TEST_F(LnTests, tebako_open_link_outside_of_memfs)
{
  if (!cross_test) {
    int fh = tebako_open(2, TEBAKIZE_PATH("s-link-outside-of-memfs"), O_RDONLY);
    EXPECT_LT(0, fh);

    char readbuf[32];
    const int num2read = sizeof(readbuf) / sizeof(readbuf[0]);

    int ret = tebako_read(fh, readbuf, num2read);
    EXPECT_EQ(num2read, ret);
    EXPECT_EQ(0, strncmp(readbuf,
                         "This is just a file outside of memfs that will be "
                         "symlinked from inside memfs",
                         num2read));

    ret = tebako_close(fh);
    EXPECT_EQ(0, ret);
  }
  else {
    GTEST_SKIP();
  }
}

TEST_F(LnTests, tebako_open_dir_outside_of_memfs)
{
  if (!cross_test) {
    DIR* dirp = tebako_opendir(TEBAKIZE_PATH("s-dir-outside-of-memfs"));
    EXPECT_TRUE(dirp != NULL);
    if (dirp != NULL) {
      pdirent entry;
      std::string fname = "a-file-outside-of-memfs.txt";
      bool found = false;
      while ((entry = tebako_readdir_adjusted(dirp)) != NULL) {
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

#ifdef TEBAKO_HAS_SCANDIR
TEST_F(LnTests, tebako_scan_dir_outside_of_memfs)
{
  if (!cross_test) {
    std::string fname = "a-file-outside-of-memfs.txt";
    bool found = false;
    struct dirent** namelist;
    int n = tebako_scandir(TEBAKIZE_PATH("s-dir-outside-of-memfs"), &namelist, NULL, alphasort);
    EXPECT_TRUE(namelist != NULL);
    if (n > 0 && namelist != NULL) {
      for (int i = 0; i < n; i++) {
        EXPECT_TRUE(namelist[i] != NULL);
        if (namelist[i]) {
          if (fname == namelist[i]->d_name)
            found = true;
          free(namelist[i]);
        }
      }
      free(namelist);
    }
    EXPECT_TRUE(found);
  }
  else {
    GTEST_SKIP();
  }
}
#endif

#ifdef TEBAKO_HAS_FSTATAT
TEST_F(LnTests, tebako_fstatat_link_follow_absolute)
{
  struct STAT_TYPE buf;
  int ret = tebako_fstatat(AT_FDCWD, TEBAKIZE_PATH("s-link-to-file-1"), &buf, 0);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(strlen("This is a file in the first directory"),
            buf.st_size);  // Content of the file
}

TEST_F(LnTests, tebako_fstatat_link_nofollow_absolute)
{
  struct STAT_TYPE buf;
  int ret = tebako_fstatat(AT_FDCWD, TEBAKIZE_PATH("s-link-to-file-1"), &buf, AT_SYMLINK_NOFOLLOW);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(strlen("directory-1/file-in-directory-1.txt"),
            buf.st_size);  // The link itself
}

#ifdef O_DIRECTOR
TEST_F(LnTests, tebako_fstatat_link_follow_relative)
{
  struct STAT_TYPE buf;
  int fd = tebako_open(2, TEBAKIZE_PATH(""), O_RDONLY | O_DIRECTORY);
  EXPECT_LT(0, fd);
  int ret = tebako_fstatat(fd, "s-link-to-file-1", &buf, 0);
  EXPECT_EQ(0, ret);
  ret = tebako_close(fd);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(strlen("This is a file in the first directory"),
            buf.st_size);  // Content of the file
}

TEST_F(LnTests, tebako_fstatat_link_nofollow_relative)
{
  struct STAT_TYPE buf;
  int fd = tebako_open(2, TEBAKIZE_PATH(""), O_RDONLY | O_DIRECTORY);
  EXPECT_LT(0, fd);
  int ret = tebako_fstatat(fd, "s-link-to-file-1", &buf, AT_SYMLINK_NOFOLLOW);
  EXPECT_EQ(0, ret);
  ret = tebako_close(fd);
  EXPECT_EQ(0, ret);
  EXPECT_EQ(strlen("directory-1/file-in-directory-1.txt"),
            buf.st_size);  // The link itself
}
#endif  // O_DIRECTORY
#endif

#if defined(TEBAKO_HAS_OPENAT) && defined(O_NOFOLLOW)
TEST_F(LnTests, tebako_openat_link_nofollow_relative)
{
  int fh1 = tebako_open(2, TEBAKIZE_PATH(""), O_RDONLY);
  EXPECT_LT(0, fh1);

  EXPECT_EQ(0, tebako_chdir(TEBAKIZE_PATH("")));
  int fh2 = tebako_openat(3, fh1, "s-link-to-file-1", O_RDONLY | O_NOFOLLOW);
  EXPECT_EQ(-1, fh2);
  EXPECT_EQ(ELOOP, errno);

  EXPECT_EQ(-1, tebako_close(fh2));
  EXPECT_EQ(0, tebako_close(fh1));
}
#endif

#endif
}  // namespace
