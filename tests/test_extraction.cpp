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
#include <sys/stat.h>

#include "zip_test_helper.h"

namespace fs = std::filesystem;

/**
 * @brief Test fixture for extraction API tests
 *
 * NOTE: These tests share global C API state and must be serialized.
 * CMakeLists.txt uses RESOURCE_LOCK to ensure this.
 */
class ExtractionTest : public ::testing::Test {
 protected:
  std::string test_dir;
  std::string archive_path;
  std::string mount_point;
  std::string extract_dir;

  void SetUp() override
  {
    // Create temporary directory
    test_dir = (fs::temp_directory_path() / "tebako_extraction_test").generic_string();
    fs::create_directories(test_dir);

    // Create test archive with various file types
    create_test_archive();

    mount_point = "/__tebako_extract_test__";
    extract_dir = test_dir + "/extracted";
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
    // Create a directory structure with various file types
    fs::path content_dir = fs::path(test_dir) / "content";
    fs::create_directories(content_dir);
    fs::create_directories(content_dir / "dir1" / "subdir");
    fs::create_directories(content_dir / "dir2");
    fs::create_directories(content_dir / "empty_dir");

    // Create various test files
    write_file(content_dir / "root.txt", "Root file content");
    write_file(content_dir / "large.bin", std::string(10000, 'X'));
    write_file(content_dir / "empty.txt", "");
    write_file(content_dir / "dir1" / "file1.txt", "File in dir1");
    write_file(content_dir / "dir1" / "subdir" / "deep.txt", "Deep nested file");
    write_file(content_dir / "dir2" / "file2.txt", "File in dir2");

    // Set various permissions
    fs::permissions(
        content_dir / "root.txt",
        fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read);  // 0644

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

  std::string read_file(const fs::path& path)
  {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
      return "";
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  }

  bool file_exists(const std::string& path) { return fs::exists(path); }

  bool dir_exists(const std::string& path) { return fs::exists(path) && fs::is_directory(path); }

  size_t count_files_recursive(const std::string& path)
  {
    size_t count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
      if (entry.is_regular_file()) {
        count++;
      }
    }
    return count;
  }

  size_t count_dirs_recursive(const std::string& path)
  {
    size_t count = 0;
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
      if (entry.is_directory()) {
        count++;
      }
    }
    return count;
  }
};

// ============================================================
// Basic Extraction Tests (5 tests)
// ============================================================

TEST_F(ExtractionTest, ExtractAll_Success)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));
  EXPECT_EQ(0, tebako_get_errno());

  // Verify extraction directory was created
  EXPECT_TRUE(dir_exists(extract_dir));
}

TEST_F(ExtractionTest, ExtractAll_NullDestination)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  EXPECT_EQ(-1, tebako_fs_extract_all(nullptr));
  EXPECT_EQ(EINVAL, tebako_get_errno());
}

TEST_F(ExtractionTest, ExtractAll_NotMounted)
{
  EXPECT_EQ(-1, tebako_fs_extract_all(extract_dir.c_str()));
  EXPECT_EQ(ENODEV, tebako_get_errno());
}

TEST_F(ExtractionTest, ExtractAll_CreatesNonexistentDirectory)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  std::string new_dir = extract_dir + "/new/nested/path";
  EXPECT_FALSE(dir_exists(new_dir));

  EXPECT_EQ(0, tebako_fs_extract_all(new_dir.c_str()));
  EXPECT_TRUE(dir_exists(new_dir));
}

TEST_F(ExtractionTest, ExtractAll_ExtractsToExistingDirectory)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  // Pre-create directory
  fs::create_directories(extract_dir);
  ASSERT_TRUE(dir_exists(extract_dir));

  EXPECT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));
}

// ============================================================
// File Content Tests (4 tests)
// ============================================================

TEST_F(ExtractionTest, ExtractedFiles_ContentCorrect)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Verify file contents
  std::string content = read_file(extract_dir + "/content/root.txt");
  EXPECT_EQ("Root file content", content);

  content = read_file(extract_dir + "/content/dir1/file1.txt");
  EXPECT_EQ("File in dir1", content);

  content = read_file(extract_dir + "/content/dir1/subdir/deep.txt");
  EXPECT_EQ("Deep nested file", content);
}

TEST_F(ExtractionTest, ExtractedFiles_EmptyFileCorrect)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  std::string empty_path = extract_dir + "/content/empty.txt";
  EXPECT_TRUE(file_exists(empty_path));

  std::string content = read_file(empty_path);
  EXPECT_EQ("", content);
  EXPECT_EQ(0, fs::file_size(empty_path));
}

