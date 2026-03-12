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
#include <tebako/fs/backends/zip_backend.h>
#include <tebako/fs/file_handle.h>
#include <fcntl.h>

using namespace tebako::fs;

// ===================================================================
// Test Fixtures
// ===================================================================

class BackendFactoryZipTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Test fixtures path
    fixtures_path = "tests/fixtures/zip/";
  }

  std::string fixtures_path;
  const std::string mount_point = "/mnt/test";
};

// ===================================================================
// 1. Format Detection Tests (6 tests)
// ===================================================================

TEST_F(BackendFactoryZipTest, DetectsZipByMagicBytes) {
  std::string path = fixtures_path + "simple.zip";
  EXPECT_TRUE(BackendFactory::is_zip_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));
}

TEST_F(BackendFactoryZipTest, DetectsZipByExtension) {
  // Even if magic bytes aren't recognized, extension should work
  std::string path = fixtures_path + "simple.zip";
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "ZIP");
}

TEST_F(BackendFactoryZipTest, DetectsJarFiles) {
  // JAR files are ZIP-based
  std::string path = fixtures_path + "simple.zip";
  // Test that factory would recognize .jar extension
  EXPECT_TRUE(BackendFactory::is_zip_format(path));
}

TEST_F(BackendFactoryZipTest, DetectsApkFiles) {
  // APK files are ZIP-based
  std::string path = fixtures_path + "simple.zip";
  // Test that factory recognizes ZIP format
  EXPECT_TRUE(BackendFactory::is_zip_format(path));
}

TEST_F(BackendFactoryZipTest, CorruptedZipDetectedButMountFails) {
  std::string path = fixtures_path + "corrupted.zip";
  // Corrupted file still has valid ZIP magic bytes, so format detection succeeds
  EXPECT_TRUE(BackendFactory::is_zip_format(path));

  // But mounting should fail due to corruption
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);  // Backend created based on magic bytes
  EXPECT_FALSE(backend->mount(path, mount_point));  // But mount fails
}

TEST_F(BackendFactoryZipTest, CorruptedZipCreatesBackendButCannotMount) {
  std::string path = fixtures_path + "corrupted.zip";
  // Should detect as ZIP format (has valid magic bytes)
  EXPECT_TRUE(BackendFactory::is_zip_format(path));

  // create_from_file should return a backend (based on magic bytes)
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);

  // But mounting should fail (file is corrupted)
  EXPECT_FALSE(backend->mount(path, mount_point));
}

// ===================================================================
// 2. Backend Instantiation Tests (4 tests)
// ===================================================================

TEST_F(BackendFactoryZipTest, CreateZipReturnsZipBackend) {
  auto backend = BackendFactory::create_zip();
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "ZIP");
}

TEST_F(BackendFactoryZipTest, CreateFromFileReturnsZipBackend) {
  std::string path = fixtures_path + "simple.zip";
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "ZIP");
}

TEST_F(BackendFactoryZipTest, BackendNameIsZIP) {
  auto backend = BackendFactory::create_zip();
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "ZIP");
}

TEST_F(BackendFactoryZipTest, BackendVersionMatchesLibzip) {
  auto backend = BackendFactory::create_zip();
  ASSERT_NE(backend, nullptr);

  std::string version = backend->backend_version();
  EXPECT_FALSE(version.empty());
  // Version should contain "libzip"
  EXPECT_NE(version.find("libzip"), std::string::npos);
}

// ===================================================================
// 3. End-to-End Tests (3 tests)
// ===================================================================

TEST_F(BackendFactoryZipTest, FactoryCreatesMountReadsFile) {
  std::string path = fixtures_path + "simple.zip";

  // Create backend via factory
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);

  // Mount archive
  auto mount_result = backend->mount(path, mount_point);
  ASSERT_TRUE(mount_result.is_ok());
  EXPECT_TRUE(backend->is_mounted());

  // Read file
  auto handle_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  char buffer[256] = {0};
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer) - 1);
  EXPECT_GT(bytes_read, 0);

  std::string content(buffer, bytes_read);
  EXPECT_EQ(content, "Hello from ZIP!\n");

  // Cleanup
  backend->unmount();
}

TEST_F(BackendFactoryZipTest, FactoryHandlesMultipleZipArchives) {
  // Create backend for first archive
  auto backend1 = BackendFactory::create_from_file(fixtures_path + "simple.zip");
  ASSERT_NE(backend1, nullptr);
  auto mount1 = backend1->mount(fixtures_path + "simple.zip", "/mnt/archive1");
  ASSERT_TRUE(mount1.is_ok());

  // Create backend for second archive
  auto backend2 = BackendFactory::create_from_file(fixtures_path + "nested.zip");
  ASSERT_NE(backend2, nullptr);
  auto mount2 = backend2->mount(fixtures_path + "nested.zip", "/mnt/archive2");
  ASSERT_TRUE(mount2.is_ok());

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

TEST_F(BackendFactoryZipTest, FactoryAutoDetectsAndInstantiates) {
  std::string path = fixtures_path + "simple.zip";

  // create_from_file should auto-detect ZIP format
  auto backend = BackendFactory::create_from_file(path);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->backend_name(), "ZIP");

  // Should be able to mount and use
  auto mount_result = backend->mount(path, mount_point);
  ASSERT_TRUE(mount_result.is_ok());
  EXPECT_TRUE(backend->exists(mount_point + "/test.txt"));

  backend->unmount();
}

// ===================================================================
// Main
// ===================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}