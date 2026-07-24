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
#include <tebako/fs/c_api.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include <fcntl.h>
#include <unistd.h>

#include "zip_test_helper.h"

namespace fs = std::filesystem;

/**
 * @brief Test fixture for C API tests
 *
 * Creates a temporary ZIP archive for testing C API functionality.
 *
 * NOTE: These tests share global C API state and must be serialized.
 * CMakeLists.txt uses RESOURCE_LOCK to ensure this.
 */
class CApiTest : public ::testing::Test {
 protected:
  std::string test_dir;
  std::string archive_path;
  std::string mount_point;

  void SetUp() override
  {
    // Create temporary directory
    test_dir = (fs::temp_directory_path() / "tebako_c_api_test").generic_string();
    fs::create_directories(test_dir);

    // Create test files
    create_test_archive();

    mount_point = "/__tebako_test__";
  }

  void TearDown() override
  {
    // Unmount if still mounted
    tebako_fs_unmount();

    // Clean up test directory
    if (fs::exists(test_dir)) {
      fs::remove_all(test_dir);
    }
  }

  void create_test_archive()
  {
    // Create a simple directory structure
    fs::path content_dir = fs::path(test_dir) / "content";
    fs::create_directories(content_dir);
    fs::create_directories(content_dir / "subdir");

    // Create test files
    write_file(content_dir / "hello.txt", "Hello, World!");
    write_file(content_dir / "data.bin", std::string(1024, 'X'));
    write_file(content_dir / "subdir" / "nested.txt", "Nested file content");
    write_file(content_dir / "empty.txt", "");

    // Create ZIP archive in-process (the system `zip` command and /dev/null
    // are not available to native Windows binaries)
    archive_path = test_dir + "/test.zip";
    ASSERT_TRUE(tebako_test::create_zip_from_dir(archive_path, content_dir, "content"))
        << "Failed to create test ZIP archive";
    ASSERT_TRUE(fs::exists(archive_path)) << "Archive not created: " << archive_path;
  }

  void write_file(const fs::path& path, const std::string& content)
  {
    std::ofstream ofs(path, std::ios::binary);
    ofs << content;
    ofs.close();
  }

  std::string read_file_via_api(const std::string& path)
  {
    int fd = tebako_fs_open(path.c_str(), O_RDONLY);
    if (fd < 0)
      return "";

    std::vector<char> buffer(4096);
    ssize_t n = tebako_fs_read(fd, buffer.data(), buffer.size());
    tebako_fs_close(fd);

    if (n <= 0)
      return "";
    return std::string(buffer.data(), n);
  }
};

// ============================================================
// Lifecycle Tests (8 tests)
// ============================================================

TEST_F(CApiTest, InitFromFile_Success)
{
  EXPECT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  EXPECT_EQ(1, tebako_is_initialized());
  EXPECT_EQ(0, tebako_get_errno());
}

TEST_F(CApiTest, InitFromFile_NullPath)
{
  EXPECT_EQ(-1, tebako_fs_init_from_file(nullptr, mount_point.c_str()));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiTest, InitFromFile_NullMountPoint)
{
  EXPECT_EQ(-1, tebako_fs_init_from_file(archive_path.c_str(), nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiTest, InitFromFile_NonexistentFile)
{
  EXPECT_EQ(-1, tebako_fs_init_from_file("/nonexistent/file.zip", mount_point.c_str()));
  EXPECT_NE(0, tebako_get_errno());
}

TEST_F(CApiTest, InitTwice_FailsWithEEXIST)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  EXPECT_EQ(-1, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  EXPECT_EQ(EEXIST, tebako_get_errno());
}

TEST_F(CApiTest, Unmount_CleansUp)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  EXPECT_EQ(1, tebako_is_initialized());

  tebako_fs_unmount();
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiTest, Unmount_Idempotent)
{
  tebako_fs_unmount();  // First call
  EXPECT_EQ(0, tebako_is_initialized());

  tebako_fs_unmount();  // Second call should be safe
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiTest, OperationsFailAfterUnmount)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  tebako_fs_unmount();

  EXPECT_EQ(-1, tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY));
  EXPECT_EQ(ENODEV, tebako_get_errno());
}

// ===================================================================
// Memory Mounting Tests
// ===================================================================

TEST_F(CApiTest, InitFromMemory_Success)
{
  // Read archive file into memory
  std::ifstream ifs(archive_path, std::ios::binary | std::ios::ate);
  ASSERT_TRUE(ifs.is_open()) << "Failed to open archive: " << archive_path;

  size_t size = ifs.tellg();
  std::vector<uint8_t> data(size);

  ifs.seekg(0);
  ifs.read(reinterpret_cast<char*>(data.data()), size);
  ifs.close();

  // Initialize from memory
  ASSERT_EQ(0, tebako_fs_init(data.data(), data.size(), mount_point.c_str()));
  EXPECT_EQ(1, tebako_is_initialized());

  // Verify mount point
  const char* mp = tebako_get_mount_point();
  ASSERT_NE(nullptr, mp);
  EXPECT_EQ(mount_point, std::string(mp));

  // Verify archive path is empty for memory mounts
  const char* ap = tebako_get_archive_path();
  EXPECT_TRUE(ap == nullptr || std::string(ap).empty());

  // Verify backend name
  const char* bn = tebako_get_backend_name();
  ASSERT_NE(nullptr, bn);
  EXPECT_EQ("ZIP", std::string(bn));
}

TEST_F(CApiTest, InitFromMemory_ReadFile)
{
  // Read archive file into memory
  std::ifstream ifs(archive_path, std::ios::binary | std::ios::ate);
  ASSERT_TRUE(ifs.is_open());

  size_t size = ifs.tellg();
  std::vector<uint8_t> data(size);

  ifs.seekg(0);
  ifs.read(reinterpret_cast<char*>(data.data()), size);
  ifs.close();

  // Initialize from memory
  ASSERT_EQ(0, tebako_fs_init(data.data(), data.size(), mount_point.c_str()));

  // Verify can read files
  std::string content = read_file_via_api(mount_point + "/content/hello.txt");
  EXPECT_EQ("Hello, World!", content);
}

TEST_F(CApiTest, InitFromMemory_InvalidData)
{
  uint8_t bad_data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  EXPECT_EQ(-1, tebako_fs_init(bad_data, sizeof(bad_data), mount_point.c_str()));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiTest, InitFromMemory_NullData)
{
  EXPECT_EQ(-1, tebako_fs_init(nullptr, 100, mount_point.c_str()));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiTest, InitFromMemory_ZeroSize)
{
  uint8_t data[] = {0x50, 0x4B, 0x03, 0x04};
  EXPECT_EQ(-1, tebako_fs_init(data, 0, mount_point.c_str()));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiTest, InitFromMemory_NullMountPoint)
{
  uint8_t data[] = {0x50, 0x4B, 0x03, 0x04};
  EXPECT_EQ(-1, tebako_fs_init(data, sizeof(data), nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

// ===================================================================
// File Operations Tests
// ===================================================================

// ============================================================
// File Operations Tests (15 tests)
// ============================================================

TEST_F(CApiTest, Open_ValidFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  EXPECT_GT(fd, 0);
  EXPECT_TRUE(tebako_fd_is_embedded(fd));
  EXPECT_EQ(0, tebako_get_errno());

  EXPECT_EQ(0, tebako_fs_close(fd));
}

TEST_F(CApiTest, Open_NonexistentFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/nonexistent.txt").c_str(), O_RDONLY);
  EXPECT_EQ(-1, fd);
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, Open_NullPath)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open(nullptr, O_RDONLY);
  EXPECT_EQ(-1, fd);
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiTest, Open_WriteMode_Fails)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_WRONLY);
  EXPECT_EQ(-1, fd);
  EXPECT_EQ(EROFS, tebako_get_errno());
}

