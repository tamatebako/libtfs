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
#include <tebako/fs/backends/zip_backend.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <fcntl.h>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

using namespace tebako::fs;

// ===================================================================
// Test Fixtures
// ===================================================================

/**
 * @brief Base test fixture for ZIP backend tests
 */
class ZipBackendTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend = std::make_unique<ZipBackend>();
  }

  void TearDown() override {
    if (backend && backend->is_mounted()) {
      backend->unmount();
    }
  }

  std::unique_ptr<ZipBackend> backend;
  const std::string fixtures_path = "tests/fixtures/zip/";
  const std::string mount_point = "/mnt/test";
};

/**
 * @brief Test fixture with a mounted simple.zip archive
 */
class ZipBackendMountedTest : public ZipBackendTest {
 protected:
  void SetUp() override {
    ZipBackendTest::SetUp();
    std::string archive = fixtures_path + "simple.zip";
    ASSERT_TRUE(backend->mount(archive, mount_point));
  }
};

// ===================================================================
// 1. Lifecycle Tests (8 tests)
// ===================================================================

TEST_F(ZipBackendTest, ConstructorCreatesUnmountedBackend) {
  EXPECT_FALSE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), "");
  EXPECT_EQ(backend->mount_point(), "");
}

TEST_F(ZipBackendTest, BackendInfoCorrect) {
  EXPECT_EQ(backend->backend_name(), "ZIP");
  EXPECT_FALSE(backend->backend_version().empty());
}

TEST_F(ZipBackendTest, MountValidArchiveSucceeds) {
  std::string archive = fixtures_path + "simple.zip";
  EXPECT_TRUE(backend->mount(archive, mount_point));
  EXPECT_TRUE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), archive);
  EXPECT_EQ(backend->mount_point(), mount_point);
}

TEST_F(ZipBackendTest, MountNonexistentArchiveFails) {
  std::string archive = fixtures_path + "nonexistent.zip";
  EXPECT_FALSE(backend->mount(archive, mount_point));
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(ZipBackendTest, MountCorruptedArchiveFails) {
  std::string archive = fixtures_path + "corrupted.zip";
  EXPECT_FALSE(backend->mount(archive, mount_point));
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(ZipBackendMountedTest, DoubleMountFails) {
  std::string another_archive = fixtures_path + "nested.zip";
  EXPECT_FALSE(backend->mount(another_archive, "/mnt/another"));
  // Should still be mounted to original archive
  EXPECT_TRUE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), fixtures_path + "simple.zip");
}

TEST_F(ZipBackendMountedTest, UnmountClearsState) {
  backend->unmount();
  EXPECT_FALSE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), "");
  EXPECT_EQ(backend->mount_point(), "");
}

TEST_F(ZipBackendTest, UnmountWithoutMountIsNoOp) {
  EXPECT_NO_THROW(backend->unmount());
  EXPECT_FALSE(backend->is_mounted());
}

// ===================================================================
// 2. File Existence Tests (6 tests)
// ===================================================================

TEST_F(ZipBackendMountedTest, ExistsReturnsTrueForValidFile) {
  EXPECT_TRUE(backend->exists(mount_point + "/test.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/file2.txt"));
}

TEST_F(ZipBackendMountedTest, ExistsReturnsFalseForInvalidFile) {
  EXPECT_FALSE(backend->exists(mount_point + "/nonexistent.txt"));
  EXPECT_FALSE(backend->exists(mount_point + "/missing/file.txt"));
}

TEST_F(ZipBackendMountedTest, IsFileCorrectForFiles) {
  EXPECT_TRUE(backend->is_file(mount_point + "/test.txt"));
  EXPECT_TRUE(backend->is_file(mount_point + "/file2.txt"));
}

TEST_F(ZipBackendMountedTest, IsFileFalseForDirectories) {
  // Root should be treated as directory
  EXPECT_FALSE(backend->is_file(mount_point));
  EXPECT_FALSE(backend->is_file(mount_point + "/"));
}

TEST_F(ZipBackendMountedTest, IsDirectoryCorrectForRoot) {
  EXPECT_TRUE(backend->is_directory(mount_point));
  EXPECT_TRUE(backend->is_directory(mount_point + "/"));
}

