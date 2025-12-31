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

#pragma once

namespace tebako {

typedef std::pair<uint32_t, std::string> tebako_mount_point;
typedef std::variant<std::string, uint32_t> tebako_mount_target;
typedef std::map<tebako_mount_point, tebako_mount_target> tebako_mount_table;

class sync_tebako_mount_table {
 private:
  tebako::Synchronized<tebako_mount_table> s_tebako_mount_table;

 public:
  static sync_tebako_mount_table& get_tebako_mount_table(void);

  bool check(const tebako_mount_point& mount_point);
  bool check(const uint32_t ino, const std::string& mount_path) { return check(std::make_pair(ino, mount_path)); };

  void clear(void);

  void erase(const tebako_mount_point& mount_point);
  void erase(const uint32_t ino, const std::string& mount_path) { erase(std::make_pair(ino, mount_path)); };

  std::optional<tebako_mount_target> get(const tebako_mount_point& mount_point);
  std::optional<tebako_mount_target> get(const uint32_t ino, const std::string& mount_path)
  {
    return get(std::make_pair(ino, mount_path));
  };

  bool insert(const tebako_mount_point& mount_point, const std::string& mount_target);
  bool insert(const uint32_t ino, const std::string& mount_path, const std::string& mount_target)
  {
    return insert(std::make_pair(ino, mount_path), mount_target);
  };

  bool insert(const tebako_mount_point& mount_point, uint32_t mount_target);
  bool insert(const uint32_t ino, const std::string& mount_path, uint32_t mount_target)
  {
    return insert(std::make_pair(ino, mount_path), mount_target);
  };
};

}  // namespace tebako
