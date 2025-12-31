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
#include <tebako/fs/internal/mount_table.h>

namespace tebako {

class MountTableTests : public ::testing::Test {
 protected:
  sync_tebako_mount_table& mount_table = sync_tebako_mount_table::get_tebako_mount_table();

  void SetUp() override { mount_table.clear(); }

  void TearDown() override { mount_table.clear(); }
};

TEST_F(MountTableTests, check_path_exists)
{
  uint32_t ino = 1;
  std::string path = "/path1";
  std::string mount = "mount1";
  mount_table.insert(ino, path, mount);

  EXPECT_TRUE(mount_table.check(ino, path));
}

TEST_F(MountTableTests, check_path_exists_ino)
{
  uint32_t ino = 1;
  std::string path = "/path1";
  uint32_t ino_mount = 1;
  mount_table.insert(ino, path, ino_mount);

  EXPECT_TRUE(mount_table.check(ino, path));
}

TEST_F(MountTableTests, check_path_does_not_exist)
{
  uint32_t ino = 2;
  std::string path = "/path2";

  EXPECT_FALSE(mount_table.check(ino, path));
}

TEST_F(MountTableTests, erase_path)
{
  uint32_t ino = 3;
  std::string path = "/path3";
  std::string mount = "mount3";
  mount_table.insert(ino, path, mount);
  mount_table.erase(ino, path);

  EXPECT_FALSE(mount_table.check(ino, path));
}

TEST_F(MountTableTests, get_existing_path)
{
  uint32_t ino = 4;
  std::string path = "/path4";
  std::string mount = "mount4";
  mount_table.insert(ino, path, mount);

  auto result = mount_table.get(ino, path);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<std::string>(result.value()), mount);
}

TEST_F(MountTableTests, get_existing_ino)
{
  uint32_t ino = 4;
  std::string path = "/path4";
  uint32_t ino_mount = 2;
  mount_table.insert(ino, path, ino_mount);

  auto result = mount_table.get(ino, path);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<uint32_t>(result.value()), ino_mount);
}

TEST_F(MountTableTests, get_non_existing_path)
{
  uint32_t ino = 5;
  std::string path = "/path5";

  auto result = mount_table.get(ino, path);
  EXPECT_FALSE(result.has_value());
}

TEST_F(MountTableTests, insert_duplicate_path)
{
  uint32_t ino = 7;
  std::string path = "/path7";
  std::string mount1 = "mount7";
  std::string mount2 = "mount8";
  tebako_mount_point mount_point = std::make_pair(ino, path);

  EXPECT_TRUE(mount_table.insert(ino, path, mount1));
  EXPECT_FALSE(mount_table.insert(mount_point, mount2));  // Insertion should fail for duplicate path
  auto result = mount_table.get(ino, path);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<std::string>(result.value()), mount1);  // Original mount should remain
}

TEST_F(MountTableTests, insert_duplicate_path_ino)
{
  uint32_t ino = 7;
  std::string path = "/path7";
  std::string mount = "mount7";
  uint32_t ino_mount = 8;
  tebako_mount_point mount_point = std::make_pair(ino, path);

  EXPECT_TRUE(mount_table.insert(ino, path, mount));
  EXPECT_FALSE(mount_table.insert(mount_point, ino_mount));  // Insertion should fail for duplicate path
  auto result = mount_table.get(ino, path);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(std::get<std::string>(result.value()), mount);  // Original mount should remain
}

TEST_F(MountTableTests, concurrent_insert_and_check)
{
  uint32_t ino = 8;
  const int num_threads = 10;
  const int num_operations = 100;
  std::vector<std::thread> threads;

  auto insert_task = [this, ino](int id) {
    for (int i = 0; i < num_operations; ++i) {
      std::string path = "/path" + std::to_string(id) + "_" + std::to_string(i);
      std::string mount = "mount" + std::to_string(id) + "_" + std::to_string(i);
      tebako_mount_point mount_point = std::make_pair(ino, path);
      mount_table.insert(mount_point, mount);
    }
  };

  auto check_task = [this, ino](int id) {
    for (int i = 0; i < num_operations; ++i) {
      std::string path = "/path" + std::to_string(id) + "_" + std::to_string(i);
      tebako_mount_point mount_point = std::make_pair(ino, path);
      mount_table.check(mount_point);
    }
  };

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(insert_task, i);
    threads.emplace_back(check_task, i);
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // Verify that all paths were inserted
  for (int i = 0; i < num_threads; ++i) {
    for (int j = 0; j < num_operations; ++j) {
      std::string path = "/path" + std::to_string(i) + "_" + std::to_string(j);
      tebako_mount_point mount_point = std::make_pair(ino, path);
      EXPECT_TRUE(mount_table.check(mount_point));
    }
  }
}

TEST_F(MountTableTests, concurrent_insert_and_erase)
{
  uint32_t ino = 9;
  const int num_threads = 10;
  const int num_operations = 100;
  std::vector<std::thread> threads_i;
  std::vector<std::thread> threads_e;

  auto insert_task = [this](const int id, const int ino) {
    for (int i = 0; i < num_operations; ++i) {
      std::string path = "/path" + std::to_string(id) + "_" + std::to_string(i);
      std::string mount = "mount" + std::to_string(id) + "_" + std::to_string(i);
      tebako_mount_point mount_point = std::make_pair(ino, path);
      mount_table.insert(mount_point, mount);
    }
  };

  for (int i = 0; i < num_threads; ++i) {
    threads_i.emplace_back(insert_task, i, ino);
  }

  auto erase_task = [this, &threads_i](const int id, const int ino) {
    threads_i[id].join();
    for (int i = 0; i < num_operations; ++i) {
      std::string path = "/path" + std::to_string(id) + "_" + std::to_string(i);
      tebako_mount_point mount_point = std::make_pair(ino, path);
      mount_table.erase(mount_point);
    }
  };

  for (int i = 0; i < num_threads; ++i) {
    threads_e.emplace_back(erase_task, i, ino);
  }

  for (auto& thread : threads_e) {
    thread.join();
  }

  // Verify that all entries have been erased
  for (int i = 0; i < num_threads; ++i) {
    for (int j = 0; j < num_operations; ++j) {
      std::string path = "/path" + std::to_string(i) + "_" + std::to_string(j);
      tebako_mount_point mount_point = std::make_pair(ino, path);
      EXPECT_FALSE(mount_table.check(mount_point));
    }
  }
}
}  // namespace tebako