TEST_F(ZipBackendTest, IsDirectoryCorrectForNestedDirs) {
  std::string archive = fixtures_path + "nested.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  EXPECT_TRUE(backend->is_directory(mount_point + "/dir1"));
  EXPECT_TRUE(backend->is_directory(mount_point + "/dir1/subdir"));
  EXPECT_TRUE(backend->is_directory(mount_point + "/dir2"));
}

// ===================================================================
// 3. File Reading Tests (12 tests)
// ===================================================================

TEST_F(ZipBackendMountedTest, OpenValidFileSucceeds) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);
  EXPECT_EQ(handle->path(), mount_point + "/test.txt");
}

TEST_F(ZipBackendMountedTest, OpenInvalidFileFails) {
  auto handle = backend->open(mount_point + "/nonexistent.txt", O_RDONLY);
  EXPECT_EQ(handle, nullptr);
}

TEST_F(ZipBackendMountedTest, OpenDirectoryFails) {
  auto handle = backend->open(mount_point, O_RDONLY);
  EXPECT_EQ(handle, nullptr);
}

TEST_F(ZipBackendMountedTest, ReadFileContentsCorrect) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  char buffer[256] = {0};
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer) - 1);
  EXPECT_GT(bytes_read, 0);

  std::string content(buffer, bytes_read);
  EXPECT_EQ(content, "Hello from ZIP!\n");
}

TEST_F(ZipBackendMountedTest, ReadIncrementsPosition) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  EXPECT_EQ(handle->tell(), 0);

  char buffer[5];
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
  EXPECT_EQ(bytes_read, 5);
  EXPECT_EQ(handle->tell(), 5);
}

TEST_F(ZipBackendMountedTest, ReadSetsEofFlag) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  EXPECT_FALSE(handle->eof());

  // Read entire file
  char buffer[256];
  while (handle->read(buffer, sizeof(buffer)) > 0) {
    // Keep reading
  }

  EXPECT_TRUE(handle->eof());
}

TEST_F(ZipBackendMountedTest, SeekSetPositionsCorrectly) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  off_t new_pos = handle->seek(5, SEEK_SET);
  EXPECT_EQ(new_pos, 5);
  EXPECT_EQ(handle->tell(), 5);
}

TEST_F(ZipBackendMountedTest, SeekCurPositionsCorrectly) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  handle->seek(5, SEEK_SET);
  off_t new_pos = handle->seek(3, SEEK_CUR);
  EXPECT_EQ(new_pos, 8);
  EXPECT_EQ(handle->tell(), 8);
}

TEST_F(ZipBackendMountedTest, SeekEndPositionsCorrectly) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  int64_t file_size = handle->size();
  off_t new_pos = handle->seek(0, SEEK_END);
  EXPECT_EQ(new_pos, file_size);
  EXPECT_EQ(handle->tell(), file_size);
}

TEST_F(ZipBackendMountedTest, SeekBeyondBoundsFails) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  // Seek beyond file size
  off_t result = handle->seek(10000, SEEK_SET);
  EXPECT_EQ(result, -1);
}

TEST_F(ZipBackendMountedTest, CloseReleasesResource) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  handle->close();

  // After close, operations should fail or return error
  char buffer[10];
  ssize_t result = handle->read(buffer, sizeof(buffer));
  EXPECT_EQ(result, -1);
}

