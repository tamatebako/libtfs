/**
 *
 * Copyright (c) 2025 [Ribose Inc](https://www.ribose.com).
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

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

// Define file_off_t as off_t for file offset operations
#include <sys/types.h>
typedef off_t file_off_t;

namespace tebako {
namespace util {

/**
 * Generic string_to template
 * Converts strings to various types.
 *
 * This is a replacement for folly::to<T>() that uses only
 * standard C library functions.
 */
template <typename T>
T string_to(const char* str);

/**
 * Specialization for double
 * Converts string to double using strtod
 */
template <>
inline double string_to<double>(const char* str)
{
  if (!str || *str == '\0') {
    throw std::invalid_argument("Cannot convert empty string to double");
  }

  char* end;
  errno = 0;
  double result = std::strtod(str, &end);

  if (errno == ERANGE) {
    throw std::out_of_range(std::string("Value out of range: ") + str);
  }
  if (end == str || *end != '\0') {
    throw std::invalid_argument(std::string("Cannot convert '") + str + "' to double");
  }

  return result;
}

/**
 * Specialization for size_t
 * Converts string to size_t using strtoull
 */
template <>
inline size_t string_to<size_t>(const char* str)
{
  if (!str || *str == '\0') {
    throw std::invalid_argument("Cannot convert empty string to size_t");
  }

  char* end;
  errno = 0;
  unsigned long long result = std::strtoull(str, &end, 10);

  if (errno == ERANGE) {
    throw std::out_of_range(std::string("Value out of range: ") + str);
  }
  if (end == str || *end != '\0') {
    throw std::invalid_argument(std::string("Cannot convert '") + str + "' to size_t");
  }

  return static_cast<size_t>(result);
}

/**
 * Specialization for file_off_t (usually int64_t or long long)
 * Converts string to file_off_t using strtoll
 */
template <>
inline file_off_t string_to<file_off_t>(const char* str)
{
  if (!str || *str == '\0') {
    throw std::invalid_argument("Cannot convert empty string to file_off_t");
  }

  char* end;
  errno = 0;
  long long result = std::strtoll(str, &end, 10);

  if (errno == ERANGE) {
    throw std::out_of_range(std::string("Value out of range: ") + str);
  }
  if (end == str || *end != '\0') {
    throw std::invalid_argument(std::string("Cannot convert '") + str + "' to file_off_t");
  }

  return static_cast<file_off_t>(result);
}

/**
 * String overload for convenience
 * Allows passing std::string directly
 */
template <typename T>
inline T string_to(const std::string& str)
{
  return string_to<T>(str.c_str());
}

}  // namespace util
}  // namespace tebako