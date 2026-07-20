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
#include <tebako/fs/backends/squashfs_backend.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>
#include <fcntl.h>

#include <thread>
#include <vector>
#include <algorithm>
#include <atomic>
#include <chrono>

using namespace tebako::fs;

// ===================================================================
// Test Fixtures
// ===================================================================

/**
 * @brief Base test fixture for SquashFS backend tests
 */
class SquashFSBackendTest : public ::testing::Test {
 protected:
  void SetUp() override { backend = std::make_unique<SquashFSBackend>(); }

  void TearDown() override
  {
    if (backend && backend->is_mounted()) {
      backend->unmount();
    }
  }

  std::unique_ptr<SquashFSBackend> backend;
  const std::string fixtures_path = "tests/fixtures/squashfs/";
  const std::string mount_point = "/mnt/test";
};

/**
 * @brief Test fixture with a mounted simple.sqfs archive
 */
class SquashFSBackendMountedTest : public SquashFSBackendTest {
 protected:
  void SetUp() override
  {
    SquashFSBackendTest::SetUp();
    std::string archive = fixtures_path + "simple.sqfs";
    auto mount_result = backend->mount(archive, mount_point);
    ASSERT_TRUE(mount_result.is_ok()) << "Failed to mount: " << mount_result.error().message;
  }
};

// ===================================================================
// 1. Lifecycle Tests (8 tests)
// ===================================================================

TEST_F(SquashFSBackendTest, ConstructorCreatesUnmountedBackend)
{
  EXPECT_FALSE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), "");
  EXPECT_EQ(backend->mount_point(), "");
}

TEST_F(SquashFSBackendTest, BackendInfoCorrect)
{
  EXPECT_EQ(backend->backend_name(), "SquashFS");
  EXPECT_FALSE(backend->backend_version().empty());
}

TEST_F(SquashFSBackendTest, MountValidArchiveSucceeds)
{
  std::string archive = fixtures_path + "simple.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  EXPECT_TRUE(mount_result.is_ok());
  EXPECT_TRUE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), archive);
  EXPECT_EQ(backend->mount_point(), mount_point);
}

TEST_F(SquashFSBackendTest, MountNonexistentArchiveFails)
{
  std::string archive = fixtures_path + "nonexistent.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  EXPECT_TRUE(mount_result.is_err());
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(SquashFSBackendTest, MountCorruptedArchiveFails)
{
  std::string archive = fixtures_path + "corrupted.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  EXPECT_TRUE(mount_result.is_err());
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(SquashFSBackendMountedTest, DoubleMountFails)
{
  std::string another_archive = fixtures_path + "nested.sqfs";
  auto mount_result = backend->mount(another_archive, "/mnt/another");
  EXPECT_TRUE(mount_result.is_err());
  // Should still be mounted to original archive
  EXPECT_TRUE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), fixtures_path + "simple.sqfs");
}

TEST_F(SquashFSBackendMountedTest, UnmountClearsState)
{
  backend->unmount();
  EXPECT_FALSE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), "");
  EXPECT_EQ(backend->mount_point(), "");
}

TEST_F(SquashFSBackendTest, UnmountWithoutMountIsNoOp)
{
  EXPECT_NO_THROW(backend->unmount());
  EXPECT_FALSE(backend->is_mounted());
}

// ===================================================================
// 2. File Existence Tests (6 tests)
// ===================================================================

TEST_F(SquashFSBackendMountedTest, ExistsReturnsTrueForValidFile)
{
  EXPECT_TRUE(backend->exists(mount_point + "/test.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/file2.txt"));
}

TEST_F(SquashFSBackendMountedTest, ExistsReturnsFalseForInvalidFile)
{
  EXPECT_FALSE(backend->exists(mount_point + "/nonexistent.txt"));
  EXPECT_FALSE(backend->exists(mount_point + "/missing/file.txt"));
}