TEST_F(CApiTest, Read_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  char buffer[100] = {0};
  ssize_t n = tebako_fs_read(fd, buffer, sizeof(buffer));
  EXPECT_GT(n, 0);
  EXPECT_STREQ("Hello, World!", buffer);

  tebako_fs_close(fd);
}

TEST_F(CApiTest, Read_EmptyFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/empty.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  char buffer[10];
  ssize_t n = tebako_fs_read(fd, buffer, sizeof(buffer));
  EXPECT_EQ(0, n);  // EOF immediately

  tebako_fs_close(fd);
}

TEST_F(CApiTest, Read_InvalidFd)
{
  char buffer[10];
  ssize_t n = tebako_fs_read(999, buffer, sizeof(buffer));
  EXPECT_EQ(-1, n);
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Read_NullBuffer)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  ssize_t n = tebako_fs_read(fd, nullptr, 10);
  EXPECT_EQ(-1, n);
  EXPECT_EQ(EINVAL, tebako_get_errno());

  tebako_fs_close(fd);
}

TEST_F(CApiTest, Lseek_SeekSet)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  // Seek to position 7
  off_t pos = tebako_fs_lseek(fd, 7, SEEK_SET);
  EXPECT_EQ(7, pos);

  // Read from new position
  char buffer[10] = {0};
  ssize_t n = tebako_fs_read(fd, buffer, 5);
  EXPECT_EQ(5, n);
  EXPECT_STREQ("World", buffer);

  tebako_fs_close(fd);
}

TEST_F(CApiTest, Lseek_SeekCur)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  // Read 5 bytes
  char buffer[5];
  tebako_fs_read(fd, buffer, 5);

  // Seek forward 2 bytes from current
  off_t pos = tebako_fs_lseek(fd, 2, SEEK_CUR);
  EXPECT_EQ(7, pos);

  tebako_fs_close(fd);
}

TEST_F(CApiTest, Lseek_SeekEnd)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  // Seek to end
  off_t size = tebako_fs_lseek(fd, 0, SEEK_END);
  EXPECT_EQ(13, size);  // strlen("Hello, World!")

  // Try to read (should get EOF)
  char buffer[10];
  ssize_t n = tebako_fs_read(fd, buffer, sizeof(buffer));
  EXPECT_EQ(0, n);

  tebako_fs_close(fd);
}

TEST_F(CApiTest, Lseek_InvalidFd)
{
  off_t pos = tebako_fs_lseek(999, 0, SEEK_SET);
  EXPECT_EQ(-1, pos);
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Close_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  EXPECT_EQ(0, tebako_fs_close(fd));

  // Further operations should fail
  char buffer[10];
  EXPECT_EQ(-1, tebako_fs_read(fd, buffer, sizeof(buffer)));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Close_InvalidFd)
{
  EXPECT_EQ(-1, tebako_fs_close(999));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, MultipleFds_Independent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd1 = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  int fd2 = tebako_fs_open((mount_point + "/content/data.bin").c_str(), O_RDONLY);

  ASSERT_GT(fd1, 0);
  ASSERT_GT(fd2, 0);
  EXPECT_NE(fd1, fd2);

  // Both should work independently
  char buf1[10], buf2[10];
  EXPECT_GT(tebako_fs_read(fd1, buf1, sizeof(buf1)), 0);
  EXPECT_GT(tebako_fs_read(fd2, buf2, sizeof(buf2)), 0);

  tebako_fs_close(fd1);
  tebako_fs_close(fd2);
}

// ============================================================
// Directory Operations Tests (10 tests)
// ============================================================

TEST_F(CApiTest, Opendir_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);
  EXPECT_EQ(0, tebako_get_errno());

  EXPECT_EQ(0, tebako_fs_closedir(dir));
}

TEST_F(CApiTest, Opendir_Nonexistent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/nonexistent").c_str());
  EXPECT_EQ(nullptr, dir);
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, Opendir_NullPath)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir(nullptr);
  EXPECT_EQ(nullptr, dir);
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiTest, Readdir_ListFiles)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  std::vector<std::string> entries;
  struct tebako_c_dirent* entry;
  while ((entry = tebako_fs_readdir(dir)) != nullptr) {
    entries.push_back(entry->d_name);
  }

  // Should have at least our test files
  EXPECT_FALSE(entries.empty());

  // Check for known files
  auto has_hello = std::find(entries.begin(), entries.end(), "hello.txt") != entries.end();
  auto has_data = std::find(entries.begin(), entries.end(), "data.bin") != entries.end();
  auto has_subdir = std::find(entries.begin(), entries.end(), "subdir") != entries.end();

  EXPECT_TRUE(has_hello || has_data || has_subdir) << "Expected test files not found";

  tebako_fs_closedir(dir);
}

TEST_F(CApiTest, Readdir_CheckTypes)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  bool found_file = false;
  bool found_dir = false;

  struct tebako_c_dirent* entry;
  while ((entry = tebako_fs_readdir(dir)) != nullptr) {
    if (entry->d_type == DT_REG)
      found_file = true;
    if (entry->d_type == DT_DIR)
      found_dir = true;
  }

  EXPECT_TRUE(found_file) << "Should find at least one regular file";

  tebako_fs_closedir(dir);
}

TEST_F(CApiTest, Readdir_EmptyAtEnd)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  // Read all entries
  while (tebako_fs_readdir(dir) != nullptr) {
  }

  // Next call should return nullptr
  EXPECT_EQ(nullptr, tebako_fs_readdir(dir));

  tebako_fs_closedir(dir);
}

TEST_F(CApiTest, Readdir_InvalidHandle)
{
  struct tebako_c_dirent* entry = tebako_fs_readdir(nullptr);
  EXPECT_EQ(nullptr, entry);
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Closedir_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  EXPECT_EQ(0, tebako_fs_closedir(dir));
  EXPECT_EQ(0, tebako_get_errno());
}

TEST_F(CApiTest, Closedir_InvalidHandle)
{
  EXPECT_EQ(-1, tebako_fs_closedir(nullptr));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, MultipleDirs_Independent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir1 = tebako_fs_opendir((mount_point + "/content").c_str());
  tebako_dir_t dir2 = tebako_fs_opendir((mount_point + "/content/subdir").c_str());

  ASSERT_NE(nullptr, dir1);
  ASSERT_NE(nullptr, dir2);
  EXPECT_NE(dir1, dir2);

  // Both should work
  EXPECT_NE(nullptr, tebako_fs_readdir(dir1));

  tebako_fs_closedir(dir1);
  tebako_fs_closedir(dir2);
}

// ============================================================
// Metadata Operations Tests (7 tests)
// ============================================================

TEST_F(CApiTest, Stat_RegularFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  struct stat st;
  int result = tebako_fs_stat((mount_point + "/content/hello.txt").c_str(), &st);

  EXPECT_EQ(0, result);
  EXPECT_TRUE(S_ISREG(st.st_mode));
  EXPECT_EQ(13, st.st_size);  // strlen("Hello, World!")
}

TEST_F(CApiTest, Stat_Directory)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  struct stat st;
  int result = tebako_fs_stat((mount_point + "/content/subdir").c_str(), &st);

  EXPECT_EQ(0, result);
  EXPECT_TRUE(S_ISDIR(st.st_mode));
}

