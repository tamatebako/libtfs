/**
 * @file platform.h
 * @brief Platform compatibility shims for the libtfs public headers
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#pragma once

#ifdef _WIN32
#include <BaseTsd.h>
// MSVC's <sys/types.h> does not provide POSIX ssize_t; map it to SSIZE_T the
// same way dwarfs' internal folly_compat.h does for its Windows builds.
typedef SSIZE_T ssize_t;

// MinGW/MSVC <sys/stat.h> has no symlink support (no S_ISLNK/_S_IFLNK) and
// does not define the POSIX group/other permission-bit macros. Zero/no-op
// fallbacks keep the modern layer portable; the CRT never sets these bits.
#ifndef S_ISLNK
#define S_ISLNK(mode) 0
#endif
#ifndef S_IRGRP
#define S_IRGRP 0
#endif
#ifndef S_IWGRP
#define S_IWGRP 0
#endif
#ifndef S_IXGRP
#define S_IXGRP 0
#endif
#ifndef S_IROTH
#define S_IROTH 0
#endif
#ifndef S_IWOTH
#define S_IWOTH 0
#endif
#ifndef S_IXOTH
#define S_IXOTH 0
#endif
#endif
