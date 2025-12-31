/**
 *
 * Copyright (c) 2024, [Ribose Inc](https://www.ribose.com).
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
#include <tebako-pch-pp.h>
#include <tebako/fs/common.h>
#include <tebako/fs/dirent.h>
#include <tebako/fs/memfs.h>
#include <tebako/fs/internal/memfs_table.h>
#include <tebako/fs/internal/mount_table.h>
#include <tebako/fs/io.h>
#include <tebako-io-inner.h>

using namespace dwarfs;

namespace tebako {

sync_tebako_memfs_table& sync_tebako_memfs_table::get_tebako_memfs_table(void)
{
  static sync_tebako_memfs_table memfs_table{};
  return memfs_table;
}

bool sync_tebako_memfs_table::check(uint32_t index)
{
  auto p_memfs_table = s_tebako_memfs_table.rlock();
  auto p_memfs = p_memfs_table->find(index);
  return (p_memfs != p_memfs_table->end());
}

void sync_tebako_memfs_table::clear(void)
{
  auto p_memfs_table = s_tebako_memfs_table.wlock();
  p_memfs_table->clear();
}

void sync_tebako_memfs_table::erase(uint32_t index)
{
  auto p_memfs_table = s_tebako_memfs_table.wlock();
  auto p_memfs = p_memfs_table->extract(index);
}

std::shared_ptr<memfs> sync_tebako_memfs_table::get(uint32_t index)
{
  auto p_memfs_table = s_tebako_memfs_table.rlock();
  auto p_memfs = p_memfs_table->find(index);
  if (p_memfs != p_memfs_table->end()) {
    return p_memfs->second;
  }
  return nullptr;
}

bool sync_tebako_memfs_table::insert(uint32_t index, std::shared_ptr<memfs> fs)
{
  auto p_memfs_table = s_tebako_memfs_table.wlock();
  return p_memfs_table->emplace(index, fs).second;
}

uint32_t sync_tebako_memfs_table::insert_auto(std::shared_ptr<memfs> fs)
{
  uint32_t index = 1;
  auto p_memfs_table = s_tebako_memfs_table.wlock();

  for (const auto& pair : *p_memfs_table) {
    if (pair.first > index) {
      break;  // Stop as soon as we find the gap
    }
    else if (pair.first == index) {
      ++index;  // Move to the next expected index
    }
  }

  if (index > 7) {  // Only three bits to store memfs index
    return 0;
  }
  fs->set_root_inode(sync_tebako_memfs_table::fsInoFromFsAndIno(index, 0));
  p_memfs_table->emplace(index, fs);
  return index;
}

}  // namespace tebako