TEST_F(CApiTest, Stat_Nonexistent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  struct stat st;
  int result = tebako_fs_stat((mount_point + "/nonexistent").c_str(), &st);

  EXPECT_EQ(-1, result);
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, Stat_NullArguments)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  struct stat st;
  EXPECT_EQ(-1, tebako_fs_stat(nullptr, &st));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  EXPECT_EQ(-1, tebako_fs_stat((mount_point + "/content/hello.txt").c_str(), nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiTest, Fstat_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  struct stat st;
  EXPECT_EQ(0, tebako_fs_fstat(fd, &st));
  EXPECT_TRUE(S_ISREG(st.st_mode));
  EXPECT_EQ(13, st.st_size);

  tebako_fs_close(fd);
}

TEST_F(CApiTest, Fstat_InvalidFd)
{
  struct stat st;
  EXPECT_EQ(-1, tebako_fs_fstat(999, &st));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Fstat_NullStat)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  EXPECT_EQ(-1, tebako_fs_fstat(fd, nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  tebako_fs_close(fd);
}

// ============================================================
// Path Detection Tests (6 tests)
// ============================================================

TEST_F(CApiTest, PathIsEmbedded_ValidPaths)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(1, tebako_path_is_embedded((mount_point + "/content/hello.txt").c_str()));
  EXPECT_EQ(1, tebako_path_is_embedded((mount_point + "/any/path").c_str()));
  EXPECT_EQ(1, tebako_path_is_embedded(mount_point.c_str()));
}

TEST_F(CApiTest, PathIsEmbedded_ExternalPaths)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(0, tebako_path_is_embedded("/tmp/file.txt"));
  EXPECT_EQ(0, tebako_path_is_embedded("/usr/bin/ls"));
  EXPECT_EQ(0, tebako_path_is_embedded("relative/path.txt"));
}

TEST_F(CApiTest, PathIsEmbedded_NullPath)
{
  EXPECT_EQ(0, tebako_path_is_embedded(nullptr));
}

TEST_F(CApiTest, PathIsEmbedded_NotInitialized)
{
  EXPECT_EQ(0, tebako_path_is_embedded((mount_point + "/file.txt").c_str()));
}

TEST_F(CApiTest, FdIsEmbedded_ValidFd)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  EXPECT_EQ(1, tebako_fd_is_embedded(fd));
  EXPECT_EQ(0, tebako_fd_is_embedded(STDOUT_FILENO));
  EXPECT_EQ(0, tebako_fd_is_embedded(STDIN_FILENO));

  tebako_fs_close(fd);
}

TEST_F(CApiTest, FdIsEmbedded_FlagCheck)
{
  // Check FD flag bit is properly set
  int fake_fd = 123 | TEBAKO_FD_FLAG;
  EXPECT_EQ(1, tebako_fd_is_embedded(fake_fd));

  int normal_fd = 123;
  EXPECT_EQ(0, tebako_fd_is_embedded(normal_fd));
}

// ============================================================
// Error Handling Tests (3 tests)
// ============================================================

TEST_F(CApiTest, GetErrno_ThreadLocal)
{
  // Set an error
  tebako_fs_open(nullptr, O_RDONLY);  // Will set EINVAL
  EXPECT_EQ(EINVAL, tebako_get_errno());

  // Different operation sets different error
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  tebako_fs_open((mount_point + "/nonexistent").c_str(), O_RDONLY);
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, Strerror_ValidCodes)
{
  const char* msg = tebako_strerror(ENOENT);
  EXPECT_NE(nullptr, msg);
  EXPECT_NE(std::string(""), msg);

  msg = tebako_strerror(EINVAL);
  EXPECT_NE(nullptr, msg);
}

TEST_F(CApiTest, Strerror_DoNotFree)
{
  const char* msg1 = tebako_strerror(ENOENT);
  const char* msg2 = tebako_strerror(ENOENT);

  // Should return same static string
  EXPECT_STREQ(msg1, msg2);
}

// ============================================================
// Utility Functions Tests (3 tests)
// ============================================================

TEST_F(CApiTest, GetMountPoint_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  const char* mp = tebako_get_mount_point();
  ASSERT_NE(nullptr, mp);
  EXPECT_STREQ(mount_point.c_str(), mp);
}

TEST_F(CApiTest, GetMountPoint_NotMounted)
{
  const char* mp = tebako_get_mount_point();
  EXPECT_EQ(nullptr, mp);
}

TEST_F(CApiTest, GetBackendName_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  const char* name = tebako_get_backend_name();
  ASSERT_NE(nullptr, name);
  EXPECT_STREQ("ZIP", name);
}

// ============================================================
// Integration Tests (2 tests)
// ============================================================

TEST_F(CApiTest, Integration_ReadNestedFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  std::string content = read_file_via_api(mount_point + "/content/subdir/nested.txt");
  EXPECT_EQ("Nested file content", content);
}

TEST_F(CApiTest, Integration_FullWorkflow)
{
  // Init
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  EXPECT_EQ(1, tebako_is_initialized());

  // Check path detection
  EXPECT_EQ(1, tebako_path_is_embedded((mount_point + "/content").c_str()));

  // List directory
  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  int file_count = 0;
  struct tebako_c_dirent* entry;
  while ((entry = tebako_fs_readdir(dir)) != nullptr) {
    file_count++;

    // Stat each entry
    std::string full_path = mount_point + "/content/" + entry->d_name;
    struct stat st;
    EXPECT_EQ(0, tebako_fs_stat(full_path.c_str(), &st));

    // If regular file, try opening
    if (entry->d_type == DT_REG) {
      int fd = tebako_fs_open(full_path.c_str(), O_RDONLY);
      EXPECT_GT(fd, 0);
      if (fd > 0) {
        tebako_fs_close(fd);
      }
    }
  }

  EXPECT_GT(file_count, 0);
  tebako_fs_closedir(dir);

  // Unmount
  tebako_fs_unmount();
  EXPECT_EQ(0, tebako_is_initialized());
}

// ============================================================
// Offset Mount Tests (tebako_fs_init_from_file_at)
// ============================================================

/**
 * @brief Test fixture for tebako_fs_init_from_file_at()
 *
 * Builds a combined file: N bytes of junk followed by the bytes of the
 * tests/fixtures/dwarfs/simple.dwarfs image, i.e. a DwarFS image embedded
 * at offset N inside a larger file.
 *
 * NOTE: Shares the global C API state; serialized via RESOURCE_LOCK like the
 * other test_c_api tests.
 */
class CApiOffsetTest : public ::testing::Test {
 protected:
  std::string test_dir;
  std::string mount_point;
  std::string plain_image_path;  // tests/fixtures/dwarfs/simple.dwarfs
  std::string combined_path;     // junk + image
  std::vector<char> image;       // bytes of simple.dwarfs
  static constexpr uint64_t kJunkSize = 1000;