TEST_F(SquashFSBackendMountedTest, IsFileCorrectForFiles)
{
  EXPECT_TRUE(backend->is_file(mount_point + "/test.txt"));
  EXPECT_TRUE(backend->is_file(mount_point + "/file2.txt"));
}

TEST_F(SquashFSBackendMountedTest, IsFileFalseForDirectories)
{
  // Root should be treated as directory
  EXPECT_FALSE(backend->is_file(mount_point));
  EXPECT_FALSE(backend->is_file(mount_point + "/"));
}

TEST_F(SquashFSBackendMountedTest, IsDirectoryCorrectForRoot)
{
  EXPECT_TRUE(backend->is_directory(mount_point));
  EXPECT_TRUE(backend->is_directory(mount_point + "/"));
}

TEST_F(SquashFSBackendTest, IsDirectoryCorrectForNestedDirs)
{
  std::string archive = fixtures_path + "nested.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  EXPECT_TRUE(backend->is_directory(mount_point + "/dir1"));
  EXPECT_TRUE(backend->is_directory(mount_point + "/dir1/subdir"));
  EXPECT_TRUE(backend->is_directory(mount_point + "/dir2"));
}

// ===================================================================
// 3. File Reading Tests (12 tests)
// ===================================================================

TEST_F(SquashFSBackendMountedTest, OpenValidFileSucceeds)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();
  EXPECT_EQ(handle->path(), mount_point + "/test.txt");
}

TEST_F(SquashFSBackendMountedTest, OpenInvalidFileFails)
{
  auto open_result = backend->open(mount_point + "/nonexistent.txt", O_RDONLY);
  EXPECT_TRUE(open_result.is_err());
}

TEST_F(SquashFSBackendMountedTest, OpenDirectoryFails)
{
  auto open_result = backend->open(mount_point, O_RDONLY);
  EXPECT_TRUE(open_result.is_err());
}

TEST_F(SquashFSBackendMountedTest, ReadFileContentsCorrect)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  char buffer[256] = {0};
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer) - 1);
  EXPECT_GT(bytes_read, 0);

  std::string content(buffer, bytes_read);
  EXPECT_EQ(content, "Hello from SquashFS!\n");
}

TEST_F(SquashFSBackendMountedTest, ReadIncrementsPosition)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  EXPECT_EQ(handle->tell(), 0);

  char buffer[5];
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
  EXPECT_EQ(bytes_read, 5);
  EXPECT_EQ(handle->tell(), 5);
}

TEST_F(SquashFSBackendMountedTest, ReadSetsEofFlag)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  EXPECT_FALSE(handle->eof());

  // Read entire file
  char buffer[256];
  while (handle->read(buffer, sizeof(buffer)) > 0) {
    // Keep reading
  }

  EXPECT_TRUE(handle->eof());
}

TEST_F(SquashFSBackendMountedTest, SeekSetPositionsCorrectly)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  off_t new_pos = handle->seek(5, SEEK_SET);
  EXPECT_EQ(new_pos, 5);
  EXPECT_EQ(handle->tell(), 5);
}

TEST_F(SquashFSBackendMountedTest, SeekCurPositionsCorrectly)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  handle->seek(5, SEEK_SET);
  off_t new_pos = handle->seek(3, SEEK_CUR);
  EXPECT_EQ(new_pos, 8);
  EXPECT_EQ(handle->tell(), 8);
}

TEST_F(SquashFSBackendMountedTest, SeekEndPositionsCorrectly)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  int64_t file_size = handle->size();
  off_t new_pos = handle->seek(0, SEEK_END);
  EXPECT_EQ(new_pos, file_size);
  EXPECT_EQ(handle->tell(), file_size);
}

TEST_F(SquashFSBackendMountedTest, SeekBeyondBoundsFails)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  // Seek beyond file size
  off_t result = handle->seek(10000, SEEK_SET);
  EXPECT_EQ(result, -1);
}

