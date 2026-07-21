/**
 *
 * Copyright (c) 2021-2025 [Ribose Inc](https://www.ribose.com).
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

#include <tebako-config.h>

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// MSVC provides sys/stat.h and sys/types.h
// but does not set any precompiler variables
#if defined(_MSC_VER)
#define _SYS_TYPES_H 1
#define _SYS_STAT_H 1
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include <fcntl.h>
#include <stdarg.h>

// dlfcn-win32 in case of MSVC compiler
#include <dlfcn.h>
#include <errno.h>

#if defined(TEBAKO_HAS_GETATTRLIST) || defined(TEBAKO_HAS_FGETATTRLIST)
#include <sys/attr.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <direct.h>
#include <io.h>

#ifndef NDEBUG
#include <crtdbg.h>
#endif
#else
#include <dirent.h>
// With the legacy API enabled, include/tebako/fs is on the include path, so the
// <dirent.h> above resolves to the shadow include/tebako/fs/dirent.h (the
// tebako_dirent buffer abstraction) rather than the system header. Pull in the
// real system dirent.h as well (DIR, struct dirent, fdopendir, scandir, ...) —
// the legacy POSIX shims (dir-io.cpp) call it directly.
#include_next <dirent.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <ftw.h>
#endif

// CRITICAL: Create typedef using decltype to capture complete type
// This avoids forward declaration issues
#ifndef _WIN32
// Use decltype on a dereferenced pointer to capture the complete type
// The expression is never evaluated (unevaluated context), just its type is used
using tebako_system_dirent_t = decltype(*static_cast<struct dirent*>(static_cast<void*>(0)));
#endif
