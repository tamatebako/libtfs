/**
 * @file test_unified_interface.cpp
 * @brief Tests for unified interface across all backends
 */

#include <gtest/gtest.h>
#include <tebako/fs/backend_factory.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>
#include <memory>
#include <vector>
#include <filesystem>
#include <fcntl.h>

using namespace tebako::fs;
namespace fs = std::filesystem;
using Backend = FileSystem;

class UnifiedInterfaceTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Get paths to test fixtures
    zip_archive_path = "tests/fixtures/zip/simple.zip";
    dwarfs_archive_path = "tests/fixtures/dwarfs/simple.dwarfs";
  }

  std::string zip_archive_path;
  std::string dwarfs_archive_path;
};

// Test backend creation from file with auto-detection
TEST_F(UnifiedInterfaceTest, CreateBackendFromFile) {
  // Test ZIP
  auto zip = BackendFactory::create_from_file(zip_archive_path);
  ASSERT_NE(zip, nullptr);
  EXPECT_EQ(zip->backend_name(), "ZIP");

  // Test DwarFS
  auto dwarfs = BackendFactory::create_from_file(dwarfs_archive_path);
  ASSERT_NE(dwarfs, nullptr);
  EXPECT_EQ(dwarfs->backend_name(), "DwarFS");
}

// Test that all backends provide the same API
TEST_F(UnifiedInterfaceTest, IdenticalAPIBehavior) {
  struct BackendTest {
    std::unique_ptr<Backend> backend;
    std::string archive_path;
  };
  std::vector<BackendTest> backends;

  // Create all available backends with their paths
  auto zip = BackendFactory::create_from_file(zip_archive_path);
  if (zip) backends.push_back({std::move(zip), zip_archive_path});

  auto dwarfs = BackendFactory::create_from_file(dwarfs_archive_path);
  if (dwarfs) backends.push_back({std::move(dwarfs), dwarfs_archive_path});

  // Test each backend with identical API calls
  int test_id = 0;
  for (auto& bt : backends) {
    SCOPED_TRACE("Testing backend: " + bt.backend->backend_name());

    // Mount the archive - use unique mount point for each backend
    std::string mount_point = "/mnt/test" + std::to_string(++test_id);
    ASSERT_TRUE(bt.backend->mount(bt.archive_path, mount_point));

    // Test exists
    EXPECT_TRUE(bt.backend->exists(mount_point));

    // Test is_directory
    EXPECT_TRUE(bt.backend->is_directory(mount_point));

    // Test list_directory
    auto iter = bt.backend->list_directory(mount_point);
    ASSERT_NE(iter, nullptr);
    EXPECT_TRUE(iter->has_next());

    // Get first entry and test file operations
    auto entry = iter->next();
    std::string file_path = mount_point + "/" + entry.name;

    if (!entry.is_directory) {
      // Test is_file
      EXPECT_TRUE(bt.backend->is_file(file_path));

      // Test file_size
      auto size = bt.backend->file_size(file_path);
      EXPECT_GT(size, 0);

      // Test open
      auto handle = bt.backend->open(file_path, O_RDONLY);
      ASSERT_NE(handle, nullptr);

      // Test read
      char buffer[1024];
      ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
      EXPECT_GE(bytes_read, 0);

      // Test tell
      auto pos = handle->tell();
      EXPECT_GE(pos, 0);

      // Test seek
      EXPECT_EQ(handle->seek(0, SEEK_SET), 0);
      EXPECT_EQ(handle->tell(), 0);
    }

    // Unmount
    bt.backend->unmount();
    EXPECT_FALSE(bt.backend->is_mounted());
  }
}

// Test polymorphic behavior through base class pointer
TEST_F(UnifiedInterfaceTest, PolymorphicBehavior) {
  struct BackendTest {
    std::unique_ptr<Backend> backend;
    std::string archive_path;
  };
  std::vector<BackendTest> backends;

  backends.push_back({BackendFactory::create_from_file(zip_archive_path), zip_archive_path});
  backends.push_back({BackendFactory::create_from_file(dwarfs_archive_path), dwarfs_archive_path});

  // Store in base class pointers and test polymorphism
  for (const auto& bt : backends) {
    if (!bt.backend) continue;

    SCOPED_TRACE("Testing polymorphic behavior for: " + bt.backend->backend_name());

    // All virtual methods should work correctly
    std::string mount_point = "/mnt/test";
    EXPECT_TRUE(bt.backend->mount(bt.archive_path, mount_point));

    // Test that backend-specific implementations are called
    EXPECT_FALSE(bt.backend->backend_name().empty());
    EXPECT_FALSE(bt.backend->backend_version().empty());

    bt.backend->unmount();
  }
}

