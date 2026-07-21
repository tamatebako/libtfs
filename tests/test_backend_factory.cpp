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
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <string>

using namespace tebako::fs;

// ===================================================================
// Test Fixtures
// ===================================================================

class BackendFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    // Create test directory under the OS temp dir (portable; /tmp + system()
    // are not available to native Windows binaries)
    test_dir_ = (std::filesystem::temp_directory_path() / (std::string("tebako_test_") + std::to_string(getpid())))
                    .generic_string();
    std::filesystem::create_directories(test_dir_);
  }

  void TearDown() override
  {
    // Clean up test directory
    std::error_code ec;
    std::filesystem::remove_all(test_dir_, ec);
  }

  /**
   * @brief Create a test file with given magic bytes
   */
  void create_test_file(const std::string& name, const uint8_t* magic, size_t magic_size)
  {
    auto path = test_dir_ + "/" + name;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(magic), magic_size);
    file.close();
  }

  /**
   * @brief Get full path to test file
   */
  std::string get_test_path(const std::string& name) const { return test_dir_ + "/" + name; }

  std::string test_dir_;
};

// ===================================================================
// Factory Creation Tests
// ===================================================================

TEST_F(BackendFactoryTest, CreateDwarfs)
{
  auto backend = BackendFactory::create_dwarfs();
  // TODO: Once DwarfsBackend is implemented, verify:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, CreateZip)
{
  auto backend = BackendFactory::create_zip();
  // TODO: Once ZipBackend is implemented, verify:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "ZIP");
}

#ifdef TEBAKO_WITH_SQUASHFS
TEST_F(BackendFactoryTest, CreateSquashFS)
{
  auto backend = BackendFactory::create_squashfs();
  // TODO: Once SquashFSBackend is implemented, verify:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "SquashFS");
}
#endif

// ===================================================================
// Magic Number Detection Tests
// ===================================================================

TEST_F(BackendFactoryTest, DetectDwarfsMagic)
{
  // DwarFS magic: "DWARFS" at offset 0
  uint8_t dwarfs_magic[] = {'D', 'W', 'A', 'R', 'F', 'S', 0x00, 0x01};
  create_test_file("test.dwarfs", dwarfs_magic, sizeof(dwarfs_magic));

  auto path = get_test_path("test.dwarfs");
  EXPECT_TRUE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_zip_format(path));
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));
}

TEST_F(BackendFactoryTest, DetectZipLocalMagic)
{
  // ZIP local file header magic
  uint8_t zip_magic[] = {0x50, 0x4B, 0x03, 0x04, 0x00, 0x00};
  create_test_file("test.zip", zip_magic, sizeof(zip_magic));

  auto path = get_test_path("test.zip");
  EXPECT_TRUE(BackendFactory::is_zip_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));
}

TEST_F(BackendFactoryTest, DetectZipCentralMagic)
{
  // ZIP central directory magic (for empty archives)
  uint8_t zip_magic[] = {0x50, 0x4B, 0x05, 0x06, 0x00, 0x00};
  create_test_file("empty.zip", zip_magic, sizeof(zip_magic));

  auto path = get_test_path("empty.zip");
  EXPECT_TRUE(BackendFactory::is_zip_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));
}

#ifdef TEBAKO_WITH_SQUASHFS
TEST_F(BackendFactoryTest, DetectSquashFSLittleEndian)
{
  // SquashFS little-endian magic: "hsqs"
  uint8_t sqfs_magic[] = {0x68, 0x73, 0x71, 0x73, 0x00, 0x00};
  create_test_file("test.sqfs", sqfs_magic, sizeof(sqfs_magic));

  auto path = get_test_path("test.sqfs");
  EXPECT_TRUE(BackendFactory::is_squashfs_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_zip_format(path));
}

TEST_F(BackendFactoryTest, DetectSquashFSBigEndian)
{
  // SquashFS big-endian magic: "sqsh"
  uint8_t sqfs_magic[] = {0x73, 0x71, 0x73, 0x68, 0x00, 0x00};
  create_test_file("test_be.sqfs", sqfs_magic, sizeof(sqfs_magic));

  auto path = get_test_path("test_be.sqfs");
  EXPECT_TRUE(BackendFactory::is_squashfs_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_zip_format(path));
}
#endif

// ===================================================================
// Auto-Detection Tests
// ===================================================================

