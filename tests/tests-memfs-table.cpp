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

#include "tests.h"

struct tebako_dirent;

#include <tebako-io-inner.h>

#ifdef _WIN32
#undef lseek
#undef close
#undef read
#undef pread

#undef chdir
#undef mkdir
#undef rmdir
#undef unlink
#undef access
#undef fstat
#undef stat
#undef lstat
#undef getcwd
#undef opendir
#undef readdir
#undef telldir
#undef seekdir
#undef rewinddir
#undef closedir
#endif

#include <tebako/fs/memfs.h>
#include <tebako/fs/internal/memfs_table.h>

namespace tebako {

class MemfsTableTests : public ::testing::Test {
 protected:
  sync_tebako_memfs_table& memfs_table = sync_tebako_memfs_table::get_tebako_memfs_table();

  void SetUp() override { memfs_table.clear(); }

  void TearDown() override { memfs_table.clear(); }
};

TEST_F(MemfsTableTests, test_singleton_retrieval)
{
  auto& instance1 = tebako::sync_tebako_memfs_table::get_tebako_memfs_table();
  auto& instance2 = tebako::sync_tebako_memfs_table::get_tebako_memfs_table();
  EXPECT_EQ(&instance1, &instance2);  // Both should be the same instance
}

TEST_F(MemfsTableTests, test_clear)
{
  auto& memfs_table = tebako::sync_tebako_memfs_table::get_tebako_memfs_table();
  memfs_table.insert(1, std::make_shared<memfs>("Test1", 5));
  memfs_table.insert(2, std::make_shared<memfs>("Test2", 5));

  EXPECT_TRUE(memfs_table.check(1));
  EXPECT_TRUE(memfs_table.check(2));

  memfs_table.clear();  // Clear the table
  EXPECT_FALSE(memfs_table.check(1));
  EXPECT_FALSE(memfs_table.check(2));
}

TEST_F(MemfsTableTests, test_erase)
{
  auto& memfs_table = tebako::sync_tebako_memfs_table::get_tebako_memfs_table();
  memfs_table.insert(1, std::make_shared<memfs>("TestA", 5));

  EXPECT_TRUE(memfs_table.check(1));

  memfs_table.erase(1);  // Erase the entry
  EXPECT_FALSE(memfs_table.check(1));
}

TEST_F(MemfsTableTests, test_get)
{
  auto& memfs_table = tebako::sync_tebako_memfs_table::get_tebako_memfs_table();
  auto fs = std::make_shared<memfs>("Test3", 5);
  memfs_table.insert(1, fs);

  EXPECT_EQ(memfs_table.get(1), fs);       // Should return the same pointer
  EXPECT_EQ(memfs_table.get(2), nullptr);  // Non-existing index should return nullptr
}

TEST_F(MemfsTableTests, test_insert_auto)
{
  auto& memfs_table = tebako::sync_tebako_memfs_table::get_tebako_memfs_table();
  memfs_table.clear();  // Ensure the table is empty

  auto fs01 = std::make_shared<memfs>("Test01", 6);
  auto fs02 = std::make_shared<memfs>("Test02", 6);
  auto fs03 = std::make_shared<memfs>("Test03", 6);
  EXPECT_EQ(memfs_table.insert_auto(fs01), 1);  // Should insert at 1
  EXPECT_EQ(memfs_table.insert_auto(fs02), 2);  // Should insert at 2
  EXPECT_EQ(memfs_table.insert_auto(fs03), 3);  // Should insert at 3

  // Fill up the remaining slots
  memfs_table.insert_auto(std::make_shared<memfs>("Test04", 6));  // 4
  memfs_table.insert_auto(std::make_shared<memfs>("Test05", 6));  // 5
  memfs_table.insert_auto(std::make_shared<memfs>("Test06", 6));  // 6
  memfs_table.insert_auto(std::make_shared<memfs>("Test07", 6));  // 7

  EXPECT_EQ(memfs_table.insert_auto(std::make_shared<memfs>("Test08", 6)), 0);  // No more valid indices
}

TEST_F(MemfsTableTests, test_concurrent_access)
{
  auto& memfs_table = tebako::sync_tebako_memfs_table::get_tebako_memfs_table();
  memfs_table.clear();

  std::thread t1([&]() { memfs_table.insert(1, std::make_shared<memfs>("Test4", 5)); });
  std::thread t2([&]() { memfs_table.insert(2, std::make_shared<memfs>("Test5", 5)); });

  t1.join();
  t2.join();

  EXPECT_TRUE(memfs_table.check(1));
  EXPECT_TRUE(memfs_table.check(2));
}

TEST_F(MemfsTableTests, fs_ino_and_index)
{
  _ino_t t = sync_tebako_memfs_table::fsInoFromFsAndIno(0x1, 0x234);
  EXPECT_EQ(sync_tebako_memfs_table::getFsIndex(t), 0x1);
  EXPECT_EQ(sync_tebako_memfs_table::getFsIno(t), 0x234);
}

}  // namespace tebako
