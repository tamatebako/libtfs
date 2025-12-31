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
#include <stdlib.h>
#include <stdio.h>

// Cannot include tebako-common.h because it is C++ header
#ifdef PATH_MAX
#define TEBAKO_PATH_LENGTH ((size_t)PATH_MAX)
#else
#define TEBAKO_PATH_LENGTH ((size_t)2048)
#endif

#include <tebako-defines.h>
#include "tests-defines.h"

#include <tebako/fs/io.h>

#include "tebako-fs.h"
#include "tebako-test-config.h"

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#if defined(TEBAKO_HAS_FDOPENDIR) && defined(TEBAKO_HAS_DIRFD)
#define WITH_DIRIO_FD_C_TEST 1
#endif

#if defined(WITH_LINK_TESTS) && defined(TEBAKO_HAS_LSTAT)
#define WITH_LINK_C_TESTS 1
#endif

#if defined(TEBAKO_HAS_SCANDIR)
#define WITH_SCANDIR_C_TEST 1
#endif

static int attr_functions_c_test(char* fname);

#if defined(TEBAKO_HAS_PREAD) || defined(RB_W32)
static int pread_c_test(int fh);
#endif

#ifdef TEBAKO_HAS_READV
static int readv_c_test(int fh);
#endif

static int lseek_read_c_test(int fh);
static int open_3_args_c_test(void);

#ifdef TEBAKO_HAS_OPENAT
static int openat_c_test(int fh);
#endif

#if (defined(TEBAKO_HAS_OPENDIR) && defined(TEBAKO_HAS_CLOSEDIR) && defined(TEBAKO_HAS_READDIR) && \
     defined(TEBAKO_HAS_SEEKDIR) && defined(TEBAKO_HAS_TELLDIR)) ||                                \
    defined(RB_W32)
static int dirio_c_test(void);
#endif

#if defined(WITH_DIRIO_FD_C_TEST)
static int dirio_fd_c_test(void);
#endif

#if defined(WITH_SCANDIR_C_TEST)
static int scandir_c_test(void);
#endif

static int dlopen_c_test(void);

#ifdef WITH_LINK_C_TESTS
static int link_c_tests(void);
#endif

static int fstatat_c_test(void);

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

