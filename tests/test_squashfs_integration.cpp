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
#include <tebako/fs/backends/squashfs_backend.h>
#include <tebako/fs/file_handle.h>
#include <fcntl.h>

using namespace tebako::fs;

// ===================================================================
// Test Fixtures
// ===================================================================

class BackendFactorySquashFSTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    // Test fixtures path
    fixtures_path = "tests/fixtures/squashfs/";
  }

  std::string fixtures_path;
  const std::string mount_point = "/mnt/test";
};

// ===================================================================
// 1. Format Detection Tests (4 tests)
// ===================================================================

TEST_F(BackendFactorySquashFSTest, DetectsSquashFSByMagicBytes)
{
  std::string path = fixtures_path + "simple.sqfs";
  EXPECT_TRUE(BackendFactory::is_squashfs_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_zip_format(path));
}

TEST_F(BackendFactorySquashFSTest, DetectsSquashFSByExtension)
{
  // Extension should be recognized
  std::string path = fixtures_path + "simple.sqfs";
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "SquashFS");
}

TEST_F(BackendFactorySquashFSTest, RejectsNonSquashFSFiles)
{
  std::string path = fixtures_path + "corrupted.sqfs";
  // Corrupted file should fail format detection
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));
}

TEST_F(BackendFactorySquashFSTest, HandlesCorruptedSquashFSFiles)
{
  std::string path = fixtures_path + "corrupted.sqfs";
  // Should not detect as valid SquashFS format
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));

  // create_from_file should return nullptr for corrupted files
  auto backend = BackendFactory::create_from_file(path);
  // Even if it creates a backend (e.g. via the .sqfs extension), mounting should fail
  if (backend) {
    auto mount_result = backend->mount(path, mount_point);
    EXPECT_TRUE(mount_result.is_err());
  }
}

// ===================================================================
// 2. Backend Instantiation Tests (4 tests)
// ===================================================================

TEST_F(BackendFactorySquashFSTest, CreateSquashFSReturnsSquashFSBackend)
{
  auto backend = BackendFactory::create_squashfs();
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "SquashFS");
}

TEST_F(BackendFactorySquashFSTest, CreateFromFileReturnsSquashFSBackend)
{
  std::string path = fixtures_path + "simple.sqfs";
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "SquashFS");
}

TEST_F(BackendFactorySquashFSTest, BackendNameIsSquashFS)
{
  auto backend = BackendFactory::create_squashfs();
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "SquashFS");
}

TEST_F(BackendFactorySquashFSTest, BackendVersionMatchesSquashFSToolsNG)
{
  auto backend = BackendFactory::create_squashfs();
  ASSERT_NE(backend, nullptr);

  std::string version = backend->backend_version();
  EXPECT_FALSE(version.empty());
  // Version should contain "squashfs"
  EXPECT_NE(version.find("squashfs"), std::string::npos);
}

// ===================================================================
// 3. End-to-End Tests (3 tests)
// ===================================================================

TEST_F(BackendFactorySquashFSTest, FactoryCreatesMountReadsFile)
{
  std::string path = fixtures_path + "simple.sqfs";

  // Create backend via factory
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);

  // Mount archive
  auto mount_result = backend->mount(path, mount_point);
  ASSERT_TRUE(mount_result.is_ok());
  EXPECT_TRUE(backend->is_mounted());

  // Read file
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  char buffer[256] = {0};
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer) - 1);
  EXPECT_GT(bytes_read, 0);

  std::string content(buffer, bytes_read);
  EXPECT_EQ(content, "Hello from SquashFS!\n");

  // Cleanup
  backend->unmount();
}

TEST_F(BackendFactorySquashFSTest, FactoryHandlesMultipleSquashFSArchives)
{
  // Create backend for first archive
  auto backend1 = BackendFactory::create_from_file(fixtures_path + "simple.sqfs");
  ASSERT_NE(backend1, nullptr);
  auto mount_result1 = backend1->mount(fixtures_path + "simple.sqfs", "/mnt/archive1");
  ASSERT_TRUE(mount_result1.is_ok());

  // Create backend for second archive
  auto backend2 = BackendFactory::create_from_file(fixtures_path + "nested.sqfs");
  ASSERT_NE(backend2, nullptr);
  auto mount_result2 = backend2->mount(fixtures_path + "nested.sqfs", "/mnt/archive2");
  ASSERT_TRUE(mount_result2.is_ok());

  // Both should be mounted simultaneously
  EXPECT_TRUE(backend1->is_mounted());
  EXPECT_TRUE(backend2->is_mounted());

  // Verify files in each archive
  EXPECT_TRUE(backend1->exists("/mnt/archive1/test.txt"));
  EXPECT_TRUE(backend2->exists("/mnt/archive2/dir1/file1.txt"));

  // Cleanup
  backend1->unmount();
  backend2->unmount();
}

TEST_F(BackendFactorySquashFSTest, FactoryAutoDetectsAndInstantiates)
{
  std::string path = fixtures_path + "simple.sqfs";

  // create_from_file should auto-detect SquashFS format
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "SquashFS");

  // Should be able to mount and use
  auto mount_result = backend->mount(path, mount_point);
  ASSERT_TRUE(mount_result.is_ok());
  EXPECT_TRUE(backend->exists(mount_point + "/test.txt"));

  backend->unmount();
}

// ===================================================================
// 4. SquashFS-Specific Tests (2 tests)
// ===================================================================

TEST_F(BackendFactorySquashFSTest, PreservesPermissionsCorrectly)
{
  std::string path = fixtures_path + "permissions.sqfs";
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);
  auto mount_result = backend->mount(path, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  // SquashFS preserves POSIX permissions (unlike ZIP's defaults)
  auto readonly_result = backend->permissions(mount_point + "/readonly.txt");
  ASSERT_TRUE(readonly_result.is_ok());
  EXPECT_EQ(readonly_result.unwrap(), 0444);

  auto script_result = backend->permissions(mount_point + "/script.sh");
  ASSERT_TRUE(script_result.is_ok());
  EXPECT_EQ(script_result.unwrap(), 0755);

  backend->unmount();
}

TEST_F(BackendFactorySquashFSTest, SupportsNativeSeek)
{
  std::string path = fixtures_path + "simple.sqfs";
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);
  auto mount_result = backend->mount(path, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  // SquashFS supports native seek (advantage over ZIP)
  off_t pos = handle->seek(10, SEEK_SET);
  EXPECT_EQ(pos, 10);
  EXPECT_EQ(handle->tell(), 10);

  // Can seek to end
  pos = handle->seek(0, SEEK_END);
  EXPECT_EQ(pos, handle->size());

  backend->unmount();
}

// ===================================================================
// Main
// ===================================================================

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
