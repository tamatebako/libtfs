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
#include <tebako/fs/internal/mount_table.h>

namespace tebako {

class HostFolderMountTests : public ::testing::Test {
 protected:
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
  }

  static void TearDownTestSuite()
  {
    unmount_root_memfs();
  }

  sync_tebako_mount_table& mount_table = sync_tebako_mount_table::get_tebako_mount_table();

  void SetUp() override
  {
    mount_table.clear();
    mount_table.insert(0, "m-dir-outside-of-memfs", tests_outside_dir());
  }

  void TearDown() override
  {
    mount_table.clear();
  }
};

bool HostFolderMountTests::cross_test = false;

TEST_F(HostFolderMountTests, mount_directory)
{
  if (!cross_test) {
    DIR* dirp = tebako_opendir(TEBAKIZE_PATH("m-dir-outside-of-memfs"));
    EXPECT_TRUE(dirp != NULL);
    if (dirp != NULL) {
#ifdef _WIN32
      struct direct* entry;
#else
      struct dirent* entry;
#endif
      std::string fname = "a-file-outside-of-memfs.txt";
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

TEST_F(HostFolderMountTests, mount_file)
{
  if (!cross_test) {
    int fh = tebako_open(2, TEBAKIZE_PATH("m-dir-outside-of-memfs/a-file-outside-of-memfs.txt"), O_RDONLY);
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

#ifdef WITH_LINK_TESTS
TEST_F(HostFolderMountTests, mount_symlink)
{
  if (!cross_test) {
    int fh = tebako_open(2, TEBAKIZE_PATH("m-dir-outside-of-memfs/o-link-outside-of-memfs"), O_RDONLY);
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

TEST_F(HostFolderMountTests, mount_symlink_readlink)
{
  if (!cross_test) {
    char readbuf[128];
    const char* pattern = "a-file-outside-of-memfs.txt";

    int ret = tebako_readlink(TEBAKIZE_PATH("m-dir-outside-of-memfs/o-link-outside-of-memfs"), readbuf,
                              sizeof(readbuf) / sizeof(readbuf[0]));
    EXPECT_LT(0, ret);
    EXPECT_TRUE(strstr(readbuf, pattern) != 0);
  }
  else {
    GTEST_SKIP();
  }
}

TEST_F(HostFolderMountTests, mount_symlink_lstat)
{
  if (!cross_test) {
    struct STAT_TYPE st;
    int ret = tebako_lstat(TEBAKIZE_PATH("m-dir-outside-of-memfs/o-link-outside-of-memfs"), &st);
    EXPECT_EQ(0, ret);
  }
  else {
    GTEST_SKIP();
  }
}

#endif

#ifdef TEBAKO_HAS_OPENAT
TEST_F(HostFolderMountTests, mount_and_openat)
{
  mount_table.insert(0, "m-bin", __BIN__);

  int fh1 = tebako_open(2, TEBAKIZE_PATH(""), O_RDONLY);
  EXPECT_LT(0, fh1);

  int fh2 = tebako_openat(3, fh1, "m-bin" __S__ __SHELL__, O_RDONLY);
  EXPECT_LT(0, fh2);

  EXPECT_EQ(0, tebako_close(fh1));
  EXPECT_EQ(0, tebako_close(fh2));
}
#endif

#ifdef TEBAKO_HAS_FSTATAT
TEST_F(HostFolderMountTests, mount_and_fstatat)
{
  mount_table.insert(0, "m-bin", __BIN__);

  int fh = tebako_open(2, TEBAKIZE_PATH(""), O_RDONLY);
  EXPECT_LT(0, fh);

  struct STAT_TYPE st;
  int ret = tebako_fstatat(fh, "m-bin" __S__ __SHELL__, &st, 0);
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, tebako_close(fh));
}
#endif

// #define -- for TEBAKIZE_PATH
#define TMP_D_NAME_1 "tebako-test-dir-for-mount-1"
#define TMP_D_NAME_2 "tebako-test-dir-for-mount-2"
#define TMP_F_NAME_1 "tebako-test-file-for-mount-1"
#define TMP_F_NAME_2 "tebako-test-file-for-mount-2"
#define TMP_F_NAME_3 "tebako-test-file-for-mount-3"
#define TMP_F_NAME_4 "tebako-test-file-for-mount-4"

TEST_F(HostFolderMountTests, mount_mkdir_rmdir_1)
{
  mount_table.insert(0, "m-tmp", __TMP__);

#if defined(TEBAKO_HAS_POSIX_MKDIR) || defined(RB_W32)
  int ret = tebako_mkdir(TEBAKIZE_PATH("m-tmp" __S__ TMP_D_NAME_1), S_IRWXU);
#else
  int ret = tebako_mkdir(TEBAKIZE_PATH("m-tmp" __S__ TMP_D_NAME_1));
#endif
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, tebako_rmdir(__AT_TMP__(TMP_D_NAME_1)));
}

TEST_F(HostFolderMountTests, mount_mkdir_rmdir_2)
{
  mount_table.insert(0, "m-tmp", __TMP__);

#if defined(TEBAKO_HAS_POSIX_MKDIR) || defined(RB_W32)
  int ret = tebako_mkdir(TEBAKIZE_PATH("m-tmp" __S__ TMP_D_NAME_2), S_IRWXU);
#else
  int ret = tebako_mkdir(TEBAKIZE_PATH("m-tmp" __S__ TMP_D_NAME_2));
#endif
  EXPECT_EQ(0, ret);

  EXPECT_EQ(0, tebako_rmdir(TEBAKIZE_PATH("m-tmp" __S__ TMP_D_NAME_2)));
}

TEST_F(HostFolderMountTests, mount_open_creat_unlink_1)
{
  mount_table.insert(0, "m-tmp", __TMP__);

  int fh = tebako_open(3, TEBAKIZE_PATH("m-tmp" __S__ TMP_F_NAME_1), O_CREAT, S_IRWXU);
  EXPECT_LT(0, fh);
  EXPECT_EQ(0, tebako_close(fh));

  EXPECT_EQ(0, tebako_unlink(__AT_TMP__(TMP_F_NAME_1)));
}

TEST_F(HostFolderMountTests, mount_open_creat_unlink_2)
{
  mount_table.insert(0, "m-tmp", __TMP__);

  int fh = tebako_open(3, TEBAKIZE_PATH("m-tmp" __S__ TMP_F_NAME_2), O_CREAT, S_IRWXU);
  EXPECT_LT(0, fh);
  EXPECT_EQ(0, tebako_close(fh));

  EXPECT_EQ(0, tebako_unlink(TEBAKIZE_PATH("m-tmp" __S__ TMP_F_NAME_2)));
}

#if defined(TEBAKO_HAS_OPENAT) && defined(O_DIRECTORY)
TEST_F(HostFolderMountTests, mount_openat_creat_unlink_1)
{
  mount_table.insert(0, "m-tmp", __TMP__);

  int fh1 = tebako_open(3, TEBAKIZE_PATH("m-tmp"), O_DIRECTORY | O_RDONLY);
  EXPECT_LT(0, fh1);
  int fh2 = tebako_openat(4, fh1, TMP_F_NAME_3, O_CREAT, S_IRWXU);
  EXPECT_LT(0, fh2);
  EXPECT_EQ(0, tebako_close(fh1));
  EXPECT_EQ(0, tebako_close(fh2));

  EXPECT_EQ(0, tebako_unlink(__AT_TMP__(TMP_F_NAME_3)));
}

TEST_F(HostFolderMountTests, mount_openat_creat_unlink_2)
{
  mount_table.insert(0, "m-tmp", __TMP__);

  int fh1 = tebako_open(3, TEBAKIZE_PATH("m-tmp"), O_DIRECTORY | O_RDONLY);
  EXPECT_LT(0, fh1);
  int fh2 = tebako_openat(4, fh1, TMP_F_NAME_4, O_CREAT, S_IRWXU);
  EXPECT_LT(0, fh2);
  EXPECT_EQ(0, tebako_close(fh1));
  EXPECT_EQ(0, tebako_close(fh2));

  EXPECT_EQ(0, tebako_unlink(TEBAKIZE_PATH("m-tmp" __S__ TMP_F_NAME_4)));
}
#endif
}  // namespace tebako