int main(int argc, char** argv)
{
  char p[TEBAKO_PATH_LENGTH];
  char* r;
  int rOK = false;
  int fh;
  struct STAT_TYPE buf;

#ifdef _WIN32
  _set_invalid_parameter_handler(invalidParameterHandler);
#endif

  int ret = mount_root_memfs(&gfsData[0], gfsSize, tests_log_level(), NULL /* cachesize*/, NULL /* workers */,
                             NULL /* mlock */, NULL /* decompress_ratio*/, NULL /* image_offset */
  );

  printf("A call to mount_root_memfs returned %i (0 expected)\n", ret);

  if (ret == 0) {
    rOK = true;

    rOK &= attr_functions_c_test(TEBAKIZE_PATH("file.txt"));
    if (!rOK)
      printf("failing\n");

    ret = chdir(TEBAKIZE_PATH("directory-1"));
    printf("A call to 'chdir' returned %i (0 expected)\n", ret);
    rOK &= (ret == 0);
    if (!rOK)
      printf("failing\n");

    ret = is_tebako_cwd();
    printf("A call to 'is_tebako_cwd' returned %i (not 0 expected)\n", ret);
    rOK &= (ret != 0);
    if (!rOK)
      printf("failing\n");

    r = getcwd(NULL, 0);
    printf("A call to 'getcwd' returned %s ('%s' expected)\n", r, TEBAKIZE_PATH("directory-1"));
    rOK &= (strstr(r, "directory-1") != NULL);
    free(r);
    if (!rOK)
      printf("failing\n");

#if defined(_WIN32) && !defined(RB_W32)
    ret = mkdir(TEBAKIZE_PATH("directory-100"));
#else
    ret = mkdir(TEBAKIZE_PATH("directory-100"), S_IRWXU);
#endif

    printf("A call to 'mkdir' returned %i (-1 expected)\n", ret);
    rOK &= (ret == -1);
    if (!rOK)
      printf("failing\n");

    fh = open(TEBAKIZE_PATH("file.txt"), O_RDONLY);
    printf("A call to 'open' returned %i (non negative file handle expected)\n", fh);
    rOK &= (fh >= 0);
    if (!rOK)
      printf("failing\n");

    rOK &= lseek_read_c_test(fh);
    if (!rOK)
      printf("failing\n");

#ifdef TEBAKO_HAS_READV
    rOK &= readv_c_test(fh); /* Skipped 'Ju', read 'st', ' a file' remains */
    if (!rOK)
      printf("failing\n");
#endif

#if defined(TEBAKO_HAS_PREAD) || defined(RB_W32)
    rOK &= pread_c_test(fh);
    if (!rOK)
      printf("failing\n");
#endif

#ifdef TEBAKO_HAS_OPENAT
    rOK &= openat_c_test(fh);
    if (!rOK)
      printf("failing\n");
#endif
    ret = fstat(fh, &buf);
    printf("A call to 'fstat' returned %i (0 expected)\n", ret);
    rOK &= (ret == 0);
    if (!rOK)
      printf("failing\n");

#if defined(TEBAKO_HAS_FLOCK) || defined(RB_W32)
    ret = flock(fh, LOCK_SH);
    printf("A call to 'flock' returned %i (0 expected)\n", ret);
    rOK &= (ret == 0);
    if (!rOK)
      printf("failing\n");
#endif

    ret = close(fh);
    printf("A call to 'close' returned %i (0 expected)\n", ret);
    rOK &= (ret == 0);
    if (!rOK)
      printf("failing\n");

    rOK &= open_3_args_c_test();
    if (!rOK)
      printf("failing\n");

#if defined(TEBAKO_HAS_OPENDIR) && defined(TEBAKO_HAS_CLOSEDIR) && defined(TEBAKO_HAS_READDIR) && \
    defined(TEBAKO_HAS_SEEKDIR) && defined(TEBAKO_HAS_TELLDIR)
    rOK &= dirio_c_test();
    if (!rOK)
      printf("failing\n");
#endif

#if defined(WITH_DIRIO_FD_C_TEST)
    rOK &= dirio_fd_c_test();
    if (!rOK)
      printf("failing\n");
#endif

#if defined(WITH_SCANDIR_C_TEST)
    rOK &= scandir_c_test();
    if (!rOK)
      printf("failing\n");
#endif
    rOK &= dlopen_c_test();
    if (!rOK)
      printf("failing\n");

#if defined(WITH_LINK_C_TESTS)
    rOK &= link_c_tests();
    if (!rOK)
      printf("failing\n");
#endif

#ifdef TEBAKO_HAS_FSTATAT
    rOK &= fstatat_c_test();
    if (!rOK)
      printf("failing\n");
#endif

    unmount_root_memfs();
    printf("Filesystem dropped\n");

    ret = rOK ? 0 : -1;
  }

  printf("Exiting. Return code: %i\n", ret);
  return ret;
}

static int attr_functions_c_test(char* fname)
{
  struct STAT_TYPE buf;
  int rOK = true;
  int ret = stat(fname, &buf);
  printf("A call to 'stat' returned %i (0 expected)\n", ret);
  rOK &= (ret == 0);

  ret = access(fname, F_OK);
  printf("A call to 'access' returned %i (0 expected)\n", ret);
  rOK &= (ret == 0);

  return rOK;
}

#ifdef TEBAKO_HAS_OPENAT
static int openat_c_test(int fh)
{
  int rOK = true;
  int fh2 = openat(fh, TEBAKIZE_PATH("file2.txt"), O_RDONLY);
  printf(
      "A call to 'openat' returned %i (non negative file handle expected "
      "expected)\n",
      fh2);

  int ret = close(fh2);
  printf("A call to 'close' returned %i (0 expected)\n", ret);
  rOK = (fh2 > 0 && ret == 0);
  return rOK;
}
#endif

