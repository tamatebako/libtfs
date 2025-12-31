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
#include <tebako/fs/common.h>

namespace {
class DirIOTests : public testing::Test {
 protected:
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
  }

  static void TearDownTestSuite()
  {
    unmount_root_memfs();
  }
};

// No dir io tests without opendir
#if defined(TEBAKO_HAS_OPENDIR) || defined(RB_W32)
TEST_F(DirIOTests, tebako_opendir_no_dir)
{
  DIR* dirp = tebako_opendir(TEBAKIZE_PATH("no_directory"));
  EXPECT_EQ(NULL, dirp);
  EXPECT_EQ(ENOENT, errno);
}

TEST_F(DirIOTests, tebako_opendir_not_dir)
{
  DIR* dirp = tebako_opendir(TEBAKIZE_PATH("file.txt"));
  EXPECT_EQ(NULL, dirp);
  EXPECT_EQ(ENOTDIR, errno);
}

#ifdef TEBAKO_HAS_FDOPENDIR
TEST_F(DirIOTests, tebako_fdopendir_not_dir)
{
  int fh = tebako_open(2, TEBAKIZE_PATH("file.txt"), O_RDONLY);
  EXPECT_LT(0, fh);
  DIR* dirp = tebako_fdopendir(fh);
  EXPECT_EQ(NULL, dirp);
  EXPECT_EQ(ENOTDIR, errno);
  EXPECT_EQ(0, tebako_close(fh));
}

TEST_F(DirIOTests, tebako_fdopendir_not_invalid_handle)
{
  DIR* dirp = tebako_fdopendir(33);
  EXPECT_EQ(NULL, dirp);
  EXPECT_EQ(EBADF, errno);
}

TEST_F(DirIOTests, tebako_fdopendir_closedir_outside)
{
  int fh = tebako_open(2, __BIN__, O_RDONLY);
  EXPECT_LT(0, fh);
  DIR* dirp = tebako_fdopendir(fh);
  EXPECT_TRUE(dirp != NULL);
  EXPECT_EQ(0, tebako_closedir(dirp));
}
#endif

#if defined(TEBAKO_HAS_DIRFD) && defined(TEBAKO_HAS_FDOPENDIR)
TEST_F(DirIOTests, tebako_fdopendir_dirfd_closedir)
{
  int fh = tebako_open(2, TEBAKIZE_PATH("directory-1"), O_RDONLY);
  EXPECT_LT(0, fh);
  DIR* dirp = tebako_fdopendir(fh);
  EXPECT_TRUE(dirp != NULL);
  EXPECT_EQ(fh, tebako_dirfd(dirp));
  EXPECT_EQ(0, tebako_closedir(dirp));
}
#endif

#if defined(TEBAKO_HAS_DIRFD)
TEST_F(DirIOTests, tebako_dirfd_invalid_dirp)
{
  uint some_uint;
  int fh = tebako_dirfd(reinterpret_cast<DIR*>(&some_uint));
  EXPECT_EQ(-1, fh);
  // IMHO it shall be EINVAL
  // https://man7.org/linux/man-pages/man3/dirfd.3.html
  EXPECT_EQ(EBADF, errno);
}

TEST_F(DirIOTests, tebako_dirfd_outside)
{
  DIR* dirp = tebako_opendir(__TMP__);
  int fh = tebako_dirfd(dirp);
  EXPECT_LT(0, fh);
  EXPECT_EQ(0, tebako_closedir(dirp));
}
#endif

