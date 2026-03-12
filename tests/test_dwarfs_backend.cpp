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
#include <tebako/fs/backends/dwarfs_backend.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <fcntl.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace tebako::fs;

// ===================================================================
// Test Fixtures
// ===================================================================

/**
 * @brief Base test fixture for DwarFS backend tests
 */
class DwarfsBackendTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backend = std::make_unique<DwarfsBackend>();
  }

  void TearDown() override {
    if (backend && backend->is_mounted()) {
      backend->unmount();
    }
  }

  std::unique_ptr<DwarfsBackend> backend;
  const std::string fixtures_path = "tests/fixtures/dwarfs/";
  const std::string mount_point = "/mnt/test";
};

/**
 * @brief Test fixture with a mounted simple.dwarfs archive
 */
class DwarfsBackendMountedTest : public DwarfsBackendTest {
 protected:
  void SetUp() override {
    DwarfsBackendTest::SetUp();
    std::string archive = fixtures_path + "simple.dwarfs";
    auto mount_result = backend->mount(archive, mount_point);
    ASSERT_TRUE(mount_result.is_ok()) << "Failed to mount: " << mount_result.error().message;
  }
};

// ===================================================================
// 1. Lifecycle Tests (8 tests)
// ===================================================================

TEST_F(DwarfsBackendTest, ConstructorCreatesUnmountedBackend) {
  EXPECT_FALSE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), "");
  EXPECT_EQ(backend->mount_point(), "");
}

TEST_F(DwarfsBackendTest, BackendInfoCorrect) {
  EXPECT_EQ(backend->backend_name(), "DwarFS");
  EXPECT_FALSE(backend->backend_version().empty());
}

TEST_F(DwarfsBackendTest, MountValidArchiveSucceeds) {
  std::string archive = fixtures_path + "simple.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  EXPECT_TRUE(mount_result.is_ok());
  EXPECT_TRUE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), archive);
  EXPECT_EQ(backend->mount_point(), mount_point);
}

TEST_F(DwarfsBackendTest, MountNonexistentArchiveFails) {
  std::string archive = fixtures_path + "nonexistent.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  EXPECT_TRUE(mount_result.is_err());
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(DwarfsBackendTest, MountCorruptedArchiveFails) {
  std::string archive = fixtures_path + "corrupted.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  EXPECT_TRUE(mount_result.is_err());
  EXPECT_FALSE(backend->is_mounted());
}

TEST_F(DwarfsBackendMountedTest, DoubleMountFails) {
  std::string another_archive = fixtures_path + "nested.dwarfs";
  auto mount_result = backend->mount(another_archive, "/mnt/another");
  EXPECT_TRUE(mount_result.is_err());
  // Should still be mounted to original archive
  EXPECT_TRUE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), fixtures_path + "simple.dwarfs");
}

TEST_F(DwarfsBackendMountedTest, UnmountClearsState) {
  backend->unmount();
  EXPECT_FALSE(backend->is_mounted());
  EXPECT_EQ(backend->archive_path(), "");
  EXPECT_EQ(backend->mount_point(), "");
}

TEST_F(DwarfsBackendTest, UnmountWithoutMountIsNoOp) {
  EXPECT_NO_THROW(backend->unmount());
  EXPECT_FALSE(backend->is_mounted());
}

// ===================================================================
// 2. File Existence Tests (6 tests)
// ===================================================================

TEST_F(DwarfsBackendMountedTest, ExistsReturnsTrueForValidFile) {
  EXPECT_TRUE(backend->exists(mount_point + "/hello.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/test.txt"));
}

TEST_F(DwarfsBackendMountedTest, ExistsReturnsFalseForInvalidFile) {
  EXPECT_FALSE(backend->exists(mount_point + "/nonexistent.txt"));
  EXPECT_FALSE(backend->exists(mount_point + "/missing/file.txt"));
}

TEST_F(DwarfsBackendMountedTest, IsFileCorrectForFiles) {
  EXPECT_TRUE(backend->is_file(mount_point + "/hello.txt"));
  EXPECT_TRUE(backend->is_file(mount_point + "/test.txt"));
}

TEST_F(DwarfsBackendMountedTest, IsFileFalseForDirectories) {
  // Root should be treated as directory
  EXPECT_FALSE(backend->is_file(mount_point));
  EXPECT_FALSE(backend->is_file(mount_point + "/"));
  EXPECT_FALSE(backend->is_file(mount_point + "/subdir"));
}

TEST_F(DwarfsBackendMountedTest, IsDirectoryCorrectForRoot) {
  EXPECT_TRUE(backend->is_directory(mount_point));
  EXPECT_TRUE(backend->is_directory(mount_point + "/"));
}

