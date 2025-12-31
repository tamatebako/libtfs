/**
 *
 * Copyright (c) 2024 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project.
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
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <optional>
#include <string>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <vector>

namespace tebako {

class package_descriptor {
 private:
  uint16_t ruby_version_major;
  uint16_t ruby_version_minor;
  uint16_t ruby_version_patch;
  uint16_t tebako_version_major;
  uint16_t tebako_version_minor;
  uint16_t tebako_version_patch;
  std::string mount_point;
  std::string entry_point;
  std::optional<std::string> cwd;

 public:
  // Deleted default constructor
  package_descriptor() = delete;

  // Constructor for deserialization
  explicit package_descriptor(const std::vector<char>& buffer);
  // Constructor from version strings and other parameters
  package_descriptor(const std::string& ruby_version,
                     const std::string& tebako_version,
                     const std::string& mount_point,
                     const std::string& entry_point,
                     const std::optional<std::string>& cwd);
  // Serialize the object to a binary format
  std::vector<char> serialize() const;

  uint16_t get_ruby_version_major() const { return ruby_version_major; }
  uint16_t get_ruby_version_minor() const { return ruby_version_minor; }
  uint16_t get_ruby_version_patch() const { return ruby_version_patch; }
  uint16_t get_tebako_version_major() const { return tebako_version_major; }
  uint16_t get_tebako_version_minor() const { return tebako_version_minor; }
  uint16_t get_tebako_version_patch() const { return tebako_version_patch; }
  const std::string& get_mount_point() const { return mount_point; }
  const std::string& get_entry_point() const { return entry_point; }
  const std::optional<std::string>& get_cwd() const { return cwd; }

  static bool is_little_endian()
  {
    uint16_t number = 1;
    return *(reinterpret_cast<char*>(&number)) == 1;
  }

  static uint16_t to_little_endian(uint16_t value)
  {
    if (is_little_endian()) {
      return value;
    }
    else {
      return (value >> 8) | (value << 8);
    }
  }

  static uint16_t from_little_endian(uint16_t value)
  {
    if (is_little_endian()) {
      return value;
    }
    else {
      return (value >> 8) | (value << 8);
    }
  }

  static const char* signature;
};

}  // namespace tebako