  void SetUp() override
  {
    test_dir = (fs::temp_directory_path() / "tebako_c_api_offset_test").generic_string();
    fs::create_directories(test_dir);
    mount_point = "/__tebako_offset_test__";

    // Fixture images are copied next to the test binaries at configure time
    plain_image_path = "tests/fixtures/dwarfs/simple.dwarfs";

    std::ifstream ifs(plain_image_path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(ifs.is_open()) << "Failed to open fixture: " << plain_image_path;
    auto size = ifs.tellg();
    ASSERT_GT(size, 0);
    image.resize(static_cast<size_t>(size));
    ifs.seekg(0);
    ifs.read(image.data(), size);
    ifs.close();

    // Prepend kJunkSize bytes of junk (deliberately not a valid archive magic)
    combined_path = test_dir + "/combined.bin";
    std::ofstream ofs(combined_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(ofs.is_open());
    for (uint64_t i = 0; i < kJunkSize; ++i) {
      char c = static_cast<char>('A' + (i * 7) % 26);
      ofs.write(&c, 1);
    }
    ofs.write(image.data(), static_cast<std::streamsize>(image.size()));
    ofs.close();
    ASSERT_TRUE(fs::exists(combined_path));
  }

  void TearDown() override
  {
    tebako_fs_unmount();
    if (fs::exists(test_dir)) {
      fs::remove_all(test_dir);
    }
  }

  std::string read_file_via_api(const std::string& path)
  {
    int fd = tebako_fs_open(path.c_str(), O_RDONLY);
    if (fd < 0)
      return "";

    std::vector<char> buffer(4096);
    ssize_t n = tebako_fs_read(fd, buffer.data(), buffer.size());
    tebako_fs_close(fd);

    if (n <= 0)
      return "";
    return std::string(buffer.data(), n);
  }
};

TEST_F(CApiOffsetTest, OffsetMount_ExplicitLength_ReadsMatchPlainImage)
{
  // Reference content: mount the plain fixture image
  ASSERT_EQ(0, tebako_fs_init_from_file(plain_image_path.c_str(), mount_point.c_str()));
  std::string expected_hello = read_file_via_api(mount_point + "/hello.txt");
  std::string expected_test = read_file_via_api(mount_point + "/test.txt");
  ASSERT_FALSE(expected_hello.empty()) << "Fixture hello.txt unexpectedly empty";
  ASSERT_FALSE(expected_test.empty()) << "Fixture test.txt unexpectedly empty";
  struct stat expected_st;
  ASSERT_EQ(0, tebako_fs_stat((mount_point + "/hello.txt").c_str(), &expected_st));
  tebako_fs_unmount();

  // Mount the same image embedded at offset kJunkSize with explicit length
  ASSERT_EQ(0, tebako_fs_init_from_file_at(combined_path.c_str(), kJunkSize, static_cast<uint64_t>(image.size()),
                                           mount_point.c_str()));
  EXPECT_EQ(1, tebako_is_initialized());

  const char* bn = tebako_get_backend_name();
  ASSERT_NE(nullptr, bn);
  EXPECT_EQ("DwarFS", std::string(bn));

  // Reads from the offset mount must match the plain mount
  EXPECT_EQ(expected_hello, read_file_via_api(mount_point + "/hello.txt"));
  EXPECT_EQ(expected_test, read_file_via_api(mount_point + "/test.txt"));

  struct stat st;
  ASSERT_EQ(0, tebako_fs_stat((mount_point + "/hello.txt").c_str(), &st));
  EXPECT_EQ(expected_st.st_size, st.st_size);
}

TEST_F(CApiOffsetTest, OffsetMount_LengthZeroMeansToEndOfFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file_at(combined_path.c_str(), kJunkSize, 0, mount_point.c_str()));
  EXPECT_EQ(1, tebako_is_initialized());
  EXPECT_FALSE(read_file_via_api(mount_point + "/hello.txt").empty());
}

TEST_F(CApiOffsetTest, OffsetMount_ZeroOffsetExplicitLength)
{
  // Region path with offset == 0 but explicit length (no trailing data)
  ASSERT_EQ(0, tebako_fs_init_from_file_at(plain_image_path.c_str(), 0, static_cast<uint64_t>(image.size()),
                                           mount_point.c_str()));
  EXPECT_EQ(1, tebako_is_initialized());
  EXPECT_FALSE(read_file_via_api(mount_point + "/hello.txt").empty());
}

TEST_F(CApiOffsetTest, OffsetMount_WholeFileDelegationStillWorks)
{
  // tebako_fs_init_from_file delegates to _at(0, 0): zero-copy whole-file mount
  ASSERT_EQ(0, tebako_fs_init_from_file(plain_image_path.c_str(), mount_point.c_str()));
  EXPECT_EQ(1, tebako_is_initialized());

  const char* bn = tebako_get_backend_name();
  ASSERT_NE(nullptr, bn);
  EXPECT_EQ("DwarFS", std::string(bn));

  // Whole-file mount keeps the archive path (unlike region mounts)
  const char* ap = tebako_get_archive_path();
  ASSERT_NE(nullptr, ap);
  EXPECT_EQ(plain_image_path, std::string(ap));

  EXPECT_FALSE(read_file_via_api(mount_point + "/hello.txt").empty());
}

TEST_F(CApiOffsetTest, OffsetPastEnd_Fails)
{
  uint64_t file_size = fs::file_size(combined_path);
  EXPECT_EQ(-1, tebako_fs_init_from_file_at(combined_path.c_str(), file_size + 1, 0, mount_point.c_str()));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiOffsetTest, OffsetAtEndEmptyRegion_Fails)
{
  uint64_t file_size = fs::file_size(combined_path);
  EXPECT_EQ(-1, tebako_fs_init_from_file_at(combined_path.c_str(), file_size, 0, mount_point.c_str()));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiOffsetTest, LengthOverflow_Fails)
{
  // offset + length extends past end of file by one byte
  EXPECT_EQ(-1, tebako_fs_init_from_file_at(combined_path.c_str(), kJunkSize, static_cast<uint64_t>(image.size()) + 1,
                                            mount_point.c_str()));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiOffsetTest, NullPath_Fails)
{
  EXPECT_EQ(-1, tebako_fs_init_from_file_at(nullptr, kJunkSize, 0, mount_point.c_str()));
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiOffsetTest, NullMountPoint_Fails)
{
  EXPECT_EQ(-1, tebako_fs_init_from_file_at(combined_path.c_str(), kJunkSize, 0, nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiOffsetTest, NonexistentFile_Fails)
{
  EXPECT_EQ(-1, tebako_fs_init_from_file_at((test_dir + "/no_such_file.bin").c_str(), 0, 1, mount_point.c_str()));
  EXPECT_EQ(ENOENT, tebako_get_errno());
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiOffsetTest, DoubleInitStillFailsWithEEXIST)
{
  ASSERT_EQ(0, tebako_fs_init_from_file_at(combined_path.c_str(), kJunkSize, 0, mount_point.c_str()));
  EXPECT_EQ(-1, tebako_fs_init_from_file_at(combined_path.c_str(), kJunkSize, 0, mount_point.c_str()));
  EXPECT_EQ(EEXIST, tebako_get_errno());
}

// ============================================================
// Multi-Mount Tests
// (tebako_fs_mount_from_file / tebako_fs_mount_from_file_at /
//  tebako_fs_mount_from_memory / tebako_fs_unmount_handle)
// ============================================================

/**
 * @brief Test fixture for multi-mount C API tests
 *
 * Builds two ZIP archives with entries at the archive root:
 *  - Archive A: "content/alpha.txt" ("alpha-content") plus
 *    "nested/beta.txt" ("from-A") which deliberately shadows archive B's
 *    root file to prove longest-prefix dispatch
 *  - Archive B: "beta.txt" ("beta-content")
 *
 * NOTE: Shares the global C API state; serialized via RESOURCE_LOCK like the
 * other test_c_api tests.
 */
class CApiMultiMountTest : public ::testing::Test {
 protected:
  std::string test_dir;
  std::string archive_a_path;
  std::string archive_b_path;
  std::vector<uint8_t> archive_a;
  std::vector<uint8_t> archive_b;

  void SetUp() override
  {
    test_dir = (fs::temp_directory_path() / "tebako_c_api_mm_test").generic_string();
    fs::create_directories(test_dir);

    fs::path tree_a = fs::path(test_dir) / "tree_a";
    fs::create_directories(tree_a / "content");
    fs::create_directories(tree_a / "nested");
    write_file(tree_a / "content" / "alpha.txt", "alpha-content");
    write_file(tree_a / "nested" / "beta.txt", "from-A");

    fs::path tree_b = fs::path(test_dir) / "tree_b";
    fs::create_directories(tree_b);
    write_file(tree_b / "beta.txt", "beta-content");

    archive_a_path = test_dir + "/a.zip";
    archive_b_path = test_dir + "/b.zip";
    ASSERT_TRUE(tebako_test::create_zip_from_dir(archive_a_path, tree_a, "")) << "Failed to create archive A";
    ASSERT_TRUE(tebako_test::create_zip_from_dir(archive_b_path, tree_b, "")) << "Failed to create archive B";

    archive_a = read_bytes(archive_a_path);
    archive_b = read_bytes(archive_b_path);
    ASSERT_FALSE(archive_a.empty());
    ASSERT_FALSE(archive_b.empty());
  }

  void TearDown() override
  {
    tebako_fs_unmount();
    if (fs::exists(test_dir)) {
      fs::remove_all(test_dir);
    }
  }

  void write_file(const fs::path& path, const std::string& content)
  {
    std::ofstream ofs(path, std::ios::binary);
    ofs << content;
    ofs.close();
  }

  static std::vector<uint8_t> read_bytes(const std::string& path)
  {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
      return {};
    }
    auto size = ifs.tellg();
    std::vector<uint8_t> data(static_cast<size_t>(size));
    ifs.seekg(0);
    ifs.read(reinterpret_cast<char*>(data.data()), size);
    return data;
  }

  tebako_mount_t mount_a_from_memory(const std::string& mount_point)
  {
    tebako_mount_t handle = -1;
    EXPECT_EQ(0, tebako_fs_mount_from_memory(archive_a.data(), archive_a.size(), mount_point.c_str(), &handle));
    return handle;
  }

  tebako_mount_t mount_b_from_memory(const std::string& mount_point)
  {
    tebako_mount_t handle = -1;
    EXPECT_EQ(0, tebako_fs_mount_from_memory(archive_b.data(), archive_b.size(), mount_point.c_str(), &handle));
    return handle;
  }

  std::string read_file_via_api(const std::string& path)
  {
    int fd = tebako_fs_open(path.c_str(), O_RDONLY);
    if (fd < 0)
      return "";

    std::vector<char> buffer(4096);
    ssize_t n = tebako_fs_read(fd, buffer.data(), buffer.size());
    tebako_fs_close(fd);

    if (n <= 0)
      return "";
    return std::string(buffer.data(), n);
  }
};

TEST_F(CApiMultiMountTest, MountTwoFromMemory_ReadFromBoth)
{
  const std::string mp_a = "/__mm_a__";
  const std::string mp_b = "/__mm_b__";

  tebako_mount_t ha = mount_a_from_memory(mp_a);
  tebako_mount_t hb = mount_b_from_memory(mp_b);
  ASSERT_GE(ha, 0);
  ASSERT_GE(hb, 0);
  EXPECT_NE(ha, hb);
  EXPECT_EQ(1, tebako_is_initialized());

  EXPECT_EQ("alpha-content", read_file_via_api(mp_a + "/content/alpha.txt"));
  EXPECT_EQ("beta-content", read_file_via_api(mp_b + "/beta.txt"));

  EXPECT_EQ(0, tebako_fs_unmount_handle(ha));
  EXPECT_EQ(1, tebako_is_initialized());  // B still mounted
  EXPECT_EQ(0, tebako_fs_unmount_handle(hb));
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiMultiMountTest, MountFromFile_ReadFromBoth)
{
  tebako_mount_t ha = -1;
  tebako_mount_t hb = -1;
  ASSERT_EQ(0, tebako_fs_mount_from_file(archive_a_path.c_str(), "/__mm_fa__", &ha));
  ASSERT_EQ(0, tebako_fs_mount_from_file(archive_b_path.c_str(), "/__mm_fb__", &hb));
  EXPECT_GE(ha, 0);
  EXPECT_GE(hb, 0);

  EXPECT_EQ("alpha-content", read_file_via_api("/__mm_fa__/content/alpha.txt"));
  EXPECT_EQ("beta-content", read_file_via_api("/__mm_fb__/beta.txt"));
}

TEST_F(CApiMultiMountTest, MountFromFileAt_MixedWithMemoryMount)
{
  // Combined file: junk prefix + archive A bytes
  const std::string combined_path = test_dir + "/combined.bin";
  constexpr uint64_t junk_size = 1000;
  {
    std::ofstream ofs(combined_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(ofs.is_open());
    for (uint64_t i = 0; i < junk_size; ++i) {
      char c = static_cast<char>('A' + (i * 7) % 26);
      ofs.write(&c, 1);
    }
    ofs.write(reinterpret_cast<const char*>(archive_a.data()), static_cast<std::streamsize>(archive_a.size()));
  }

  tebako_mount_t ha = -1;
  ASSERT_EQ(0, tebako_fs_mount_from_file_at(combined_path.c_str(), junk_size, static_cast<uint64_t>(archive_a.size()),
                                            "/__mm_at__", &ha));
  tebako_mount_t hb = mount_b_from_memory("/__mm_mem__");
  ASSERT_GE(hb, 0);

  EXPECT_EQ("alpha-content", read_file_via_api("/__mm_at__/content/alpha.txt"));
  EXPECT_EQ("beta-content", read_file_via_api("/__mm_mem__/beta.txt"));
}

TEST_F(CApiMultiMountTest, LongestPrefix_NestedMountsDispatch)
{
  const std::string mp_outer = "/__mm__";
  const std::string mp_nested = "/__mm__/nested";

  ASSERT_GE(mount_a_from_memory(mp_outer), 0);
  ASSERT_GE(mount_b_from_memory(mp_nested), 0);

  // Outer mount still serves its own subtree
  EXPECT_EQ("alpha-content", read_file_via_api(mp_outer + "/content/alpha.txt"));

  // The nested mount owns the /__mm__/nested prefix: archive B's "beta.txt"
  // must win over archive A's shadowing "nested/beta.txt" ("from-A")
  EXPECT_EQ("beta-content", read_file_via_api(mp_nested + "/beta.txt"));

  struct stat st;
  ASSERT_EQ(0, tebako_fs_stat((mp_nested + "/beta.txt").c_str(), &st));
  EXPECT_EQ(static_cast<off_t>(std::strlen("beta-content")), st.st_size);
}

TEST_F(CApiMultiMountTest, DuplicateMountPoint_FailsWithEEXIST)
{
  const std::string mp = "/__mm_dup__";
  ASSERT_GE(mount_a_from_memory(mp), 0);

  tebako_mount_t h = -1;
  EXPECT_EQ(-1, tebako_fs_mount_from_memory(archive_b.data(), archive_b.size(), mp.c_str(), &h));
  EXPECT_EQ(EEXIST, tebako_get_errno());

  EXPECT_EQ(-1, tebako_fs_mount_from_file(archive_b_path.c_str(), mp.c_str(), &h));
  EXPECT_EQ(EEXIST, tebako_get_errno());

  // Original mount is still fully usable
  EXPECT_EQ("alpha-content", read_file_via_api(mp + "/content/alpha.txt"));
}

TEST_F(CApiMultiMountTest, Mount_BadArguments)
{
  tebako_mount_t h = -1;

  // NULL out_handle
  EXPECT_EQ(-1, tebako_fs_mount_from_memory(archive_a.data(), archive_a.size(), "/__mm_x__", nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_mount_from_file(archive_a_path.c_str(), "/__mm_x__", nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_mount_from_file_at(archive_a_path.c_str(), 0, 1, "/__mm_x__", nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  // NULL / empty mount point
  EXPECT_EQ(-1, tebako_fs_mount_from_memory(archive_a.data(), archive_a.size(), nullptr, &h));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_mount_from_memory(archive_a.data(), archive_a.size(), "", &h));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_mount_from_file(archive_a_path.c_str(), "", &h));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  // NULL data / zero size
  EXPECT_EQ(-1, tebako_fs_mount_from_memory(nullptr, 100, "/__mm_x__", &h));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_mount_from_memory(archive_a.data(), 0, "/__mm_x__", &h));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  // NULL archive path
  EXPECT_EQ(-1, tebako_fs_mount_from_file(nullptr, "/__mm_x__", &h));
  EXPECT_EQ(EINVAL, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_mount_from_file_at(nullptr, 0, 0, "/__mm_x__", &h));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiMultiMountTest, UnmountHandle_UnknownHandle_ENODEV)
{
  EXPECT_EQ(-1, tebako_fs_unmount_handle(999));
  EXPECT_EQ(ENODEV, tebako_get_errno());

  ASSERT_GE(mount_a_from_memory("/__mm_u__"), 0);
  EXPECT_EQ(-1, tebako_fs_unmount_handle(999));
  EXPECT_EQ(ENODEV, tebako_get_errno());
}

TEST_F(CApiMultiMountTest, UnmountHandle_ForceClosesOnlyOwnFdsAndDirs)
{
  const std::string mp_a = "/__mm_iso_a__";
  const std::string mp_b = "/__mm_iso_b__";
  tebako_mount_t ha = mount_a_from_memory(mp_a);
  tebako_mount_t hb = mount_b_from_memory(mp_b);
  ASSERT_GE(ha, 0);
  ASSERT_GE(hb, 0);

  int fd_a = tebako_fs_open((mp_a + "/content/alpha.txt").c_str(), O_RDONLY);
  int fd_b = tebako_fs_open((mp_b + "/beta.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd_a, 0);
  ASSERT_GT(fd_b, 0);

  tebako_dir_t dir_a = tebako_fs_opendir((mp_a + "/content").c_str());
  tebako_dir_t dir_b = tebako_fs_opendir(mp_b.c_str());
  ASSERT_NE(nullptr, dir_a);
  ASSERT_NE(nullptr, dir_b);

  ASSERT_EQ(0, tebako_fs_unmount_handle(ha));

  // A's handles are force-closed
  char buf[16];
  EXPECT_EQ(-1, tebako_fs_read(fd_a, buf, sizeof(buf)));
  EXPECT_EQ(EBADF, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_close(fd_a));
  EXPECT_EQ(EBADF, tebako_get_errno());
  EXPECT_EQ(nullptr, tebako_fs_readdir(dir_a));
  EXPECT_EQ(EBADF, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_closedir(dir_a));
  EXPECT_EQ(EBADF, tebako_get_errno());

  // B is unaffected
  std::vector<char> rbuf(64);
  ssize_t n = tebako_fs_read(fd_b, rbuf.data(), rbuf.size());
  ASSERT_GT(n, 0);
  EXPECT_EQ("beta-content", std::string(rbuf.data(), static_cast<size_t>(n)));
  struct tebako_c_dirent* entry = tebako_fs_readdir(dir_b);
  ASSERT_NE(nullptr, entry);
  EXPECT_STREQ("beta.txt", entry->d_name);

  // New operations on B still work
  EXPECT_EQ("beta-content", read_file_via_api(mp_b + "/beta.txt"));
  EXPECT_EQ(1, tebako_is_initialized());

  EXPECT_EQ(0, tebako_fs_close(fd_b));
  EXPECT_EQ(0, tebako_fs_closedir(dir_b));
  EXPECT_EQ(0, tebako_fs_unmount_handle(hb));
  EXPECT_EQ(0, tebako_is_initialized());
}

TEST_F(CApiMultiMountTest, UnmountHandle_AllowsRemount_HandlesNotReused)
{
  const std::string mp = "/__mm_re__";
  tebako_mount_t h1 = mount_a_from_memory(mp);
  ASSERT_GE(h1, 0);
  ASSERT_EQ(0, tebako_fs_unmount_handle(h1));

  // The mount point is free again; the handle is not reused
  tebako_mount_t h2 = mount_b_from_memory(mp);
  ASSERT_GE(h2, 0);
  EXPECT_GT(h2, h1);
  EXPECT_EQ("beta-content", read_file_via_api(mp + "/beta.txt"));
}

TEST_F(CApiMultiMountTest, Compat_InitFailsWhenMountsExist)
{
  ASSERT_GE(mount_a_from_memory("/__mm_c__"), 0);

  // init* stays single-mount: any existing mount makes it fail
  EXPECT_EQ(-1, tebako_fs_init_from_file(archive_b_path.c_str(), "/__mm_other__"));
  EXPECT_EQ(EEXIST, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_init(archive_b.data(), archive_b.size(), "/__mm_other__"));
  EXPECT_EQ(EEXIST, tebako_get_errno());

  EXPECT_EQ("alpha-content", read_file_via_api("/__mm_c__/content/alpha.txt"));
}

TEST_F(CApiMultiMountTest, Compat_MountAfterInit_GettersUnaffected)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_a_path.c_str(), "/__mm_compat__"));

  tebako_mount_t hb = mount_b_from_memory("/__mm_extra__");
  ASSERT_GE(hb, 0);
  EXPECT_EQ(1, tebako_is_initialized());

  // Compat getters still report the init* mount
  const char* mp = tebako_get_mount_point();
  ASSERT_NE(nullptr, mp);
  EXPECT_STREQ("/__mm_compat__", mp);
  const char* bn = tebako_get_backend_name();
  ASSERT_NE(nullptr, bn);
  EXPECT_STREQ("ZIP", bn);
  const char* ap = tebako_get_archive_path();
  ASSERT_NE(nullptr, ap);
  EXPECT_STREQ(archive_a_path.c_str(), ap);

  // Both mounts readable
  EXPECT_EQ("alpha-content", read_file_via_api("/__mm_compat__/content/alpha.txt"));
  EXPECT_EQ("beta-content", read_file_via_api("/__mm_extra__/beta.txt"));

  // tebako_fs_unmount() tears down everything
  tebako_fs_unmount();
  EXPECT_EQ(0, tebako_is_initialized());
  EXPECT_EQ(nullptr, tebako_get_mount_point());
}

TEST_F(CApiMultiMountTest, Compat_UnmountAllForceClosesAllMounts)
{
  const std::string mp_a = "/__mm_ua__";
  const std::string mp_b = "/__mm_ub__";
  ASSERT_GE(mount_a_from_memory(mp_a), 0);
  ASSERT_GE(mount_b_from_memory(mp_b), 0);

  int fd_a = tebako_fs_open((mp_a + "/content/alpha.txt").c_str(), O_RDONLY);
  int fd_b = tebako_fs_open((mp_b + "/beta.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd_a, 0);
  ASSERT_GT(fd_b, 0);

  tebako_fs_unmount();
  EXPECT_EQ(0, tebako_is_initialized());

  char buf[16];
  EXPECT_EQ(-1, tebako_fs_read(fd_a, buf, sizeof(buf)));
  EXPECT_EQ(EBADF, tebako_get_errno());
  EXPECT_EQ(-1, tebako_fs_read(fd_b, buf, sizeof(buf)));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiMultiMountTest, PathIsEmbedded_MultiMounts)
{
  ASSERT_GE(mount_a_from_memory("/__mm_pa__"), 0);
  ASSERT_GE(mount_b_from_memory("/__mm_pb__"), 0);

  EXPECT_EQ(1, tebako_path_is_embedded("/__mm_pa__/content/alpha.txt"));
  EXPECT_EQ(1, tebako_path_is_embedded("/__mm_pb__/beta.txt"));
  EXPECT_EQ(1, tebako_path_is_embedded("/__mm_pb__"));
  EXPECT_EQ(0, tebako_path_is_embedded("/__mm_other__/x"));
  EXPECT_EQ(0, tebako_path_is_embedded("/tmp/file.txt"));
}

TEST_F(CApiMultiMountTest, ExtractAll_MultiMount_Subtrees)
{
  ASSERT_GE(mount_a_from_memory("/__mm_xa__"), 0);
  ASSERT_GE(mount_b_from_memory("/__mm_xb__"), 0);

  std::string dest = test_dir + "/extracted";
  fs::create_directories(dest);
  ASSERT_EQ(0, tebako_fs_extract_all(dest.c_str()));

  // Each mount is extracted under its own mount-point-basename subtree
  std::ifstream ifs_a(fs::path(dest) / "__mm_xa__" / "content" / "alpha.txt");
  ASSERT_TRUE(ifs_a.is_open());
  std::string content_a((std::istreambuf_iterator<char>(ifs_a)), std::istreambuf_iterator<char>());
  EXPECT_EQ("alpha-content", content_a);

  std::ifstream ifs_b(fs::path(dest) / "__mm_xb__" / "beta.txt");
  ASSERT_TRUE(ifs_b.is_open());
  std::string content_b((std::istreambuf_iterator<char>(ifs_b)), std::istreambuf_iterator<char>());
  EXPECT_EQ("beta-content", content_b);
}

// ============================================================
// tebako_fs_pread Tests
// ============================================================

TEST_F(CApiTest, Pread_ReadsAtOffset_LeavesPositionIntact)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  // "Hello, World!" — 5 bytes at offset 7 is "World"
  char buffer[16] = {0};
  ssize_t n = tebako_fs_pread(fd, buffer, 5, 7);
  EXPECT_EQ(5, n);
  EXPECT_STREQ("World", buffer);

  // The fd position must be untouched: a plain read starts at offset 0
  char full[32] = {0};
  n = tebako_fs_read(fd, full, sizeof(full) - 1);
  EXPECT_EQ(13, n);
  EXPECT_STREQ("Hello, World!", full);

  EXPECT_EQ(0, tebako_fs_close(fd));
}

TEST_F(CApiTest, Pread_AfterSeek_DoesNotMovePosition)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  ASSERT_EQ(3, tebako_fs_lseek(fd, 3, SEEK_SET));

  char buffer[16] = {0};
  ssize_t n = tebako_fs_pread(fd, buffer, 5, 7);
  EXPECT_EQ(5, n);
  EXPECT_STREQ("World", buffer);

  // Plain read must resume from the seek position (3), not from 7+5
  char rest[32] = {0};
  n = tebako_fs_read(fd, rest, sizeof(rest) - 1);
  EXPECT_EQ(10, n);
  EXPECT_STREQ("lo, World!", rest);

  EXPECT_EQ(0, tebako_fs_close(fd));
}

TEST_F(CApiTest, Pread_OffsetBeyondEof_ReturnsZero)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  char buffer[16];
  EXPECT_EQ(0, tebako_fs_pread(fd, buffer, sizeof(buffer), 13));  // at EOF
  EXPECT_EQ(0, tebako_fs_pread(fd, buffer, sizeof(buffer), 4096));

  // Position still intact
  char full[32] = {0};
  ssize_t n = tebako_fs_read(fd, full, sizeof(full) - 1);
  EXPECT_EQ(13, n);
  EXPECT_STREQ("Hello, World!", full);

  EXPECT_EQ(0, tebako_fs_close(fd));
}

TEST_F(CApiTest, Pread_InvalidFd)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  char buffer[16];
  EXPECT_EQ(-1, tebako_fs_pread(999, buffer, sizeof(buffer), 0));
  EXPECT_EQ(EBADF, tebako_get_errno());

  // Host fds are not libtfs fds even with a valid buffer
  EXPECT_EQ(-1, tebako_fs_pread(0, buffer, sizeof(buffer), 0));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Pread_NullBuffer)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  EXPECT_EQ(-1, tebako_fs_pread(fd, nullptr, 16, 0));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  EXPECT_EQ(0, tebako_fs_close(fd));
}