TEST_F(SquashFSBackendMountedTest, CloseReleasesResource)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  handle->close();

  // After close, operations should fail or return error
  char buffer[10];
  ssize_t result = handle->read(buffer, sizeof(buffer));
  EXPECT_EQ(result, -1);
}

TEST_F(SquashFSBackendMountedTest, OperationsAfterCloseFail)
{
  auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  handle->close();

  // Read should fail
  char buffer[10];
  EXPECT_EQ(handle->read(buffer, sizeof(buffer)), -1);

  // Seek should fail
  EXPECT_EQ(handle->seek(0, SEEK_SET), -1);
}

// ===================================================================
// 4. Directory Listing Tests (5 tests)
// ===================================================================

TEST_F(SquashFSBackendMountedTest, ListDirectoryReturnsAllEntries)
{
  auto list_result = backend->list_directory(mount_point);
  ASSERT_TRUE(list_result.is_ok());
  auto iter = std::move(list_result).unwrap();

  std::vector<std::string> entries;
  while (iter->has_next()) {
    entries.push_back(iter->next().name);
  }

  EXPECT_EQ(entries.size(), 2);
  EXPECT_NE(std::find(entries.begin(), entries.end(), "test.txt"), entries.end());
  EXPECT_NE(std::find(entries.begin(), entries.end(), "file2.txt"), entries.end());
}

TEST_F(SquashFSBackendMountedTest, DirectoryEntryHasCorrectMetadata)
{
  auto list_result = backend->list_directory(mount_point);
  ASSERT_TRUE(list_result.is_ok());
  auto iter = std::move(list_result).unwrap();

  ASSERT_TRUE(iter->has_next());
  DirectoryEntry entry = iter->next();

  EXPECT_FALSE(entry.name.empty());
  EXPECT_FALSE(entry.is_directory);
  EXPECT_GT(entry.size, 0);
  EXPECT_GT(entry.mtime, 0);
}

TEST_F(SquashFSBackendMountedTest, IteratorResetWorks)
{
  auto list_result = backend->list_directory(mount_point);
  ASSERT_TRUE(list_result.is_ok());
  auto iter = std::move(list_result).unwrap();

  // Read first entry
  ASSERT_TRUE(iter->has_next());
  std::string first_name = iter->next().name;

  // Reset and read again
  iter->reset();
  ASSERT_TRUE(iter->has_next());
  std::string first_name_again = iter->next().name;

  EXPECT_EQ(first_name, first_name_again);
}

TEST_F(SquashFSBackendTest, ListNestedDirectoryWorks)
{
  std::string archive = fixtures_path + "nested.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto list_result = backend->list_directory(mount_point + "/dir1");
  ASSERT_TRUE(list_result.is_ok());
  auto iter = std::move(list_result).unwrap();

  std::vector<std::string> entries;
  while (iter->has_next()) {
    entries.push_back(iter->next().name);
  }

  // Should have file1.txt and subdir
  EXPECT_GE(entries.size(), 1);
}

TEST_F(SquashFSBackendTest, ListEmptyDirectoryReturnsNoEntries)
{
  std::string archive = fixtures_path + "empty.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto list_result = backend->list_directory(mount_point + "/empty_dir");
  ASSERT_TRUE(list_result.is_ok());
  auto iter = std::move(list_result).unwrap();

  int count = 0;
  while (iter->has_next()) {
    iter->next();
    count++;
  }

  EXPECT_EQ(count, 0);
}

// ===================================================================
// 5. Metadata Tests (4 tests)
// ===================================================================

TEST_F(SquashFSBackendMountedTest, FileSizeCorrect)
{
  auto size_result = backend->file_size(mount_point + "/test.txt");
  ASSERT_TRUE(size_result.is_ok());
  EXPECT_EQ(size_result.unwrap(), 21);  // "Hello from SquashFS!\n" = 21 bytes
}

TEST_F(SquashFSBackendMountedTest, FileSizeInvalidFileReturnsError)
{
  auto size_result = backend->file_size(mount_point + "/nonexistent.txt");
  EXPECT_TRUE(size_result.is_err());
}