TEST_F(DwarfsBackendTest, IsDirectoryCorrectForNestedDirs) {
  std::string archive = fixtures_path + "nested.dwarfs";
  ASSERT_TRUE(backend->mount(archive, mount_point));

  EXPECT_TRUE(backend->is_directory(mount_point + "/a"));
  EXPECT_TRUE(backend->is_directory(mount_point + "/a/b"));
  EXPECT_TRUE(backend->is_directory(mount_point + "/a/b/c"));
  EXPECT_TRUE(backend->is_directory(mount_point + "/a/b/c/d"));
}

// ===================================================================
// 3. File Reading Tests (12 tests)
// ===================================================================

TEST_F(DwarfsBackendMountedTest, OpenValidFileSucceeds) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();
  EXPECT_EQ(handle->path(), mount_point + "/hello.txt");
}

TEST_F(DwarfsBackendMountedTest, OpenInvalidFileFails) {
  auto result = backend->open(mount_point + "/nonexistent.txt", O_RDONLY);
  EXPECT_TRUE(result.is_err());
}

TEST_F(DwarfsBackendMountedTest, OpenDirectoryFails) {
  auto result = backend->open(mount_point, O_RDONLY);
  EXPECT_TRUE(result.is_err());
}

TEST_F(DwarfsBackendMountedTest, ReadFileContentsCorrect) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  char buffer[256] = {0};
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer) - 1);
  EXPECT_GT(bytes_read, 0);

  std::string content(buffer, bytes_read);
  EXPECT_EQ(content, "Hello, DwarFS!\n");
}

TEST_F(DwarfsBackendMountedTest, ReadIncrementsPosition) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  EXPECT_EQ(handle->tell(), 0);

  char buffer[5];
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
  EXPECT_EQ(bytes_read, 5);
  EXPECT_EQ(handle->tell(), 5);
}

TEST_F(DwarfsBackendMountedTest, ReadSetsEofFlag) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  EXPECT_FALSE(handle->eof());

  // Read entire file
  char buffer[256];
  while (handle->read(buffer, sizeof(buffer)) > 0) {
    // Keep reading
  }

  EXPECT_TRUE(handle->eof());
}

TEST_F(DwarfsBackendMountedTest, SeekSetPositionsCorrectly) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  off_t new_pos = handle->seek(5, SEEK_SET);
  EXPECT_EQ(new_pos, 5);
  EXPECT_EQ(handle->tell(), 5);
}

TEST_F(DwarfsBackendMountedTest, SeekCurPositionsCorrectly) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  handle->seek(5, SEEK_SET);
  off_t new_pos = handle->seek(3, SEEK_CUR);
  EXPECT_EQ(new_pos, 8);
  EXPECT_EQ(handle->tell(), 8);
}

TEST_F(DwarfsBackendMountedTest, SeekEndPositionsCorrectly) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  int64_t file_size = handle->size();
  off_t new_pos = handle->seek(0, SEEK_END);
  EXPECT_EQ(new_pos, file_size);
  EXPECT_EQ(handle->tell(), file_size);
}

TEST_F(DwarfsBackendMountedTest, SeekBeyondBoundsFails) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  // Seek beyond file size
  off_t res = handle->seek(10000, SEEK_SET);
  EXPECT_EQ(res, -1);
}

TEST_F(DwarfsBackendMountedTest, NativeSeekDoesNotReopenFile) {
  // DwarFS advantage: native seek support without file reopening
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  // Multiple seeks should be fast (no reopening)
  for (int i = 0; i < 10; i++) {
    off_t pos = handle->seek(i, SEEK_SET);
    EXPECT_EQ(pos, i);
  }
}

