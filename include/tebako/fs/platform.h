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
#endif