TEST_F(ExtractionTest, ExtractedFiles_LargeFileCorrect)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  std::string large_path = extract_dir + "/content/large.bin";
  EXPECT_TRUE(file_exists(large_path));

  size_t size = fs::file_size(large_path);
  EXPECT_EQ(10000, size);

  std::string content = read_file(large_path);
  EXPECT_EQ(10000, content.size());
  EXPECT_EQ('X', content[0]);
  EXPECT_EQ('X', content[9999]);
}

TEST_F(ExtractionTest, ExtractedFiles_BinaryDataPreserved)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Read binary file and verify all bytes
  std::string large_path = extract_dir + "/content/large.bin";
  std::string content = read_file(large_path);

  for (size_t i = 0; i < content.size(); i++) {
    EXPECT_EQ('X', content[i]) << "Byte mismatch at position " << i;
  }
}

// ============================================================
// Directory Structure Tests (4 tests)
// ============================================================

TEST_F(ExtractionTest, ExtractedDirs_StructurePreserved)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Verify directory structure
  EXPECT_TRUE(dir_exists(extract_dir + "/content"));
  EXPECT_TRUE(dir_exists(extract_dir + "/content/dir1"));
  EXPECT_TRUE(dir_exists(extract_dir + "/content/dir1/subdir"));
  EXPECT_TRUE(dir_exists(extract_dir + "/content/dir2"));
  EXPECT_TRUE(dir_exists(extract_dir + "/content/empty_dir"));
}

TEST_F(ExtractionTest, ExtractedDirs_EmptyDirectoryCreated)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  std::string empty_dir_path = extract_dir + "/content/empty_dir";
  EXPECT_TRUE(dir_exists(empty_dir_path));

  // Verify it's empty
  int count = 0;
  for (const auto& entry : fs::directory_iterator(empty_dir_path)) {
    (void)entry;
    count++;
  }
  EXPECT_EQ(0, count);
}

TEST_F(ExtractionTest, ExtractedDirs_AllFilesPresent)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Count extracted files
  size_t file_count = count_files_recursive(extract_dir);

  // We created 6 files: root.txt, large.bin, empty.txt, file1.txt, deep.txt, file2.txt
  EXPECT_EQ(6, file_count);
}

TEST_F(ExtractionTest, ExtractedDirs_NestedPathsCorrect)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Verify deep nested file is in correct location
  std::string deep_path = extract_dir + "/content/dir1/subdir/deep.txt";
  EXPECT_TRUE(file_exists(deep_path));

  std::string content = read_file(deep_path);
  EXPECT_EQ("Deep nested file", content);
}

// ============================================================
// Metadata Preservation Tests (4 tests)
// ============================================================

TEST_F(ExtractionTest, Metadata_PermissionsPreserved)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  std::string file_path = extract_dir + "/content/root.txt";
  EXPECT_TRUE(file_exists(file_path));

  // Get file permissions
  auto perms = fs::status(file_path).permissions();

  // Should have read permissions at minimum
  EXPECT_TRUE((perms & fs::perms::owner_read) != fs::perms::none);
}

TEST_F(ExtractionTest, Metadata_ModificationTimePreserved)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  // Get original mtime from archive
  struct stat st;
  ASSERT_EQ(0, tebako_stat((mount_point + "/content/root.txt").c_str(), &st));
  time_t original_mtime = st.st_mtime;

  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Get extracted file mtime
  std::string file_path = extract_dir + "/content/root.txt";
  auto ftime = fs::last_write_time(file_path);
  auto sys_time = std::chrono::file_clock::to_sys(ftime);
  auto sys_time_us = std::chrono::time_point_cast<std::chrono::microseconds>(sys_time);
  time_t extracted_mtime = std::chrono::system_clock::to_time_t(sys_time_us);

  // Times should match (allow 1 second difference for filesystem precision)
  EXPECT_NEAR(original_mtime, extracted_mtime, 1);
}

TEST_F(ExtractionTest, Metadata_DirectoryPermissionsSet)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  std::string dir_path = extract_dir + "/content/dir1";
  EXPECT_TRUE(dir_exists(dir_path));

  // Check directory has execute permission (needed to enter directory)
  auto perms = fs::status(dir_path).permissions();
  EXPECT_TRUE((perms & fs::perms::owner_exec) != fs::perms::none);
}