TEST_F(CApiTest, Pread_NegativeOffset)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_fs_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  char buffer[16];
  EXPECT_EQ(-1, tebako_fs_pread(fd, buffer, sizeof(buffer), -1));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  EXPECT_EQ(0, tebako_fs_close(fd));
}

// ============================================================
// Directory handle introspection / positioning Tests
// ============================================================

TEST_F(CApiTest, DirIsEmbedded_Handles)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);
  EXPECT_EQ(1, tebako_fs_dir_is_embedded(dir));

  ASSERT_EQ(0, tebako_fs_closedir(dir));
  // Closed handle is no longer in the registry
  EXPECT_EQ(0, tebako_fs_dir_is_embedded(dir));
}

TEST_F(CApiTest, DirIsEmbedded_NullAndUnknown)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(0, tebako_fs_dir_is_embedded(nullptr));
  // Wild pointer that was never registered: membership test must not crash
  EXPECT_EQ(0, tebako_fs_dir_is_embedded(reinterpret_cast<tebako_dir_t>(static_cast<uintptr_t>(0xdeadbeef))));
}

TEST_F(CApiTest, Telldir_TracksReadPosition)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  // Fresh stream: next entry ordinal is 0
  EXPECT_EQ(0, tebako_fs_telldir(dir));

  long expected = 0;
  struct tebako_c_dirent* entry;
  while ((entry = tebako_fs_readdir(dir)) != nullptr) {
    expected++;
    EXPECT_EQ(expected, tebako_fs_telldir(dir));
  }
  EXPECT_GT(expected, 0);

  tebako_fs_closedir(dir);
}