TEST_F(BackendFactoryTest, AutoDetectDwarfs)
{
  uint8_t dwarfs_magic[] = {'D', 'W', 'A', 'R', 'F', 'S'};
  create_test_file("archive.dwarfs", dwarfs_magic, sizeof(dwarfs_magic));

  auto path = get_test_path("archive.dwarfs");
  auto backend = BackendFactory::create_from_file(path);

  // TODO: Once DwarfsBackend is implemented:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, AutoDetectZip)
{
  uint8_t zip_magic[] = {0x50, 0x4B, 0x03, 0x04};
  create_test_file("archive.zip", zip_magic, sizeof(zip_magic));

  auto path = get_test_path("archive.zip");
  auto backend = BackendFactory::create_from_file(path);

  // TODO: Once ZipBackend is implemented:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "ZIP");
}

#ifdef TEBAKO_WITH_SQUASHFS
TEST_F(BackendFactoryTest, AutoDetectSquashFS)
{
  uint8_t sqfs_magic[] = {0x68, 0x73, 0x71, 0x73};
  create_test_file("archive.sqfs", sqfs_magic, sizeof(sqfs_magic));

  auto path = get_test_path("archive.sqfs");
  auto backend = BackendFactory::create_from_file(path);

  // TODO: Once SquashFSBackend is implemented:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "SquashFS");
}
#endif

// ===================================================================
// Extension Fallback Tests
// ===================================================================

TEST_F(BackendFactoryTest, ExtensionFallbackZip)
{
  // File with no recognizable magic number
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};
  create_test_file("file.jar", dummy, sizeof(dummy));

  auto path = get_test_path("file.jar");
  auto backend = BackendFactory::create_from_file(path);

  // TODO: Once ZipBackend is implemented:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "ZIP");
}

TEST_F(BackendFactoryTest, ExtensionFallbackDwarfs)
{
  // File with no recognizable magic number
  uint8_t dummy[] = {0xFF, 0xFF, 0xFF, 0xFF};
  create_test_file("file.dfs", dummy, sizeof(dummy));

  auto path = get_test_path("file.dfs");
  auto backend = BackendFactory::create_from_file(path);

  // TODO: Once DwarfsBackend is implemented:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "DwarFS");
}

TEST_F(BackendFactoryTest, ExtensionCaseInsensitive)
{
  // Test case-insensitive extension matching
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};
  create_test_file("file.ZIP", dummy, sizeof(dummy));

  auto path = get_test_path("file.ZIP");
  auto backend = BackendFactory::create_from_file(path);

  // TODO: Once ZipBackend is implemented:
  // ASSERT_NE(backend, nullptr);
  // EXPECT_EQ(backend->backend_name(), "ZIP");
}

// ===================================================================
// Error Case Tests
// ===================================================================

TEST_F(BackendFactoryTest, UnknownFormat)
{
  uint8_t dummy[] = {0xFF, 0xFF, 0xFF, 0xFF};
  create_test_file("file.unknown", dummy, sizeof(dummy));

  auto path = get_test_path("file.unknown");
  auto backend = BackendFactory::create_from_file(path);

  EXPECT_EQ(backend, nullptr);
}

TEST_F(BackendFactoryTest, NonExistentFile)
{
  auto path = get_test_path("nonexistent.zip");

  EXPECT_FALSE(BackendFactory::is_zip_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));

  auto backend = BackendFactory::create_from_file(path);
  EXPECT_EQ(backend, nullptr);
}

TEST_F(BackendFactoryTest, FileTooSmall)
{
  // Create a file with only 2 bytes (too small for any magic)
  uint8_t tiny[] = {0x50, 0x4B};
  create_test_file("tiny.zip", tiny, sizeof(tiny));

  auto path = get_test_path("tiny.zip");

  // Magic detection should fail (file too small)
  EXPECT_FALSE(BackendFactory::is_zip_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));

  // Extension fallback should work
  auto backend = BackendFactory::create_from_file(path);
  // TODO: Once ZipBackend is implemented:
  // ASSERT_NE(backend, nullptr);
}

TEST_F(BackendFactoryTest, EmptyFile)
{
  // Create empty file
  create_test_file("empty.zip", nullptr, 0);

  auto path = get_test_path("empty.zip");

  EXPECT_FALSE(BackendFactory::is_zip_format(path));
  EXPECT_FALSE(BackendFactory::is_dwarfs_format(path));
  EXPECT_FALSE(BackendFactory::is_squashfs_format(path));
}

// ===================================================================
// Multiple Extension Tests
// ===================================================================

TEST_F(BackendFactoryTest, ZipVariantExtensions)
{
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};

  // Test various ZIP-based formats
  const char* extensions[] = {".zip", ".jar", ".apk", ".war", ".ear"};

  for (const char* ext : extensions) {
    std::string filename = std::string("file") + ext;
    create_test_file(filename, dummy, sizeof(dummy));

    auto path = get_test_path(filename);
    auto backend = BackendFactory::create_from_file(path);

    // TODO: Once ZipBackend is implemented:
    // ASSERT_NE(backend, nullptr) << "Failed for extension: " << ext;
    // EXPECT_EQ(backend->backend_name(), "ZIP");
  }
}

#ifdef TEBAKO_WITH_SQUASHFS
TEST_F(BackendFactoryTest, SquashFSExtensions)
{
  uint8_t dummy[] = {0x00, 0x00, 0x00, 0x00};

  // Test SquashFS extensions
  const char* extensions[] = {".sqfs", ".squashfs"};

  for (const char* ext : extensions) {
    std::string filename = std::string("file") + ext;
    create_test_file(filename, dummy, sizeof(dummy));

    auto path = get_test_path(filename);
    auto backend = BackendFactory::create_from_file(path);

    // TODO: Once SquashFSBackend is implemented:
    // ASSERT_NE(backend, nullptr) << "Failed for extension: " << ext;
    // EXPECT_EQ(backend->backend_name(), "SquashFS");
  }
}
#endif

// ===================================================================
// Main
// ===================================================================

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}