TEST_F(SquashFSBackendMountedTest, ModificationTimeNonZero)
{
  auto mtime_result = backend->modification_time(mount_point + "/test.txt");
  ASSERT_TRUE(mtime_result.is_ok());
  EXPECT_GT(mtime_result.unwrap(), 0);
}

TEST_F(SquashFSBackendTest, PermissionsPreservedCorrectly)
{
  std::string archive = fixtures_path + "permissions.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  // Read-only file (444)
  auto readonly_result = backend->permissions(mount_point + "/readonly.txt");
  ASSERT_TRUE(readonly_result.is_ok());
  EXPECT_EQ(readonly_result.unwrap(), 0444);

  // Executable script (755)
  auto script_result = backend->permissions(mount_point + "/script.sh");
  ASSERT_TRUE(script_result.is_ok());
  EXPECT_EQ(script_result.unwrap(), 0755);

  // Private file (600)
  auto private_result = backend->permissions(mount_point + "/private.txt");
  ASSERT_TRUE(private_result.is_ok());
  EXPECT_EQ(private_result.unwrap(), 0600);

  // Restricted directory (700)
  auto dir_result = backend->permissions(mount_point + "/restricted_dir");
  ASSERT_TRUE(dir_result.is_ok());
  EXPECT_EQ(dir_result.unwrap(), 0700);
}

// ===================================================================
// 6. Nested Directory Tests (3 tests)
// ===================================================================

TEST_F(SquashFSBackendTest, NestedDirectoryExists)
{
  std::string archive = fixtures_path + "nested.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  EXPECT_TRUE(backend->exists(mount_point + "/dir1"));
  EXPECT_TRUE(backend->exists(mount_point + "/dir1/subdir"));
}

TEST_F(SquashFSBackendTest, NestedFileExists)
{
  std::string archive = fixtures_path + "nested.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  EXPECT_TRUE(backend->exists(mount_point + "/dir1/file1.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/dir1/subdir/file2.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/dir2/file3.txt"));
}

TEST_F(SquashFSBackendTest, CanListNestedDirectory)
{
  std::string archive = fixtures_path + "nested.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto list_result = backend->list_directory(mount_point + "/dir1/subdir");
  ASSERT_TRUE(list_result.is_ok());
  auto iter = std::move(list_result).unwrap();

  bool found_file2 = false;
  while (iter->has_next()) {
    DirectoryEntry entry = iter->next();
    if (entry.name == "file2.txt") {
      found_file2 = true;
    }
  }

  EXPECT_TRUE(found_file2);
}

// ===================================================================
// 7. Edge Case Tests (3 tests)
// ===================================================================

TEST_F(SquashFSBackendTest, EmptyFileHasZeroSize)
{
  std::string archive = fixtures_path + "empty.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto size_result = backend->file_size(mount_point + "/empty_file.txt");
  ASSERT_TRUE(size_result.is_ok());
  EXPECT_EQ(size_result.unwrap(), 0);
}

TEST_F(SquashFSBackendTest, ReadEmptyFileReturnsZero)
{
  std::string archive = fixtures_path + "empty.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto open_result = backend->open(mount_point + "/empty_file.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  char buffer[10];
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
  EXPECT_EQ(bytes_read, 0);
  EXPECT_TRUE(handle->eof());
}

TEST_F(SquashFSBackendTest, EmptyDirectoryListsNoEntries)
{
  std::string archive = fixtures_path + "empty.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto list_result = backend->list_directory(mount_point + "/empty_dir");
  ASSERT_TRUE(list_result.is_ok());
  auto iter = std::move(list_result).unwrap();

  EXPECT_FALSE(iter->has_next());
}

// ===================================================================
// 8. Thread Safety Tests (2 tests)
// ===================================================================