TEST_F(CApiTest, Telldir_InvalidHandle)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(-1, tebako_fs_telldir(nullptr));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Rewinddir_ResetsStream)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  std::vector<std::string> first_pass;
  struct tebako_c_dirent* entry;
  while ((entry = tebako_fs_readdir(dir)) != nullptr) {
    first_pass.push_back(entry->d_name);
  }
  ASSERT_FALSE(first_pass.empty());

  tebako_fs_rewinddir(dir);
  EXPECT_EQ(0, tebako_fs_telldir(dir));

  std::vector<std::string> second_pass;
  while ((entry = tebako_fs_readdir(dir)) != nullptr) {
    second_pass.push_back(entry->d_name);
  }
  EXPECT_EQ(first_pass, second_pass);

  tebako_fs_closedir(dir);
}

TEST_F(CApiTest, Rewinddir_InvalidHandle)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_fs_rewinddir(nullptr);  // no crash; errno reports EBADF
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Seekdir_RoundTripsCookie)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  struct tebako_c_dirent* entry = tebako_fs_readdir(dir);
  ASSERT_NE(nullptr, entry);
  const std::string name0 = entry->d_name;

  const long cookie = tebako_fs_telldir(dir);  // ordinal of the next entry
  EXPECT_EQ(1, cookie);

  entry = tebako_fs_readdir(dir);
  ASSERT_NE(nullptr, entry);
  const std::string name1 = entry->d_name;

  // Seek back to the saved cookie: the same entry must come again
  tebako_fs_seekdir(dir, cookie);
  EXPECT_EQ(cookie, tebako_fs_telldir(dir));
  entry = tebako_fs_readdir(dir);
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(name1, entry->d_name);

  // seekdir(dir, 0) is a rewind
  tebako_fs_seekdir(dir, 0);
  EXPECT_EQ(0, tebako_fs_telldir(dir));
  entry = tebako_fs_readdir(dir);
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(name0, entry->d_name);

  tebako_fs_closedir(dir);
}