#if defined(TEBAKO_HAS_OPENDIR) || defined(RB_W32)
TEST_F(DirIOTests, tebako_opendir_seekdir_telldir_readdir_closedir)
{
  DIR* dirp = tebako_opendir(TEBAKIZE_PATH("directory-with-90-files"));
  EXPECT_TRUE(dirp != NULL);
  if (dirp != NULL) {
    errno = 0;

    const off_t start_fnum = 10; /* The first file nane is file - 10.txt */
    const size_t size_dir = 89;
    long pos = 49;
    std::string fname;

    tebako_seekdir(dirp, pos);
    EXPECT_EQ(pos, tebako_telldir(dirp));

    pdirent entry = tebako_readdir_adjusted(dirp);
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      fname = "file-";
      fname += std::to_string(pos + start_fnum - 2 /* for '.' and '..' */);
      fname += ".txt";
      EXPECT_TRUE(fname == entry->d_name);
      EXPECT_TRUE(entry->d_type == DT_REG);
      EXPECT_EQ(++pos, tebako_telldir(dirp));
    }

    entry = tebako_readdir_adjusted(dirp);
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      fname = "file-";
      fname += std::to_string(pos + start_fnum - 2 /* for '.' and '..' */);
      fname += ".txt";
      EXPECT_TRUE(fname == entry->d_name);
      EXPECT_EQ(++pos, tebako_telldir(dirp));
    }

    entry = tebako_readdir_adjusted(dirp);  // Expecting dirent buffer reload at this point
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      fname = "file-";
      fname += std::to_string(pos + start_fnum - 2 /* for '.' and '..' */);
      fname += ".txt";
      EXPECT_TRUE(fname == entry->d_name);
      EXPECT_EQ(++pos, tebako_telldir(dirp));
    }

    pos = size_dir + 2; /* for '.' and '..' */
    tebako_seekdir(dirp, pos);
    EXPECT_EQ(pos, tebako_telldir(dirp));

    entry = tebako_readdir_adjusted(dirp);
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      fname = "file-";
      fname += std::to_string(pos + start_fnum - 2 /* for '.' and '..' */);
      fname += ".txt";
      EXPECT_TRUE(fname == entry->d_name);
      EXPECT_EQ(++pos, tebako_telldir(dirp));
    }

    entry = tebako_readdir_adjusted(dirp);  // Expecting read beyond dir size
    EXPECT_TRUE(entry == NULL);

    tebako_seekdir(dirp, 0);
    EXPECT_EQ(0, tebako_telldir(dirp));
    entry = tebako_readdir_adjusted(dirp);  // Expecting dirent buffer reload at this point
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      fname = ".";
      EXPECT_TRUE(fname == entry->d_name);
      EXPECT_TRUE(entry->d_type == DT_DIR);
      EXPECT_EQ(1, tebako_telldir(dirp));
    }
    EXPECT_EQ(0, tebako_closedir(dirp));
  }
}
#endif

#if defined(TEBAKO_HAS_OPENDIR) || defined(RB_W32)
TEST_F(DirIOTests, tebako_opendir_seekdir_telldir_readdir_closedir_pass_through)
{
  const char* const shell_name = __SHELL__;
  const char* const shell_folder = __BIN__;

  DIR* dirp = tebako_opendir(shell_folder);
  EXPECT_TRUE(dirp != NULL);
  EXPECT_EQ(0, tebako_telldir(dirp));

  if (dirp != NULL) {
    long loc = -1;
    errno = 0;
    pdirent entry = tebako_readdir_adjusted(dirp);
    while (entry != NULL) {
      long l = tebako_telldir(dirp);
      entry = tebako_readdir_adjusted(dirp);
      if (entry != NULL && strcmp(entry->d_name, shell_name) == 0) {
        loc = l;
      }
    }
    EXPECT_NE(-1, loc);
    EXPECT_EQ(errno, 0);

    if (loc != -1) {
      tebako_seekdir(dirp, loc);
      entry = tebako_readdir_adjusted(dirp);
      EXPECT_TRUE(strcmp(entry->d_name, shell_name) == 0);
    }

    EXPECT_EQ(0, tebako_closedir(dirp));
  }
}
#endif

#ifdef TEBAKO_HAS_SCANDIR
TEST_F(DirIOTests, tebako_scandir)
{
  const off_t start_fnum = 10; /* The first filename is 'file-10.txt' */
  const size_t size_dir = 90;
  struct dirent** namelist;
  std::string fname;
  int n = tebako_scandir(TEBAKIZE_PATH("directory-with-90-files"), &namelist, NULL, alphasort);
  EXPECT_EQ(n, size_dir + 2);
  EXPECT_TRUE(namelist != NULL);
  if (n > 0 && namelist != NULL) {
    for (int i = 0; i < n; i++) {
      switch (i) {
        case 0:
          fname = ".";
          break;
        case 1:
          fname = "..";
          break;
        default:
          fname = "file-";
          fname += std::to_string(i + start_fnum - 2 /* for '.' and '..' */);
          fname += ".txt";
          break;
      }
      EXPECT_TRUE(namelist[i] != NULL);
      if (namelist[i]) {
        EXPECT_TRUE(fname == namelist[i]->d_name);
        free(namelist[i]);
      }
    }
    free(namelist);
  }
}