TEST_F(DwarfsBackendMountedTest, CloseReleasesResource) {
  auto handle_result = backend->open(mount_point + "/hello.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  handle->close();

  // After close, operations should fail or return error
  char buffer[10];
  ssize_t res = handle->read(buffer, sizeof(buffer));
  EXPECT_EQ(res, -1);
}

// ===================================================================
// 4. Directory Listing Tests (5 tests)
// ===================================================================

TEST_F(DwarfsBackendMountedTest, ListDirectoryReturnsAllEntries) {
  auto result = backend->list_directory(mount_point);
  ASSERT_TRUE(result.is_ok());
  auto iter = std::move(result).unwrap();

  std::vector<std::string> entries;
  while (iter->has_next()) {
    entries.push_back(iter->next().name);
  }

  // simple.dwarfs has hello.txt, test.txt, and subdir
  EXPECT_GE(entries.size(), 2);
  EXPECT_NE(std::find(entries.begin(), entries.end(), "hello.txt"), entries.end());
  EXPECT_NE(std::find(entries.begin(), entries.end(), "test.txt"), entries.end());
}

TEST_F(DwarfsBackendMountedTest, DirectoryEntryHasCorrectMetadata) {
  auto result = backend->list_directory(mount_point);
  ASSERT_TRUE(result.is_ok());
  auto iter = std::move(result).unwrap();

  ASSERT_TRUE(iter->has_next());
  DirectoryEntry entry = iter->next();

  EXPECT_FALSE(entry.name.empty());
  EXPECT_GT(entry.size, 0);
  EXPECT_GT(entry.mtime, 0);
}

TEST_F(DwarfsBackendMountedTest, IteratorResetWorks) {
  auto result = backend->list_directory(mount_point);
  ASSERT_TRUE(result.is_ok());
  auto iter = std::move(result).unwrap();

  // Read first entry
  ASSERT_TRUE(iter->has_next());
  std::string first_name = iter->next().name;

  // Reset and read again
  iter->reset();
  ASSERT_TRUE(iter->has_next());
  std::string first_name_again = iter->next().name;

  EXPECT_EQ(first_name, first_name_again);
}

TEST_F(DwarfsBackendTest, ListNestedDirectoryWorks) {
  std::string archive = fixtures_path + "nested.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto result = backend->list_directory(mount_point + "/a/b/c");
  ASSERT_TRUE(result.is_ok());
  auto iter = std::move(result).unwrap();

  std::vector<std::string> entries;
  while (iter->has_next()) {
    entries.push_back(iter->next().name);
  }

  // Should have directory 'd'
  EXPECT_GE(entries.size(), 1);
}

TEST_F(DwarfsBackendTest, ListEmptyDirectoryReturnsNoEntries) {
  std::string archive = fixtures_path + "empty.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto result = backend->list_directory(mount_point);
  ASSERT_TRUE(result.is_ok());
  auto iter = std::move(result).unwrap();

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

TEST_F(DwarfsBackendMountedTest, FileSizeCorrect) {
  auto result = backend->file_size(mount_point + "/hello.txt");
  ASSERT_TRUE(result.is_ok());
  EXPECT_EQ(result.unwrap(), 15);  // "Hello, DwarFS!\n" = 15 bytes
}

TEST_F(DwarfsBackendMountedTest, FileSizeInvalidFileReturnsError) {
  auto result = backend->file_size(mount_point + "/nonexistent.txt");
  EXPECT_TRUE(result.is_err());
}

TEST_F(DwarfsBackendMountedTest, ModificationTimeNonZero) {
  auto result = backend->modification_time(mount_point + "/hello.txt");
  ASSERT_TRUE(result.is_ok());
  EXPECT_GT(result.unwrap(), 0);
}

TEST_F(DwarfsBackendTest, PermissionsPreservedCorrectly) {
  std::string archive = fixtures_path + "permissions.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  // Read-only file (444)
  auto readonly_result = backend->permissions(mount_point + "/readonly.txt");
  ASSERT_TRUE(readonly_result.is_ok());
  EXPECT_EQ(readonly_result.unwrap(), 0444);

  // Executable script (755)
  auto exec_result = backend->permissions(mount_point + "/executable.sh");
  ASSERT_TRUE(exec_result.is_ok());
  EXPECT_EQ(exec_result.unwrap(), 0755);

  // Regular file (644)
  auto regular_result = backend->permissions(mount_point + "/readable.txt");
  ASSERT_TRUE(regular_result.is_ok());
  EXPECT_EQ(regular_result.unwrap(), 0644);
}

// ===================================================================
// 6. Nested Directory Tests (3 tests)
// ===================================================================

TEST_F(DwarfsBackendTest, NestedDirectoryExists) {
  std::string archive = fixtures_path + "nested.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  EXPECT_TRUE(backend->exists(mount_point + "/a"));
  EXPECT_TRUE(backend->exists(mount_point + "/a/b"));
  EXPECT_TRUE(backend->exists(mount_point + "/a/b/c"));
  EXPECT_TRUE(backend->exists(mount_point + "/a/b/c/d"));
}

TEST_F(DwarfsBackendTest, NestedFileExists) {
  std::string archive = fixtures_path + "nested.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  EXPECT_TRUE(backend->exists(mount_point + "/root.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/a/b/c/d/deep.txt"));
}

TEST_F(DwarfsBackendTest, CanListNestedDirectory) {
  std::string archive = fixtures_path + "nested.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto result = backend->list_directory(mount_point + "/a/b/c/d");
  ASSERT_TRUE(result.is_ok());
  auto iter = std::move(result).unwrap();

  bool found_deep = false;
  while (iter->has_next()) {
    DirectoryEntry entry = iter->next();
    if (entry.name == "deep.txt") {
      found_deep = true;
    }
  }

  EXPECT_TRUE(found_deep);
}

// ===================================================================
// 7. Edge Case Tests (3 tests)
// ===================================================================

TEST_F(DwarfsBackendTest, EmptyArchiveMountsSuccessfully) {
  std::string archive = fixtures_path + "empty.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  EXPECT_TRUE(mount_result.is_ok());
  EXPECT_TRUE(backend->is_mounted());
}

TEST_F(DwarfsBackendMountedTest, ReadNestedFileContentsCorrect) {
  backend->unmount();
  std::string archive = fixtures_path + "simple.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  auto handle_result = backend->open(mount_point + "/subdir/nested.txt", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  char buffer[256] = {0};
  ssize_t bytes_read = handle->read(buffer, sizeof(buffer) - 1);
  EXPECT_GT(bytes_read, 0);

  std::string content(buffer, bytes_read);
  EXPECT_EQ(content, "Nested file\n");
}

TEST_F(DwarfsBackendTest, PathNormalizationWorks) {
  std::string archive = fixtures_path + "simple.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  ASSERT_TRUE(mount_result.is_ok());

  // All these should resolve to the same file
  EXPECT_TRUE(backend->exists(mount_point + "/hello.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "//hello.txt"));
  EXPECT_TRUE(backend->exists(mount_point + "/./hello.txt"));
}

// ===================================================================
// 8. Thread Safety Tests (2 tests)
// ===================================================================

TEST_F(DwarfsBackendMountedTest, ConcurrentReadsSucceed) {
  const int num_threads = 10;
  std::atomic<int> success_count{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &success_count]() {
      auto result = backend->open(mount_point + "/hello.txt", O_RDONLY);
      if (result.is_ok()) {
        auto handle = std::move(result).unwrap();
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

TEST_F(DwarfsBackendMountedTest, ConcurrentDirectoryListsSucceed) {
  const int num_threads = 10;
  std::atomic<int> success_count{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([this, &success_count]() {
      auto result = backend->list_directory(mount_point);
      if (result.is_ok()) {
        auto iter = std::move(result).unwrap();
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

TEST_F(DwarfsBackendTest, OperationsOnUnmountedBackendFail) {
  EXPECT_FALSE(backend->exists(mount_point + "/hello.txt"));
  EXPECT_FALSE(backend->is_file(mount_point + "/hello.txt"));
  EXPECT_FALSE(backend->is_directory(mount_point));
  EXPECT_TRUE(backend->file_size(mount_point + "/hello.txt").is_err());
  EXPECT_TRUE(backend->open(mount_point + "/hello.txt", O_RDONLY).is_err());
  EXPECT_TRUE(backend->list_directory(mount_point).is_err());
}

TEST_F(DwarfsBackendMountedTest, InvalidOperationsReturnProperErrors) {
  // Open with write flags should fail (read-only archive)
  EXPECT_TRUE(backend->open(mount_point + "/hello.txt", O_WRONLY).is_err());
  EXPECT_TRUE(backend->open(mount_point + "/hello.txt", O_RDWR).is_err());

  // List non-directory should fail
  EXPECT_TRUE(backend->list_directory(mount_point + "/hello.txt").is_err());

  // File operations on directory should fail
  EXPECT_TRUE(backend->file_size(mount_point + "/subdir").is_err());
}

// ===================================================================
// 10. Performance Tests
// ===================================================================

TEST_F(DwarfsBackendTest, ReadLargeFilePerformance) {
  std::string archive = fixtures_path + "large.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  if (mount_result.is_err()) {
    GTEST_SKIP() << "Large test fixture not available";
  }

  auto handle_result = backend->open(mount_point + "/10mb.bin", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

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

  // Performance expectation: should read 10MB in under 2 seconds
  // DwarFS should be faster than ZIP due to better compression and native seek
  EXPECT_LT(duration.count(), 2000) << "Reading 10MB took " << duration.count() << "ms";
}

TEST_F(DwarfsBackendTest, RandomAccessPerformance) {
  std::string archive = fixtures_path + "large.dwarfs";
  auto mount_result = backend->mount(archive, mount_point);
  if (mount_result.is_err()) {
    GTEST_SKIP() << "Large test fixture not available";
  }

  auto handle_result = backend->open(mount_point + "/10mb.bin", O_RDONLY);
  ASSERT_TRUE(handle_result.is_ok());
  auto handle = std::move(handle_result).unwrap();

  auto start = std::chrono::high_resolution_clock::now();

  // Perform 100 random seeks
  char buffer[1024];
  for (int i = 0; i < 100; i++) {
    off_t pos = (i * 97531) % (10 * 1024 * 1024);  // Random-ish position
    handle->seek(pos, SEEK_SET);
    handle->read(buffer, sizeof(buffer));
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  // Native seek should be very fast (no file reopening)
  EXPECT_LT(duration.count(), 1000) << "100 random seeks took " << duration.count() << "ms";
}