TEST_F(CApiTest, Seekdir_InvalidHandleAndNegativePos)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_fs_seekdir(nullptr, 0);  // no crash; errno reports EBADF
  EXPECT_EQ(EBADF, tebako_get_errno());

  tebako_dir_t dir = tebako_fs_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  tebako_fs_seekdir(dir, -1);
  EXPECT_EQ(EINVAL, tebako_get_errno());
  // Failed seek leaves the stream where it was
  EXPECT_EQ(0, tebako_fs_telldir(dir));

  tebako_fs_closedir(dir);
}

// ============================================================
// tebako_fs_dlmap2file Tests
// ============================================================

TEST_F(CApiTest, Dlmap2file_ExtractsReadableHostFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  char* host_path = tebako_fs_dlmap2file((mount_point + "/content/hello.txt").c_str());
  ASSERT_NE(nullptr, host_path);
  EXPECT_EQ(0, tebako_get_errno());

  // The mapped file lives on the host and carries the memfs content
  ASSERT_TRUE(fs::exists(host_path));
  std::ifstream ifs(host_path, std::ios::binary);
  ASSERT_TRUE(ifs.is_open());
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  EXPECT_EQ("Hello, World!", content);

  free(host_path);
}

TEST_F(CApiTest, Dlmap2file_CachesExtraction)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  const std::string path = mount_point + "/content/data.bin";
  char* first = tebako_fs_dlmap2file(path.c_str());
  ASSERT_NE(nullptr, first);
  char* second = tebako_fs_dlmap2file(path.c_str());
  ASSERT_NE(nullptr, second);

  // Same memfs path maps to the same host file (separate string copies)
  EXPECT_STREQ(first, second);
  EXPECT_EQ(static_cast<uintmax_t>(1024), fs::file_size(first));

  free(first);
  free(second);
}