TEST_F(SquashFSBackendMountedTest, ConcurrentReadsSucceed)
{
  const int num_threads = 10;
  std::atomic<int> success_count{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &success_count]() {
      auto open_result = backend->open(mount_point + "/test.txt", O_RDONLY);
      if (open_result.is_ok()) {
        auto handle = std::move(open_result).unwrap();
        char buffer[256];
        ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
        if (bytes_read > 0) {
          success_count++;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(success_count, num_threads);
}

TEST_F(SquashFSBackendMountedTest, ConcurrentDirectoryListsSucceed)
{
  const int num_threads = 10;
  std::atomic<int> success_count{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &success_count]() {
      auto list_result = backend->list_directory(mount_point);
      if (list_result.is_ok()) {
        auto iter = std::move(list_result).unwrap();
        if (iter->has_next()) {
          iter->next();
          success_count++;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(success_count, num_threads);
}

// ===================================================================
// 9. Error Handling Tests (2 tests)
// ===================================================================

TEST_F(SquashFSBackendTest, OperationsOnUnmountedBackendFail)
{
  EXPECT_FALSE(backend->exists(mount_point + "/test.txt"));
  EXPECT_FALSE(backend->is_file(mount_point + "/test.txt"));
  EXPECT_FALSE(backend->is_directory(mount_point));
  EXPECT_TRUE(backend->file_size(mount_point + "/test.txt").is_err());
  EXPECT_TRUE(backend->open(mount_point + "/test.txt", O_RDONLY).is_err());
  EXPECT_TRUE(backend->list_directory(mount_point).is_err());
}

TEST_F(SquashFSBackendMountedTest, InvalidOperationsReturnProperErrors)
{
  // Open with write flags should fail (read-only archive)
  EXPECT_TRUE(backend->open(mount_point + "/test.txt", O_WRONLY).is_err());
  EXPECT_TRUE(backend->open(mount_point + "/test.txt", O_RDWR).is_err());

  // List non-directory should fail
  EXPECT_TRUE(backend->list_directory(mount_point + "/test.txt").is_err());

  // File operations on directory should fail
  EXPECT_TRUE(backend->file_size(mount_point).is_err());
}

// ===================================================================
// 10. Performance Tests (2 tests - optional with GTEST_SKIP)
// ===================================================================

TEST_F(SquashFSBackendTest, ReadLargeFilePerformance)
{
  std::string archive = fixtures_path + "large.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  if (mount_result.is_err()) {
    GTEST_SKIP() << "Large test fixture not available";
  }

  auto open_result = backend->open(mount_point + "/large.txt", O_RDONLY);
  ASSERT_TRUE(open_result.is_ok());
  auto handle = std::move(open_result).unwrap();

  auto start = std::chrono::high_resolution_clock::now();

  char buffer[4096];
  int64_t total_read = 0;
  while (true) {
    ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  EXPECT_EQ(total_read, 10 * 1024 * 1024);  // 10 MB

  // Performance expectation: should read 10MB in under 5 seconds
  // SquashFS should be faster than ZIP due to better compression and native seek
  EXPECT_LT(duration.count(), 5000) << "Reading 10MB took " << duration.count() << "ms";
}

TEST_F(SquashFSBackendTest, ListManyFilesPerformance)
{
  std::string archive = fixtures_path + "large.sqfs";
  auto mount_result = backend->mount(archive, mount_point);
  if (mount_result.is_err()) {
    GTEST_SKIP() << "Large test fixture not available";
  }

  auto start = std::chrono::high_resolution_clock::now();

  auto list_result = backend->list_directory(mount_point + "/many_files");
  ASSERT_TRUE(list_result.is_ok());
  auto iter = std::move(list_result).unwrap();

  int count = 0;
  while (iter->has_next()) {
    iter->next();
    count++;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  EXPECT_EQ(count, 100);

  // Should list 100 files in under 1 second
  EXPECT_LT(duration.count(), 1000) << "Listing 100 files took " << duration.count() << "ms";
}
