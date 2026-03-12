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

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include <fcntl.h>
#include <unistd.h>

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
    test_dir = fs::temp_directory_path() / "tebako_c_api_test";
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

    // Create ZIP archive using system command
    archive_path = test_dir + "/test.zip";
    std::string cmd = "cd " + test_dir + " && zip -r test.zip content/ >/dev/null 2>&1";
    int result = std::system(cmd.c_str());
    ASSERT_EQ(0, result) << "Failed to create test ZIP archive";
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
    int fd = tebako_open(path.c_str(), O_RDONLY);
    if (fd < 0)
      return "";

    std::vector<char> buffer(4096);
    ssize_t n = tebako_read(fd, buffer.data(), buffer.size());
    tebako_close(fd);

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

  EXPECT_EQ(-1, tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY));
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

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  EXPECT_GT(fd, 0);
  EXPECT_TRUE(tebako_fd_is_embedded(fd));
  EXPECT_EQ(0, tebako_get_errno());

  EXPECT_EQ(0, tebako_close(fd));
}

TEST_F(CApiTest, Open_NonexistentFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/nonexistent.txt").c_str(), O_RDONLY);
  EXPECT_EQ(-1, fd);
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, Open_NullPath)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open(nullptr, O_RDONLY);
  EXPECT_EQ(-1, fd);
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiTest, Open_WriteMode_Fails)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_WRONLY);
  EXPECT_EQ(-1, fd);
  EXPECT_EQ(EROFS, tebako_get_errno());
}

TEST_F(CApiTest, Read_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  char buffer[100] = {0};
  ssize_t n = tebako_read(fd, buffer, sizeof(buffer));
  EXPECT_GT(n, 0);
  EXPECT_STREQ("Hello, World!", buffer);

  tebako_close(fd);
}

TEST_F(CApiTest, Read_EmptyFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/empty.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  char buffer[10];
  ssize_t n = tebako_read(fd, buffer, sizeof(buffer));
  EXPECT_EQ(0, n);  // EOF immediately

  tebako_close(fd);
}

TEST_F(CApiTest, Read_InvalidFd)
{
  char buffer[10];
  ssize_t n = tebako_read(999, buffer, sizeof(buffer));
  EXPECT_EQ(-1, n);
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Read_NullBuffer)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  ssize_t n = tebako_read(fd, nullptr, 10);
  EXPECT_EQ(-1, n);
  EXPECT_EQ(EINVAL, tebako_get_errno());

  tebako_close(fd);
}

TEST_F(CApiTest, Lseek_SeekSet)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  // Seek to position 7
  off_t pos = tebako_lseek(fd, 7, SEEK_SET);
  EXPECT_EQ(7, pos);

  // Read from new position
  char buffer[10] = {0};
  ssize_t n = tebako_read(fd, buffer, 5);
  EXPECT_EQ(5, n);
  EXPECT_STREQ("World", buffer);

  tebako_close(fd);
}

TEST_F(CApiTest, Lseek_SeekCur)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  // Read 5 bytes
  char buffer[5];
  tebako_read(fd, buffer, 5);

  // Seek forward 2 bytes from current
  off_t pos = tebako_lseek(fd, 2, SEEK_CUR);
  EXPECT_EQ(7, pos);

  tebako_close(fd);
}

TEST_F(CApiTest, Lseek_SeekEnd)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  // Seek to end
  off_t size = tebako_lseek(fd, 0, SEEK_END);
  EXPECT_EQ(13, size);  // strlen("Hello, World!")

  // Try to read (should get EOF)
  char buffer[10];
  ssize_t n = tebako_read(fd, buffer, sizeof(buffer));
  EXPECT_EQ(0, n);

  tebako_close(fd);
}

TEST_F(CApiTest, Lseek_InvalidFd)
{
  off_t pos = tebako_lseek(999, 0, SEEK_SET);
  EXPECT_EQ(-1, pos);
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Close_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  EXPECT_EQ(0, tebako_close(fd));

  // Further operations should fail
  char buffer[10];
  EXPECT_EQ(-1, tebako_read(fd, buffer, sizeof(buffer)));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Close_InvalidFd)
{
  EXPECT_EQ(-1, tebako_close(999));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, MultipleFds_Independent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd1 = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  int fd2 = tebako_open((mount_point + "/content/data.bin").c_str(), O_RDONLY);

  ASSERT_GT(fd1, 0);
  ASSERT_GT(fd2, 0);
  EXPECT_NE(fd1, fd2);

  // Both should work independently
  char buf1[10], buf2[10];
  EXPECT_GT(tebako_read(fd1, buf1, sizeof(buf1)), 0);
  EXPECT_GT(tebako_read(fd2, buf2, sizeof(buf2)), 0);

  tebako_close(fd1);
  tebako_close(fd2);
}

// ============================================================
// Directory Operations Tests (10 tests)
// ============================================================

TEST_F(CApiTest, Opendir_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);
  EXPECT_EQ(0, tebako_get_errno());

  EXPECT_EQ(0, tebako_closedir(dir));
}

TEST_F(CApiTest, Opendir_Nonexistent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_opendir((mount_point + "/nonexistent").c_str());
  EXPECT_EQ(nullptr, dir);
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, Opendir_NullPath)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_opendir(nullptr);
  EXPECT_EQ(nullptr, dir);
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiTest, Readdir_ListFiles)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  std::vector<std::string> entries;
  struct tebako_c_dirent* entry;
  while ((entry = tebako_readdir(dir)) != nullptr) {
    entries.push_back(entry->d_name);
  }

  // Should have at least our test files
  EXPECT_FALSE(entries.empty());

  // Check for known files
  auto has_hello = std::find(entries.begin(), entries.end(), "hello.txt") != entries.end();
  auto has_data = std::find(entries.begin(), entries.end(), "data.bin") != entries.end();
  auto has_subdir = std::find(entries.begin(), entries.end(), "subdir") != entries.end();

  EXPECT_TRUE(has_hello || has_data || has_subdir) << "Expected test files not found";

  tebako_closedir(dir);
}

