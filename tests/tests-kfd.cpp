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
#include <tebako/fs/internal/kfd.h>

namespace tebako {

class KfdTableTests : public ::testing::Test {
 protected:
  sync_tebako_kfdtable& kfd_table = sync_tebako_kfdtable::get_tebako_kfdtable();

  void SetUp() override { kfd_table.clear(); }

  void TearDown() override { kfd_table.clear(); }
};

TEST_F(KfdTableTests, check_key_exists)
{
  uintptr_t key = 12345;
  kfd_table.insert(key);

  EXPECT_TRUE(kfd_table.check(key));
}

TEST_F(KfdTableTests, check_key_does_not_exist)
{
  uintptr_t key = 12345;

  EXPECT_FALSE(kfd_table.check(key));
}

TEST_F(KfdTableTests, erase_key)
{
  uintptr_t key = 12345;
  kfd_table.insert(key);
  kfd_table.erase(key);

  EXPECT_FALSE(kfd_table.check(key));
}

TEST_F(KfdTableTests, clear_table)
{
  uintptr_t key1 = 12345;
  uintptr_t key2 = 67890;
  kfd_table.insert(key1);
  kfd_table.insert(key2);
  kfd_table.clear();

  EXPECT_FALSE(kfd_table.check(key1));
  EXPECT_FALSE(kfd_table.check(key2));
}

TEST_F(KfdTableTests, concurrent_insert_and_check)
{
  const int num_threads = 10;
  const int num_operations = 100;
  std::vector<std::thread> threads;

  auto insert_task = [this](int id) {
    for (int i = 0; i < num_operations; ++i) {
      uintptr_t key = id * 1000 + i;
      kfd_table.insert(key);
      kfd_table.check(key);
    }
  };

  // Start insert threads
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(insert_task, i);
  }

  // Join all threads
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify that all keys were inserted
  for (int i = 0; i < num_threads; ++i) {
    for (int j = 0; j < num_operations; ++j) {
      uintptr_t key = i * 1000 + j;
      EXPECT_TRUE(kfd_table.check(key));
    }
  }
}

TEST_F(KfdTableTests, concurrent_insert_and_erase)
{
  const int num_threads = 10;
  const int num_operations = 100;
  std::vector<std::thread> threads;

  auto insert_task = [this](int id) {
    for (int i = 0; i < num_operations; ++i) {
      uintptr_t key = id * 1000 + i;
      kfd_table.insert(key);
      kfd_table.erase(key);
    }
  };

  // Start insert threads
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(insert_task, i);
  }

  // Join all threads
  for (auto& thread : threads) {
    thread.join();
  }

  // Verify that all keys were erased
  for (int i = 0; i < num_threads; ++i) {
    for (int j = 0; j < num_operations; ++j) {
      uintptr_t key = i * 1000 + j;
      EXPECT_FALSE(kfd_table.check(key));
    }
  }
}

}  // namespace tebako
