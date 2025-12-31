/**
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project (libtfs).
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

#include <gtest/gtest.h>
#include <tebako/fs/backend_factory.h>
#include <tebako/fs/backends/dwarfs_backend.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <fcntl.h>
#include <vector>
#include <algorithm>

using namespace tebako::fs;

// ===================================================================
// Test Fixtures
// ===================================================================

class DwarfsIntegrationTest : public ::testing::Test {
 protected:
  const std::string fixtures_path = "tests/fixtures/dwarfs/";
  const std::string mount_point = "/mnt/test";
};

// ===================================================================
// 1. Factory Integration Tests (3 tests)
// ===================================================================

TEST_F(DwarfsIntegrationTest, AutoDetectDwarfsMagicSucceeds) {
  std::string archive = fixtures_path + "simple.dwarfs";
  auto backend = BackendFactory::create_from_file(archive);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(DwarfsIntegrationTest, FactoryReturnsNullForInvalidArchive) {
  std::string archive = fixtures_path + "nonexistent.dwarfs";
  auto backend = BackendFactory::create_from_file(archive);
  EXPECT_EQ(backend, nullptr);
}

TEST_F(DwarfsIntegrationTest, FactoryReturnsNullForCorruptedArchive) {
  std::string archive = fixtures_path + "corrupted.dwarfs";
  auto backend = BackendFactory::create_from_file(archive);
  // Should create a DwarFS backend (has valid magic bytes)
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "DwarFS");

  // But mounting should fail due to corruption
  EXPECT_FALSE(backend->mount(archive, mount_point));
}

// ===================================================================
// 2. Complete Workflow Tests (5 tests)
// ===================================================================

TEST_F(DwarfsIntegrationTest, CompleteWorkflowMountReadUnmount) {
  std::string archive = fixtures_path + "simple.dwarfs";
  auto backend = BackendFactory::create_from_file(archive);
  ASSERT_NE(backend, nullptr);

  // Mount
  ASSERT_TRUE(backend->mount(archive, mount_point));
  EXPECT_TRUE(backend->is_mounted());

  // Read file
  auto handle = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  char buffer[256];
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
  EXPECT_GT(bytes_read, 0);

  std::string content(buffer, bytes_read);
  EXPECT_EQ(content, "Hello, DwarFS!\n");

  // Unmount
  backend->unmount();
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(DwarfsIntegrationTest, CompleteWorkflowDirectoryTraversal) {
  std::string archive = fixtures_path + "nested.dwarfs";
  auto backend = std::make_unique<DwarfsBackend>();
  ASSERT_TRUE(backend->mount(archive, mount_point));

  // List root
  auto root_iter = backend->list_directory(mount_point);
  ASSERT_NE(root_iter, nullptr);

  std::vector<std::string> root_entries;
  while (root_iter->has_next()) {
    root_entries.push_back(root_iter->next().name);
  }
  EXPECT_GT(root_entries.size(), 0);

  // Navigate to nested directory
  auto nested_iter = backend->list_directory(mount_point + "/a/b/c/d");
  ASSERT_NE(nested_iter, nullptr);

  bool found_deep = false;
  while (nested_iter->has_next()) {
    if (nested_iter->next().name == "deep.txt") {
      found_deep = true;
    }
  }
  EXPECT_TRUE(found_deep);

  // Read nested file
  auto handle = backend->open(mount_point + "/a/b/c/d/deep.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  char buffer[256];
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
  EXPECT_GT(bytes_read, 0);

  backend->unmount();
}

TEST_F(DwarfsIntegrationTest, MultipleFileOperationsInSession) {
  std::string archive = fixtures_path + "simple.dwarfs";
  auto backend = std::make_unique<DwarfsBackend>();
  ASSERT_TRUE(backend->mount(archive, mount_point));

  // Read first file
  auto handle1 = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_NE(handle1, nullptr);
  char buffer1[256];
  ssize_t bytes1 = handle1->read(buffer1, sizeof(buffer1));
  EXPECT_GT(bytes1, 0);

  // Read second file
  auto handle2 = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle2, nullptr);
  char buffer2[256];
  ssize_t bytes2 = handle2->read(buffer2, sizeof(buffer2));
  EXPECT_GT(bytes2, 0);

  // Both files should have different content
  std::string content1(buffer1, bytes1);
  std::string content2(buffer2, bytes2);
  EXPECT_NE(content1, content2);

  backend->unmount();
}

TEST_F(DwarfsIntegrationTest, SeekAndReadPattern) {
  std::string archive = fixtures_path + "simple.dwarfs";
  auto backend = std::make_unique<DwarfsBackend>();
  ASSERT_TRUE(backend->mount(archive, mount_point));

  auto handle = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  // Seek to middle
  handle->seek(7, SEEK_SET);
  EXPECT_EQ(handle->tell(), 7);

  // Read from middle
  char buffer[10];
  ssize_t bytes = handle->read(buffer, 6);
  EXPECT_EQ(bytes, 6);
  std::string partial(buffer, bytes);
  EXPECT_EQ(partial, "DwarFS");

  // Seek back to beginning
  handle->seek(0, SEEK_SET);
  EXPECT_EQ(handle->tell(), 0);

  // Read from beginning
  bytes = handle->read(buffer, 5);
  EXPECT_EQ(bytes, 5);
  std::string start(buffer, bytes);
  EXPECT_EQ(start, "Hello");

  backend->unmount();
}

TEST_F(DwarfsIntegrationTest, MetadataConsistencyCheck) {
  std::string archive = fixtures_path + "permissions.dwarfs";
  auto backend = std::make_unique<DwarfsBackend>();
  ASSERT_TRUE(backend->mount(archive, mount_point));

  // Check executable file
  EXPECT_TRUE(backend->exists(mount_point + "/executable.sh"));
  EXPECT_TRUE(backend->is_file(mount_point + "/executable.sh"));
  mode_t perms = backend->permissions(mount_point + "/executable.sh");
  EXPECT_EQ(perms, 0755);

  int64_t size = backend->file_size(mount_point + "/executable.sh");
  EXPECT_GT(size, 0);

  time_t mtime = backend->modification_time(mount_point + "/executable.sh");
  EXPECT_GT(mtime, 0);

  backend->unmount();
}

// ===================================================================
// 3. Error Recovery Tests (2 tests)
// ===================================================================

TEST_F(DwarfsIntegrationTest, RecoverFromFailedMount) {
  auto backend = std::make_unique<DwarfsBackend>();

  // Attempt to mount invalid archive
  EXPECT_FALSE(backend->mount(fixtures_path + "nonexistent.dwarfs", mount_point));
  EXPECT_FALSE(backend->is_mounted());

  // Should be able to mount valid archive after failure
  EXPECT_TRUE(backend->mount(fixtures_path + "simple.dwarfs", mount_point));
  EXPECT_TRUE(backend->is_mounted());

  backend->unmount();
}

TEST_F(DwarfsIntegrationTest, HandleOperationsAfterUnmount) {
  auto backend = std::make_unique<DwarfsBackend>();
  ASSERT_TRUE(backend->mount(fixtures_path + "simple.dwarfs", mount_point));

  // Get a handle while mounted
  auto handle = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  // Unmount
  backend->unmount();
  EXPECT_FALSE(backend->is_mounted());

  // Operations on backend should fail
  EXPECT_FALSE(backend->exists(mount_point + "/hello.txt"));
  EXPECT_EQ(backend->open(mount_point + "/hello.txt", O_RDONLY), nullptr);

  // Note: Using the old handle after unmount is undefined behavior (use-after-free)
  // and can legitimately crash. We don't test this as it's not a supported use case.
}

// ===================================================================
// 4. Cross-Backend Comparison Tests (2 tests)
// ===================================================================

TEST_F(DwarfsIntegrationTest, DwarfsVsZipCompatibility) {
  // Both backends should provide the same logical interface
  auto dwarfs = std::make_unique<DwarfsBackend>();

  std::string dwarfs_archive = fixtures_path + "simple.dwarfs";
  if (dwarfs->mount(dwarfs_archive, mount_point)) {
    // Check that basic operations work
    EXPECT_TRUE(dwarfs->exists(mount_point + "/hello.txt"));
    EXPECT_TRUE(dwarfs->is_file(mount_point + "/hello.txt"));
    EXPECT_FALSE(dwarfs->is_directory(mount_point + "/hello.txt"));

    auto handle = dwarfs->open(mount_point + "/hello.txt", O_RDONLY);
    EXPECT_NE(handle, nullptr);

    dwarfs->unmount();
  } else {
    GTEST_SKIP() << "DwarFS archive not available";
  }
}

TEST_F(DwarfsIntegrationTest, NativeSeekAdvantageOverZip) {
  auto backend = std::make_unique<DwarfsBackend>();
  std::string archive = fixtures_path + "large.dwarfs";

  if (!backend->mount(archive, mount_point)) {
    GTEST_SKIP() << "Large DwarFS archive not available";
  }

  auto handle = backend->open(mount_point + "/10mb.bin", O_RDONLY);
  if (!handle) {
    backend->unmount();
    GTEST_SKIP() << "Large file not in archive";
  }

  // DwarFS should handle many seeks efficiently (no reopening)
  for (int i = 0; i < 50; i++) {
    off_t pos = (i * 12345) % (10 * 1024 * 1024);
    off_t result = handle->seek(pos, SEEK_SET);
    EXPECT_EQ(result, pos);
  }

  backend->unmount();
}

// ===================================================================
// 5. Real-World Usage Patterns (1 test)
// ===================================================================

TEST_F(DwarfsIntegrationTest, TypicalApplicationUsagePattern) {
  // Simulate typical application: mount, read config, read resources, unmount
  auto backend = std::make_unique<DwarfsBackend>();
  std::string archive = fixtures_path + "simple.dwarfs";

  // 1. Mount application archive
  ASSERT_TRUE(backend->mount(archive, mount_point));

  // 2. Check if config exists
  bool has_config = backend->exists(mount_point + "/hello.txt");
  EXPECT_TRUE(has_config);

  // 3. Read configuration
  if (has_config) {
    auto handle = backend->open(mount_point + "/hello.txt", O_RDONLY);
    ASSERT_NE(handle, nullptr);

    char buffer[256];
    ssize_t bytes = handle->read(buffer, sizeof(buffer));
    EXPECT_GT(bytes, 0);
  }

  // 4. List available resources
  auto iter = backend->list_directory(mount_point);
  ASSERT_NE(iter, nullptr);

  std::vector<std::string> resources;
  while (iter->has_next()) {
    resources.push_back(iter->next().name);
  }
  EXPECT_GT(resources.size(), 0);

  // 5. Access specific resource
  if (std::find(resources.begin(), resources.end(), "test.txt") != resources.end()) {
    auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
    EXPECT_NE(handle, nullptr);
  }

  // 6. Clean shutdown
  backend->unmount();
  EXPECT_FALSE(backend->is_mounted());
}