static int lseek_read_c_test(int fh)
{
  int ret;
  char readbuf[32];
  int rOK = true;
  ret = lseek(fh, 2, SEEK_SET);
  printf("A call to 'lseek' returned %i (2 expected)\n", ret);
  rOK &= (ret == 2);

  ret = read(fh, readbuf, 2);
  readbuf[2] = '\0';
  printf(
      "A call to 'read' returned %i (2 expected); Read buffer: '%s' ('st' "
      "expected)\n",
      ret, readbuf);
  rOK &= (ret == 2);
  rOK &= (strcmp(readbuf, "st") == 0);
  return rOK;
}

#if defined(TEBAKO_HAS_PREAD) || defined(RB_W32)
static int pread_c_test(int fh)
{
  int rOK = true;
  int ret;
  char readbuf[32];
  ret = pread(fh, readbuf, 4, 7);
  readbuf[4] = '\0';
  printf(
      "A call to 'pread' returned %i (4 expected); Read buffer: '%s' ('file' "
      "expected)\n",
      ret, readbuf);
  rOK &= (ret == 4);
  rOK &= (strcmp(readbuf, "file") == 0);
  return rOK;
}
#endif

#ifdef TEBAKO_HAS_READV
static int readv_c_test(int fh)
{
  int rOK = true;
  ssize_t s;
  const int buflen = 5;
  char buf0[buflen];
  char buf1[buflen];
  char buf2[buflen];
  int iovcnt;
  struct iovec iov[3];
  char* pattern = " a file";
  int l = strlen(pattern);

  iov[0].iov_base = buf0;
  iov[0].iov_len = buflen;
  iov[1].iov_base = buf1;
  iov[1].iov_len = buflen;
  iov[2].iov_base = buf2;
  iov[2].iov_len = buflen;
  iovcnt = sizeof(iov) / sizeof(struct iovec);

  s = readv(fh, &iov[0], iovcnt);
  printf("A call to 'readv' returned %li (7 expected)\n", s); /* Skipped 'Ju', read 'st', ' a file' remains */
  rOK &= (s == 7);

  printf("buf0 = '%.*s'(' a fi' expected); buf1 = '%.*s'('le' expected)\n", buflen, buf0, l - buflen, buf1);
  rOK &= (strncmp(buf0, pattern, buflen) == 0);
  rOK &= (strncmp(buf1, pattern + buflen, l - buflen) == 0);
  return rOK;
}
#endif