TEST_F(ExtractionTest, Metadata_FileSizesCorrect)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Verify file sizes
  EXPECT_EQ(17, fs::file_size(extract_dir + "/content/root.txt"));
  EXPECT_EQ(10000, fs::file_size(extract_dir + "/content/large.bin"));
  EXPECT_EQ(0, fs::file_size(extract_dir + "/content/empty.txt"));
  EXPECT_EQ(12, fs::file_size(extract_dir + "/content/dir1/file1.txt"));
}

// ============================================================
// Edge Cases (3 tests)
// ============================================================

TEST_F(ExtractionTest, EdgeCase_ExtractTwiceOverwrites)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  // First extraction
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Modify an extracted file
  std::string file_path = extract_dir + "/content/root.txt";
  write_file(file_path, "Modified content");
  EXPECT_EQ("Modified content", read_file(file_path));

  // Second extraction should overwrite
  EXPECT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));
  EXPECT_EQ("Root file content", read_file(file_path));
}

TEST_F(ExtractionTest, EdgeCase_PathWithSpaces)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  std::string spaced_dir = test_dir + "/path with spaces";
  EXPECT_EQ(0, tebako_fs_extract_all(spaced_dir.c_str()));

  EXPECT_TRUE(dir_exists(spaced_dir));
  EXPECT_TRUE(file_exists(spaced_dir + "/content/root.txt"));
}

TEST_F(ExtractionTest, EdgeCase_ExtractionAfterUnmount)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  tebako_fs_unmount();

  EXPECT_EQ(-1, tebako_fs_extract_all(extract_dir.c_str()));
  EXPECT_EQ(ENODEV, tebako_get_errno());
}

// ============================================================
// Integration Tests (3 tests)
// ============================================================

TEST_F(ExtractionTest, Integration_ExtractAndCompare)
{
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));
  ASSERT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // For each file in archive, verify it exists on disk with same content
  std::vector<std::string> test_files = {"/content/root.txt",
                                         "/content/large.bin",
                                         "/content/empty.txt",
                                         "/content/dir1/file1.txt",
                                         "/content/dir1/subdir/deep.txt",
                                         "/content/dir2/file2.txt"};

  for (const auto& rel_path : test_files) {
    // Read from VFS
    int fd = tebako_open((mount_point + rel_path).c_str(), O_RDONLY);
    ASSERT_GT(fd, 0) << "Failed to open: " << rel_path;

    std::vector<char> vfs_content(20000);
    ssize_t vfs_size = tebako_read(fd, vfs_content.data(), vfs_content.size());
    ASSERT_GE(vfs_size, 0);
    tebako_close(fd);

    // Read from disk
    std::string disk_path = extract_dir + rel_path;
    std::string disk_content = read_file(disk_path);

    // Compare
    EXPECT_EQ(vfs_size, disk_content.size()) << "Size mismatch for: " << rel_path;
    EXPECT_EQ(0, memcmp(vfs_content.data(), disk_content.data(), vfs_size)) << "Content mismatch for: " << rel_path;
  }
}

TEST_F(ExtractionTest, Integration_MultipleBackendsSupported)
{
  // Test with ZIP backend
  ASSERT_EQ(0, tebako_fs_init_from_file(archive_path.c_str(), mount_point.c_str()));

  const char* backend = tebako_get_backend_name();
  ASSERT_NE(nullptr, backend);
  EXPECT_STREQ("ZIP", backend);

  EXPECT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  size_t file_count = count_files_recursive(extract_dir);
  EXPECT_EQ(6, file_count);
}

TEST_F(ExtractionTest, Integration_MemoryMountedArchiveExtracts)
{
  // Read archive into memory
  std::ifstream ifs(archive_path, std::ios::binary | std::ios::ate);
  ASSERT_TRUE(ifs.is_open());

  size_t size = ifs.tellg();
  std::vector<uint8_t> data(size);

  ifs.seekg(0);
  ifs.read(reinterpret_cast<char*>(data.data()), size);
  ifs.close();

  // Mount from memory
  ASSERT_EQ(0, tebako_fs_init(data.data(), data.size(), mount_point.c_str()));

  // Extract should work
  EXPECT_EQ(0, tebako_fs_extract_all(extract_dir.c_str()));

  // Verify extraction
  EXPECT_TRUE(file_exists(extract_dir + "/content/root.txt"));
  EXPECT_EQ("Root file content", read_file(extract_dir + "/content/root.txt"));
}