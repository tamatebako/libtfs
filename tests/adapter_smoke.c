/**
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project (libtfs).
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

/**
 * @file adapter_smoke.c
 * @brief Minimal C consumer of the shared libtfs (adapter smoke test)
 *
 * Mounts a ZIP image via tebako_fs_init_from_file() and reads a file,
 * using only the public C ABI. Compiled with pkg-config flags and run by
 * cmake/run_adapter_smoke.cmake as the adapter_smoke_c ctest case; this
 * program is also the C adapter template referenced by docs/EMBEDDING.md.
 */

#include <stdio.h>
#include <fcntl.h>

#include <tebako/fs/c_api.h>

int main(int argc, char** argv)
{
  if (argc < 2) {
    fprintf(stderr, "usage: %s <image.zip>\n", argv[0]);
    return 2;
  }

  /* Adapters feature-detect the ABI at load time */
  if (tebako_fs_abi_version() < TEBAKO_FS_ABI_VERSION) {
    fprintf(stderr, "libtfs ABI too old: %d < %d\n", tebako_fs_abi_version(), TEBAKO_FS_ABI_VERSION);
    return 3;
  }

  if (tebako_fs_init_from_file(argv[1], "/__smoke__") != 0) {
    fprintf(stderr, "mount failed: %s\n", tebako_strerror(tebako_get_errno()));
    return 4;
  }

  int fd = tebako_fs_open("/__smoke__/test.txt", O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "open failed: %s\n", tebako_strerror(tebako_get_errno()));
    return 5;
  }

  char buf[64];
  ssize_t n = tebako_fs_read(fd, buf, sizeof(buf) - 1);
  if (n < 0) {
    fprintf(stderr, "read failed: %s\n", tebako_strerror(tebako_get_errno()));
    return 6;
  }
  buf[n] = '\0';

  tebako_fs_close(fd);
  tebako_fs_unmount();

  printf("%s", buf);
  return 0;
}