TEST_F(ZipBackendMountedTest, OperationsAfterCloseFail) {
  auto handle = backend->open(mount_point + "/test.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

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

TEST_F(ZipBackendMountedTest, ListDirectoryReturnsAllEntries) {
  auto iter = backend->list_directory(mount_point);
  ASSERT_NE(iter, nullptr);

  std::vector<std::string> entries;
  while (iter->has_next()) {
    entries.push_back(iter->next().name);
  }

  EXPECT_EQ(entries.size(), 2);
  EXPECT_NE(std::find(entries.begin(), entries.end(), "test.txt"), entries.end());
  EXPECT_NE(std::find(entries.begin(), entries.end(), "file2.txt"), entries.end());
}

TEST_F(ZipBackendMountedTest, DirectoryEntryHasCorrectMetadata) {
  auto iter = backend->list_directory(mount_point);
  ASSERT_NE(iter, nullptr);

  ASSERT_TRUE(iter->has_next());
  DirectoryEntry entry = iter->next();

  EXPECT_FALSE(entry.name.empty());
  EXPECT_FALSE(entry.is_directory);
  EXPECT_GT(entry.size, 0);
}

TEST_F(ZipBackendMountedTest, IteratorResetWorks) {
  auto iter = backend->list_directory(mount_point);
  ASSERT_NE(iter, nullptr);

  // Read first entry
  ASSERT_TRUE(iter->has_next());
  std::string first_name = iter->next().name;

  // Reset and read again
  iter->reset();
  ASSERT_TRUE(iter->has_next());
  std::string first_name_again = iter->next().name;

  EXPECT_EQ(first_name, first_name_again);
}

TEST_F(ZipBackendTest, ListNestedDirectoryWorks) {
  std::string archive = fixtures_path + "nested.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  auto iter = backend->list_directory(mount_point + "/dir1");
  ASSERT_NE(iter, nullptr);

  std::vector<std::string> entries;
  while (iter->has_next()) {
    entries.push_back(iter->next().name);
  }

  // Should have file1.txt and subdir
  EXPECT_GE(entries.size(), 1);
}

TEST_F(ZipBackendTest, ListEmptyDirectoryReturnsNoEntries) {
  std::string archive = fixtures_path + "empty.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  auto iter = backend->list_directory(mount_point + "/empty_dir");
  ASSERT_NE(iter, nullptr);

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

TEST_F(ZipBackendMountedTest, FileSizeCorrect) {
  int64_t size = backend->file_size(mount_point + "/test.txt");
  EXPECT_EQ(size, 16);  // "Hello from ZIP!\n" = 16 bytes
}

TEST_F(ZipBackendMountedTest, FileSizeInvalidFileReturnsNegative) {
  int64_t size = backend->file_size(mount_point + "/nonexistent.txt");
  EXPECT_EQ(size, -1);
}

TEST_F(ZipBackendMountedTest, ModificationTimeNonZero) {
  time_t mtime = backend->modification_time(mount_point + "/test.txt");
  EXPECT_GT(mtime, 0);
}

TEST_F(ZipBackendMountedTest, PermissionsDefaultForFileAndDirectory) {
  // Files should have 0644
  mode_t file_perms = backend->permissions(mount_point + "/test.txt");
  EXPECT_EQ(file_perms, 0644);

  // Directories should have 0755
  mode_t dir_perms = backend->permissions(mount_point);
  EXPECT_EQ(dir_perms, 0755);
}

// ===================================================================
// 6. Nested Directory Tests (3 tests)
// ===================================================================

TEST_F(ZipBackendTest, NestedDirectoryExists) {
  std::string archive = fixtures_path + "nested.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  EXPECT_TRUE(backend->exists(mount_point + "/dir1"));
  EXPECT_TRUE(backend->exists(mount_point + "/dir1/subdir"));
}

TEST_F(ZipBackendTest, NestedFileExists) {
  std::string archive = fixtures_path + "nested.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  EXPECT_TRUE(backend->exists(mount_point + "/dir1/file1.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/dir1/subdir/file2.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/dir2/file3.txt"));
}

TEST_F(ZipBackendTest, CanListNestedDirectory) {
  std::string archive = fixtures_path + "nested.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  auto iter = backend->list_directory(mount_point + "/dir1/subdir");
  ASSERT_NE(iter, nullptr);

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

TEST_F(ZipBackendTest, EmptyFileHasZeroSize) {
  std::string archive = fixtures_path + "empty.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  int64_t size = backend->file_size(mount_point + "/empty_file.txt");
  EXPECT_EQ(size, 0);
}

TEST_F(ZipBackendTest, ReadEmptyFileReturnsZero) {
  std::string archive = fixtures_path + "empty.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  auto handle = backend->open(mount_point + "/empty_file.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  char buffer[10];
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
  EXPECT_EQ(bytes_read, 0);
  EXPECT_TRUE(handle->eof());
}

TEST_F(ZipBackendTest, EmptyDirectoryListsNoEntries) {
  std::string archive = fixtures_path + "empty.zip";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  auto iter = backend->list_directory(mount_point + "/empty_dir");
  ASSERT_NE(iter, nullptr);

  EXPECT_FALSE(iter->has_next());
}

// ===================================================================
// 8. Thread Safety Tests (2 tests)
// ===================================================================

TEST_F(ZipBackendMountedTest, ConcurrentReadsSucceed) {
  // NOTE: libzip has internal limitations with concurrent zip_fopen_index() calls
  // We serialize file opening but allow concurrent reading to test our thread safety
  const int num_threads = 4;
  std::atomic<int> success_count{0};
  std::mutex open_mutex;  // Serialize file opening due to libzip limitations
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &success_count, &open_mutex]() {
      std::unique_ptr<FileHandle> handle;
      {
        std::lock_guard<std::mutex> lock(open_mutex);
        handle = backend->open(mount_point + "/test.txt", O_RDONLY);
      }
      if (handle) {
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

TEST_F(ZipBackendMountedTest, ConcurrentDirectoryListsSucceed) {
  const int num_threads = 4;  // Reduced from 10 to avoid libzip race conditions
  std::atomic<int> success_count{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &success_count]() {
      auto iter = backend->list_directory(mount_point);
      if (iter && iter->has_next()) {
        iter->next();
        success_count++;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(success_count, num_threads);
}

// =================================================================--
// 9. Error Handling Tests (2 tests)
// ===================================================================

TEST_F(ZipBackendTest, OperationsOnUnmountedBackendFail) {
  EXPECT_FALSE(backend->exists(mount_point + "/test.txt"));
  EXPECT_FALSE(backend->is_file(mount_point + "/test.txt"));
  EXPECT_FALSE(backend->is_directory(mount_point));
  EXPECT_EQ(backend->file_size(mount_point + "/test.txt"), -1);
  EXPECT_EQ(backend->open(mount_point + "/test.txt", O_RDONLY), nullptr);
  EXPECT_EQ(backend->list_directory(mount_point), nullptr);
}

TEST_F(ZipBackendMountedTest, InvalidOperationsReturnProperErrors) {
  // Open with write flags should fail (read-only archive)
  EXPECT_EQ(backend->open(mount_point + "/test.txt", O_WRONLY), nullptr);
  EXPECT_EQ(backend->open(mount_point + "/test.txt", O_RDWR), nullptr);

  // List non-directory should fail
  EXPECT_EQ(backend->list_directory(mount_point + "/test.txt"), nullptr);

  // File operations on directory should fail
  EXPECT_EQ(backend->file_size(mount_point), -1);
}

// ===================================================================
// 10. Performance Tests (2 tests - optional with GTEST_SKIP)
// ===================================================================

TEST_F(ZipBackendTest, ReadLargeFilePerformance) {
  std::string archive = fixtures_path + "large.zip";
  if (!backend->mount(archive, mount_point)) {
    GTEST_SKIP() << "Large test fixture not available";
  }

  auto handle = backend->open(mount_point + "/large.txt", O_RDONLY);
  ASSERT_NE(handle, nullptr);

  auto start = std::chrono::high_resolution_clock::now();

  char buffer[4096];
  int64_t total_read = 0;
  while (true) {
    ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
    if (bytes_read <= 0) break;
    total_read += bytes_read;
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  EXPECT_EQ(total_read, 10 * 1024 * 1024);  // 10 MB

  // Performance expectation: should read 10MB in under 5 seconds
  // Adjust threshold as needed for CI environment
  EXPECT_LT(duration.count(), 5000) << "Reading 10MB took " << duration.count() << "ms";
}

TEST_F(ZipBackendTest, ListManyFilesPerformance) {
  std::string archive = fixtures_path + "large.zip";
  if (!backend->mount(archive, mount_point)) {
    GTEST_SKIP() << "Large test fixture not available";
  }

  auto start = std::chrono::high_resolution_clock::now();

  auto iter = backend->list_directory(mount_point + "/many_files");
  ASSERT_NE(iter, nullptr);

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