TEST_F(CApiTest, Dlmap2file_NotEmbedded)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(nullptr, tebako_fs_dlmap2file("/tmp/not-embedded.txt"));
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, Dlmap2file_NullPath)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(nullptr, tebako_fs_dlmap2file(nullptr));
}

TEST_F(CApiTest, Dlmap2file_MissingInMount)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(nullptr, tebako_fs_dlmap2file((mount_point + "/content/nonexistent.txt").c_str()));
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

// ============================================================
// Multi-mount interplay: operations dispatch to the owning mount
// ============================================================

TEST_F(CApiMultiMountTest, Pread_DispatchesToOwningMount)
{
  const std::string mp_a = "/__mm_pa__";
  const std::string mp_b = "/__mm_pb__";
  ASSERT_GE(mount_a_from_memory(mp_a), 0);
  ASSERT_GE(mount_b_from_memory(mp_b), 0);

  int fd_a = tebako_fs_open((mp_a + "/content/alpha.txt").c_str(), O_RDONLY);
  int fd_b = tebako_fs_open((mp_b + "/beta.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd_a, 0);
  ASSERT_GT(fd_b, 0);

  // "alpha-content": 7 bytes at offset 6 is "content"
  char buf_a[16] = {0};
  EXPECT_EQ(7, tebako_fs_pread(fd_a, buf_a, 7, 6));
  EXPECT_STREQ("content", buf_a);

  // "beta-content": 4 bytes at offset 0 is "beta"
  char buf_b[16] = {0};
  EXPECT_EQ(4, tebako_fs_pread(fd_b, buf_b, 4, 0));
  EXPECT_STREQ("beta", buf_b);

  // Both fd positions are untouched
  char rest_a[32] = {0};
  EXPECT_EQ(13, tebako_fs_read(fd_a, rest_a, sizeof(rest_a) - 1));
  EXPECT_STREQ("alpha-content", rest_a);
  char rest_b[32] = {0};
  EXPECT_EQ(12, tebako_fs_read(fd_b, rest_b, sizeof(rest_b) - 1));
  EXPECT_STREQ("beta-content", rest_b);

  EXPECT_EQ(0, tebako_fs_close(fd_a));
  EXPECT_EQ(0, tebako_fs_close(fd_b));
}

TEST_F(CApiMultiMountTest, DirPositioning_IndependentPerMount)
{
  const std::string mp_a = "/__mm_dpa__";
  const std::string mp_b = "/__mm_dpb__";
  ASSERT_GE(mount_a_from_memory(mp_a), 0);
  ASSERT_GE(mount_b_from_memory(mp_b), 0);

  tebako_dir_t dir_a = tebako_fs_opendir((mp_a + "/content").c_str());
  tebako_dir_t dir_b = tebako_fs_opendir(mp_b.c_str());
  ASSERT_NE(nullptr, dir_a);
  ASSERT_NE(nullptr, dir_b);

  struct tebako_c_dirent* entry = tebako_fs_readdir(dir_a);
  ASSERT_NE(nullptr, entry);
  EXPECT_STREQ("alpha.txt", entry->d_name);
  EXPECT_EQ(1, tebako_fs_telldir(dir_a));
  // Positioning on A does not disturb B's stream
  EXPECT_EQ(0, tebako_fs_telldir(dir_b));

  tebako_fs_seekdir(dir_a, 0);
  EXPECT_EQ(0, tebako_fs_telldir(dir_a));
  entry = tebako_fs_readdir(dir_a);
  ASSERT_NE(nullptr, entry);
  EXPECT_STREQ("alpha.txt", entry->d_name);

  // B still serves its own first entry
  entry = tebako_fs_readdir(dir_b);
  ASSERT_NE(nullptr, entry);
  EXPECT_STREQ("beta.txt", entry->d_name);

  tebako_fs_rewinddir(dir_b);
  EXPECT_EQ(0, tebako_fs_telldir(dir_b));
  entry = tebako_fs_readdir(dir_b);
  ASSERT_NE(nullptr, entry);
  EXPECT_STREQ("beta.txt", entry->d_name);

  EXPECT_EQ(0, tebako_fs_closedir(dir_a));
  EXPECT_EQ(0, tebako_fs_closedir(dir_b));
}

TEST_F(CApiMultiMountTest, DirIsEmbedded_PerMountRegistry)
{
  const std::string mp_a = "/__mm_dia__";
  const std::string mp_b = "/__mm_dib__";
  tebako_mount_t ha = mount_a_from_memory(mp_a);
  tebako_mount_t hb = mount_b_from_memory(mp_b);
  ASSERT_GE(ha, 0);
  ASSERT_GE(hb, 0);

  tebako_dir_t dir_a = tebako_fs_opendir((mp_a + "/content").c_str());
  tebako_dir_t dir_b = tebako_fs_opendir(mp_b.c_str());
  ASSERT_NE(nullptr, dir_a);
  ASSERT_NE(nullptr, dir_b);
  EXPECT_EQ(1, tebako_fs_dir_is_embedded(dir_a));
  EXPECT_EQ(1, tebako_fs_dir_is_embedded(dir_b));

  // Unmounting A force-closes only A's dir handles
  ASSERT_EQ(0, tebako_fs_unmount_handle(ha));
  EXPECT_EQ(0, tebako_fs_dir_is_embedded(dir_a));
  EXPECT_EQ(1, tebako_fs_dir_is_embedded(dir_b));

  EXPECT_EQ(0, tebako_fs_closedir(dir_b));
  EXPECT_EQ(0, tebako_fs_unmount_handle(hb));
}

TEST_F(CApiMultiMountTest, Dlmap2file_MultiMount_DistinctExtractions)
{
  const std::string mp_a = "/__mm_dla__";
  const std::string mp_b = "/__mm_dlb__";
  ASSERT_GE(mount_a_from_memory(mp_a), 0);
  ASSERT_GE(mount_b_from_memory(mp_b), 0);

  // Same basename "beta.txt" in both mounts, different content
  char* host_a = tebako_fs_dlmap2file((mp_a + "/nested/beta.txt").c_str());
  char* host_b = tebako_fs_dlmap2file((mp_b + "/beta.txt").c_str());
  ASSERT_NE(nullptr, host_a);
  ASSERT_NE(nullptr, host_b);
  EXPECT_STRNE(host_a, host_b);

  std::ifstream ifs_a(host_a, std::ios::binary);
  ASSERT_TRUE(ifs_a.is_open());
  std::string content_a((std::istreambuf_iterator<char>(ifs_a)), std::istreambuf_iterator<char>());
  EXPECT_EQ("from-A", content_a);

  std::ifstream ifs_b(host_b, std::ios::binary);
  ASSERT_TRUE(ifs_b.is_open());
  std::string content_b((std::istreambuf_iterator<char>(ifs_b)), std::istreambuf_iterator<char>());
  EXPECT_EQ("beta-content", content_b);

  free(host_a);
  free(host_b);
}