// Test format detection works correctly
TEST_F(UnifiedInterfaceTest, FormatDetection) {
  // ZIP should be detected
  auto zip = BackendFactory::create_from_file(zip_archive_path);
  ASSERT_NE(zip, nullptr);
  EXPECT_EQ(zip->backend_name(), "ZIP");

  // DwarFS should be detected
  auto dwarfs = BackendFactory::create_from_file(dwarfs_archive_path);
  ASSERT_NE(dwarfs, nullptr);
  EXPECT_EQ(dwarfs->backend_name(), "DwarFS");

  // Invalid file should return nullptr
  auto invalid = BackendFactory::create_from_file("nonexistent.xyz");
  EXPECT_EQ(invalid, nullptr);
}

// Test consistent error handling across backends
TEST_F(UnifiedInterfaceTest, ConsistentErrorHandling) {
  struct BackendTest {
    std::unique_ptr<Backend> backend;
    std::string archive_path;
  };
  std::vector<BackendTest> backends;

  backends.push_back({BackendFactory::create_from_file(zip_archive_path), zip_archive_path});
  backends.push_back({BackendFactory::create_from_file(dwarfs_archive_path), dwarfs_archive_path});

  for (auto& bt : backends) {
    if (!bt.backend) continue;

    SCOPED_TRACE("Testing error handling for: " + bt.backend->backend_name());

    std::string mount_point = "/mnt/test";
    ASSERT_TRUE(bt.backend->mount(bt.archive_path, mount_point));

    // Non-existent file
    EXPECT_FALSE(bt.backend->exists(mount_point + "/nonexistent.txt"));
    EXPECT_FALSE(bt.backend->is_file(mount_point + "/nonexistent.txt"));

    // Open non-existent file should return nullptr
    auto handle = bt.backend->open(mount_point + "/nonexistent.txt", O_RDONLY);
    EXPECT_EQ(handle, nullptr);

    bt.backend->unmount();
    EXPECT_FALSE(bt.backend->is_mounted());
  }
}

// Test metadata consistency across backends
TEST_F(UnifiedInterfaceTest, MetadataConsistency) {
  struct BackendTest {
    std::unique_ptr<Backend> backend;
    std::string archive_path;
  };
  std::vector<BackendTest> backends;

  backends.push_back({BackendFactory::create_from_file(zip_archive_path), zip_archive_path});
  backends.push_back({BackendFactory::create_from_file(dwarfs_archive_path), dwarfs_archive_path});

  for (auto& bt : backends) {
    if (!bt.backend) continue;

    SCOPED_TRACE("Testing metadata for: " + bt.backend->backend_name());

    std::string mount_point = "/mnt/test";
    ASSERT_TRUE(bt.backend->mount(bt.archive_path, mount_point));

    auto iter = bt.backend->list_directory(mount_point);
    ASSERT_NE(iter, nullptr);

    if (iter->has_next()) {
      auto entry = iter->next();
      std::string path = mount_point + "/" + entry.name;

      if (!entry.is_directory) {
        // All backends should provide these metadata
        auto size = bt.backend->file_size(path);
        EXPECT_GE(size, 0);

        auto mtime = bt.backend->modification_time(path);
        EXPECT_GE(mtime, 0);

        auto perms = bt.backend->permissions(path);
        EXPECT_GE(perms, 0);
      }
    }

    bt.backend->unmount();
    EXPECT_FALSE(bt.backend->is_mounted());
  }
}

// Test that backends can be used interchangeably
TEST_F(UnifiedInterfaceTest, InterchangeableBackends) {
  // Function that works with any backend
  auto test_backend = [](Backend& backend, const std::string& archive_path) {
    std::string mount_point = "/mnt/test";
    EXPECT_TRUE(backend.mount(archive_path, mount_point));
    EXPECT_TRUE(backend.exists(mount_point));
    backend.unmount();
  };

  // Test with different backends
  auto zip = BackendFactory::create_from_file(zip_archive_path);
  if (zip) {
    SCOPED_TRACE("Testing with ZIP backend");
    test_backend(*zip, zip_archive_path);
  }

  auto dwarfs = BackendFactory::create_from_file(dwarfs_archive_path);
  if (dwarfs) {
    SCOPED_TRACE("Testing with DwarFS backend");
    test_backend(*dwarfs, dwarfs_archive_path);
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}