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

#include <tebako-pch.h>
#include <tebako-pch-pp.h>

#include <tebako/fs/internal/kfd.h>

namespace tebako {

sync_tebako_kfdtable& sync_tebako_kfdtable::get_tebako_kfdtable(void)
{
  static sync_tebako_kfdtable kfd_table{};
  return kfd_table;
}

bool sync_tebako_kfdtable::check(uintptr_t dirp)
{
  auto p_kfdtable = s_tebako_kfdtable.rlock();
  auto p_kfd = p_kfdtable->find(dirp);
  return (p_kfd != p_kfdtable->end());
};

void sync_tebako_kfdtable::clear(void)
{
  auto p_kfdtable = s_tebako_kfdtable.wlock();
  p_kfdtable->clear();
}

void sync_tebako_kfdtable::erase(uintptr_t dirp)
{
  auto p_kfdtable = s_tebako_kfdtable.wlock();
  p_kfdtable->erase(dirp);
}

void sync_tebako_kfdtable::insert(uintptr_t dirp)
{
  auto p_kfdtable = s_tebako_kfdtable.wlock();
  p_kfdtable->insert(dirp);
}

};  // namespace tebako