TEST_F(CApiTest, Readdir_CheckTypes)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  bool found_file = false;
  bool found_dir = false;

  struct tebako_c_dirent* entry;
  while ((entry = tebako_readdir(dir)) != nullptr) {
    if (entry->d_type == DT_REG)
      found_file = true;
    if (entry->d_type == DT_DIR)
      found_dir = true;
  }

  EXPECT_TRUE(found_file) << "Should find at least one regular file";

  tebako_closedir(dir);
}

TEST_F(CApiTest, Readdir_EmptyAtEnd)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  // Read all entries
  while (tebako_readdir(dir) != nullptr) {
  }

  // Next call should return nullptr
  EXPECT_EQ(nullptr, tebako_readdir(dir));

  tebako_closedir(dir);
}

TEST_F(CApiTest, Readdir_InvalidHandle)
{
  struct tebako_c_dirent* entry = tebako_readdir(nullptr);
  EXPECT_EQ(nullptr, entry);
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Closedir_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir = tebako_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  EXPECT_EQ(0, tebako_closedir(dir));
  EXPECT_EQ(0, tebako_get_errno());
}

TEST_F(CApiTest, Closedir_InvalidHandle)
{
  EXPECT_EQ(-1, tebako_closedir(nullptr));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, MultipleDirs_Independent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  tebako_dir_t dir1 = tebako_opendir((mount_point + "/content").c_str());
  tebako_dir_t dir2 = tebako_opendir((mount_point + "/content/subdir").c_str());

  ASSERT_NE(nullptr, dir1);
  ASSERT_NE(nullptr, dir2);
  EXPECT_NE(dir1, dir2);

  // Both should work
  EXPECT_NE(nullptr, tebako_readdir(dir1));

  tebako_closedir(dir1);
  tebako_closedir(dir2);
}

// ============================================================
// Metadata Operations Tests (7 tests)
// ============================================================

TEST_F(CApiTest, Stat_RegularFile)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  struct stat st;
  int result = tebako_stat((mount_point + "/content/hello.txt").c_str(), &st);

  EXPECT_EQ(0, result);
  EXPECT_TRUE(S_ISREG(st.st_mode));
  EXPECT_EQ(13, st.st_size);  // strlen("Hello, World!")
}

TEST_F(CApiTest, Stat_Directory)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  struct stat st;
  int result = tebako_stat((mount_point + "/content/subdir").c_str(), &st);

  EXPECT_EQ(0, result);
  EXPECT_TRUE(S_ISDIR(st.st_mode));
}

TEST_F(CApiTest, Stat_Nonexistent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  struct stat st;
  int result = tebako_stat((mount_point + "/nonexistent").c_str(), &st);

  EXPECT_EQ(-1, result);
  EXPECT_EQ(ENOENT, tebako_get_errno());
}

TEST_F(CApiTest, Stat_NullArguments)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  struct stat st;
  EXPECT_EQ(-1, tebako_stat(nullptr, &st));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  EXPECT_EQ(-1, tebako_stat((mount_point + "/content/hello.txt").c_str(), nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(CApiTest, Fstat_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  struct stat st;
  EXPECT_EQ(0, tebako_fstat(fd, &st));
  EXPECT_TRUE(S_ISREG(st.st_mode));
  EXPECT_EQ(13, st.st_size);

  tebako_close(fd);
}

TEST_F(CApiTest, Fstat_InvalidFd)
{
  struct stat st;
  EXPECT_EQ(-1, tebako_fstat(999, &st));
  EXPECT_EQ(EBADF, tebako_get_errno());
}

TEST_F(CApiTest, Fstat_NullStat)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  EXPECT_EQ(-1, tebako_fstat(fd, nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());

  tebako_close(fd);
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

  int fd = tebako_open((mount_point + "/content/hello.txt").c_str(), O_RDONLY);
  ASSERT_GT(fd, 0);

  EXPECT_EQ(1, tebako_fd_is_embedded(fd));
  EXPECT_EQ(0, tebako_fd_is_embedded(STDOUT_FILENO));
  EXPECT_EQ(0, tebako_fd_is_embedded(STDIN_FILENO));

  tebako_close(fd);
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
  tebako_open(nullptr, O_RDONLY);  // Will set EINVAL
  EXPECT_EQ(EINVAL, tebako_get_errno());

  // Different operation sets different error
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  tebako_open((mount_point + "/nonexistent").c_str(), O_RDONLY);
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
  tebako_dir_t dir = tebako_opendir((mount_point + "/content").c_str());
  ASSERT_NE(nullptr, dir);

  int file_count = 0;
  struct tebako_c_dirent* entry;
  while ((entry = tebako_readdir(dir)) != nullptr) {
    file_count++;

    // Stat each entry
    std::string full_path = mount_point + "/content/" + entry->d_name;
    struct stat st;
    EXPECT_EQ(0, tebako_stat(full_path.c_str(), &st));

    // If regular file, try opening
    if (entry->d_type == DT_REG) {
      int fd = tebako_open(full_path.c_str(), O_RDONLY);
      EXPECT_GT(fd, 0);
      if (fd > 0) {
        tebako_close(fd);
      }
    }
  }

  EXPECT_GT(file_count, 0);
  tebako_closedir(dir);

  // Unmount
  tebako_fs_unmount();
  EXPECT_EQ(0, tebako_is_initialized());
}