extern "C" int zero_filter(const struct dirent*)
{
  return 0;
}

TEST_F(DirIOTests, tebako_scandir_filter_empty)
{
  struct dirent** namelist;
  std::string fname;
  int n = tebako_scandir(TEBAKIZE_PATH("directory-with-90-files"), &namelist, zero_filter, NULL);
  EXPECT_EQ(0, n);
  EXPECT_TRUE(namelist != NULL);
  if (namelist != NULL) {
    free(namelist);
  }
}

extern "C" int bash_filter(const struct dirent* entry)
{
  return (strcmp(entry->d_name, "bash") == 0);
}

TEST_F(DirIOTests, tebako_scandir_filter_pass_through)
{
  struct dirent** namelist;
  std::string fname;
  int n = tebako_scandir("/bin", &namelist, bash_filter, NULL);
  EXPECT_EQ(1, n);
  EXPECT_TRUE(namelist != NULL);
  if (namelist != NULL) {
    EXPECT_TRUE(strcmp(namelist[0]->d_name, "bash") == 0);
    free(namelist[0]);
    free(namelist);
  }
}
#endif

TEST_F(DirIOTests, tebako_dir_io_null_ptr)
{
  errno = 0;
  EXPECT_EQ(NULL, tebako_opendir(NULL));
  EXPECT_EQ(ENOENT, errno);

  errno = 0;
  EXPECT_EQ(-1, tebako_telldir(NULL));
  EXPECT_EQ(EBADF, errno);

  tebako_seekdir(NULL, 1);  // Just nothing. No error, no SEGFAULT

#ifdef TEBAKKO_HAS_DIRFD
  errno = 0;
  EXPECT_EQ(-1, tebako_dirfd(NULL));
  EXPECT_EQ(EBADF, errno);
#endif
  errno = 0;
  EXPECT_EQ(-1, tebako_closedir(NULL));
  EXPECT_EQ(EBADF, errno);

  errno = 0;
  EXPECT_EQ(NULL, tebako_readdir_adjusted(NULL));
  EXPECT_EQ(EBADF, errno);

#ifdef TEBAKO_HAS_SCANDIR
  struct dirent** namelist;

  errno = 0;
  EXPECT_EQ(-1, tebako_scandir(NULL, &namelist, zero_filter, alphasort));
  EXPECT_EQ(ENOENT, errno);

  errno = 0;
  EXPECT_EQ(-1, tebako_scandir(TEBAKIZE_PATH("directory-3"), NULL, zero_filter, alphasort));
  EXPECT_EQ(EFAULT, errno);
#endif
}

TEST_F(DirIOTests, tebako_opendir_readdir_closedir_dot_dot)
{
  DIR* dirp = tebako_opendir(TEBAKIZE_PATH("directory-3/level-1/.//level-2/level-3/.."));
  EXPECT_TRUE(dirp != NULL);
  if (dirp != NULL) {
    std::string fname;
    std::string fname_alt;
    pdirent entry = tebako_readdir_adjusted(dirp);
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      fname = ".";
      EXPECT_TRUE(fname == entry->d_name);
      EXPECT_TRUE(entry->d_type == DT_DIR);
    }
    entry = tebako_readdir_adjusted(dirp);
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      fname = "..";
      EXPECT_TRUE(fname == entry->d_name);
      EXPECT_TRUE(entry->d_type == DT_DIR);
    }
    fname = "test-file-at-level-2.txt";
    fname_alt = "level-3";
    entry = tebako_readdir_adjusted(dirp);
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      EXPECT_TRUE((fname == entry->d_name) || (fname_alt == entry->d_name));
      EXPECT_TRUE(entry->d_type == (entry->d_name == fname_alt ? DT_DIR : DT_REG));
    }
    entry = tebako_readdir_adjusted(dirp);
    EXPECT_TRUE(entry != NULL);
    if (entry != NULL) {
      EXPECT_TRUE((fname == entry->d_name) || (fname_alt == entry->d_name));
      EXPECT_TRUE(entry->d_type == (entry->d_name == fname_alt ? DT_DIR : DT_REG));
    }
    EXPECT_EQ(0, tebako_closedir(dirp));
  }
}
#endif
}  // namespace