static int open_3_args_c_test(void)
{
  int rOK = true;
  int ret = unlink(__AT_TMP__("some-tebako-test-file.txt"));
  printf("A call to 'unlink' returned %i (-1 supposed but 0 is also possible)\n", ret);
  // ret is not checked intensionally !!!

  ret = open(__AT_TMP__("some-tebako-test-file.txt"), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  printf(
      "A call to 'open' with 3 arguments returned %i (non negative file handle "
      "expected)\n",
      ret);
  rOK &= (ret >= 0);

  if (rOK) {
    ret = close(ret);
    printf("A call to 'close' returned %i (0 expected)\n", ret);
    rOK &= (ret == 0);

    ret = unlink(__AT_TMP__("some-tebako-test-file.txt"));
    printf("A call to 'unlink' returned %i (0 expected)\n", ret);
    rOK &= (ret == 0);
  }
  return rOK;
}

#if defined(TEBAKO_HAS_OPENDIR) && defined(TEBAKO_HAS_CLOSEDIR) && defined(TEBAKO_HAS_READDIR) && \
    defined(TEBAKO_HAS_SEEKDIR) && defined(TEBAKO_HAS_TELLDIR)
static int dirio_c_test(void)
{
  int rOK = true;
  DIR* dirp = opendir(TEBAKIZE_PATH("directory-2"));
  printf("A call to 'opendir' returned %p (not NULL expected)\n", dirp);
  rOK &= (dirp != NULL);

  long pos = telldir(dirp);
  printf("A call to 'telldir' returned %li (0 expected)\n", pos);
  rOK &= (pos == 0L);

  seekdir(dirp, 2);
  pos = telldir(dirp);
  printf(
      "A call to 'telldir' after 'seekdir(dirp, 2)' returned %li (2 "
      "expected)\n",
      pos);
  rOK &= (pos == 2L);

#if defined(_WIN32)
  struct direct* entry = readdir(dirp);
#else
  struct dirent* entry = readdir(dirp);
#endif
  printf("A call to 'readdir'  returned %p (not NULL expected)\n", entry);
  rOK &= (entry != NULL);
  if (entry != NULL) {
    printf("Filename: %s ('file-in-directory-2.txt' expected)\n", entry->d_name);
    rOK &= (strcmp(entry->d_name, "file-in-directory-2.txt") == 0);
  }

  rewinddir(dirp);
  pos = telldir(dirp);
  printf("A call to 'telldir' after 'rewinddir(dirp)' returned %li (0 expected)\n", pos);
  rOK &= (pos == 0L);

  int ret = closedir(dirp);
  printf("A call to 'closedir' returned %i (0 expected)\n", ret);
  rOK &= (ret == 0);

  return rOK;
}
#endif

#if defined(WITH_DIRIO_FD_C_TEST)
static int dirio_fd_c_test(void)
{
  int rOK = true;
  int fh = open(TEBAKIZE_PATH("directory-1"), O_RDONLY | O_DIRECTORY);
  printf("A call to 'open' returned %i (non negative file handle expected)\n", fh);
  rOK &= (fh >= 0);

  DIR* dirp = fdopendir(fh);
  printf("A call to 'opendir' returned %p (not NULL expected)\n", dirp);
  rOK &= (dirp != NULL);

  int fh2 = dirfd(dirp);
  printf("A call to 'dirfd' returned %i (%i expected)\n", fh, fh2);
  rOK &= (fh == fh2);

  int ret = closedir(dirp);
  printf("A call to 'closedir' returned %i (0 expected)\n", ret);
  rOK &= (ret == 0);
  return rOK;
}
#endif

#if defined(WITH_SCANDIR_C_TEST)
static int scandir_c_test(void)
{
  int rOK = true;
  struct dirent** namelist;
  int n, i;

#ifdef WITH_LINK_TESTS
  const int N = 7;
#else
  const int N = 6;
#endif
  n = scandir(TEBAKIZE_PATH("directory-1"), &namelist, NULL, alphasort);
  printf("A call to 'scandir' returned %i (%i expected)\n", n, N);
  rOK &= (n == N);
  if (n > 0) {
    for (i = 0; i < n; i++) {
      printf("Scandir file name #%i: '%s'\n", i, namelist[i]->d_name);
      free(namelist[i]);
    }
    free(namelist);
  }
  return rOK;
}
#endif

static int dlopen_c_test(void)
{
  int rOK = true;
  void* handle = dlopen(TEBAKIZE_PATH("directory-1/" __LIBEMPTY__), RTLD_LAZY | RTLD_GLOBAL);
  rOK &= (handle != NULL);
  printf("A call to 'dlopen' returned %p (not NULL expected)\n", handle);
  if (handle != NULL) {
    int ret = dlclose(handle);
    printf("A call to 'dlclose' returned %i (0 expected)\n", ret);
    rOK &= (ret == 0);
  }
  return rOK;
}

#ifdef WITH_LINK_C_TESTS
static int link_c_tests(void)
{
  int rOK = true;
  struct STAT_TYPE st;
  char buf[256];
  int ret = lstat(TEBAKIZE_PATH("s-link-to-file-1"), &st);
  printf("A call to 'lstat' returned %i (0 expected)\n", ret);
  rOK &= (ret == 0);

  ret = readlink(TEBAKIZE_PATH("s-link-to-file-1"), buf, sizeof(buf) / sizeof(buf[0]));
  printf("A call to 'readlink' returned %i (35 expected)\n", ret);
  rOK &= (ret == 35);
  return rOK;
}
#endif

#ifdef TEBAKO_HAS_FSTATAT
static int fstatat_c_test(void)
{
  int rOK = true;
  struct STAT_TYPE buf;
  int ret = chdir(TEBAKIZE_PATH("directory-2"));
  rOK &= (ret == 0);
  ret = fstatat(AT_FDCWD, "file-in-directory-2.txt", &buf, 0);
  rOK &= (ret == 0);
  return rOK;
}
#endif
