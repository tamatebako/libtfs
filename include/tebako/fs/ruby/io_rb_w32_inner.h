/**
 *
 * Copyright (c) 2022-2025 [Ribose Inc](https://www.ribose.com).
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

#pragma once

#ifdef RB_W32
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
char* rb_w32_ugetcwd(char*, int);
int rb_w32_uchdir(const char*);
int rb_w32_umkdir(const char*, int);
int rb_w32_urmdir(const char*);
int rb_w32_uunlink(const char*);
int rb_w32_access(const char*, int);

int rb_w32_uopen(const char*, int, ...);
int rb_w32_close(int);
ssize_t rb_w32_read(int, void*, size_t);
ssize_t rb_w32_pread(int, void*, size_t, size_t);
off_t rb_w32_lseek(int, off_t, int);

int rb_w32_stati128(const char*, struct stati128*);
int rb_w32_lstati128(const char*, struct stati128*);
int rb_w32_fstati128(int, struct stati128*);

#if defined(RUBY_WIN32_DIR_H) || defined(RB_W32_DIR_DEFINED)
DIR* rb_w32_uopendir(const char*);
struct direct* rb_w32_readdir(DIR*, void*);
long rb_w32_telldir(DIR*);
void rb_w32_seekdir(DIR*, long);
void rb_w32_rewinddir(DIR*);
void rb_w32_closedir(DIR*);
#endif

int flock(int, int);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // RB_W32
