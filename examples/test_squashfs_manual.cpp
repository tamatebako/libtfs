/**
 * Manual test program for SquashFS backend
 *
 * This program demonstrates basic usage of the SquashFS backend.
 *
 * Usage:
 *   ./test_squashfs_manual <path-to-squashfs-file>
 *
 * Example:
 *   ./test_squashfs_manual test.sqfs
 */

#include <tebako/fs/backend_factory.h>
#include <tebako/fs/backends/squashfs_backend.h>
#include <iostream>
#include <iomanip>

using namespace tebako::fs;

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <squashfs-file>" << std::endl;
    std::cerr << "Example: " << argv[0] << " test.sqfs" << std::endl;
    return 1;
  }

  std::string archive_path = argv[1];
  std::string mount_point = "/mnt/test";

  std::cout << "=== SquashFS Backend Manual Test ===" << std::endl;
  std::cout << std::endl;

  // Test 1: Create backend directly
  std::cout << "Test 1: Creating SquashFS backend..." << std::endl;
  auto backend = BackendFactory::create_squashfs();
  if (!backend) {
    std::cerr << "ERROR: Failed to create SquashFS backend" << std::endl;
    return 1;
  }
  std::cout << "✓ Backend created: " << backend->backend_name() << std::endl;
  std::cout << "✓ Backend version: " << backend->backend_version() << std::endl;
  std::cout << std::endl;

  // Test 2: Mount archive
  std::cout << "Test 2: Mounting archive..." << std::endl;
  std::cout << "  Archive path: " << archive_path << std::endl;
  std::cout << "  Mount point: " << mount_point << std::endl;

  if (!backend->mount(archive_path, mount_point)) {
    std::cerr << "ERROR: Failed to mount archive" << std::endl;
    std::cerr << "  Make sure the file exists and is a valid SquashFS archive" << std::endl;
    return 1;
  }
  std::cout << "✓ Archive mounted successfully" << std::endl;
  std::cout << "✓ Is mounted: " << (backend->is_mounted() ? "yes" : "no") << std::endl;
  std::cout << std::endl;

  // Test 3: Check root directory
  std::cout << "Test 3: Checking root directory..." << std::endl;
  if (backend->exists(mount_point)) {
    std::cout << "✓ Root exists" << std::endl;
  }
  else {
    std::cerr << "ERROR: Root does not exist" << std::endl;
  }

  if (backend->is_directory(mount_point)) {
    std::cout << "✓ Root is a directory" << std::endl;
  }
  else {
    std::cerr << "ERROR: Root is not a directory" << std::endl;
  }
  std::cout << std::endl;

  // Test 4: List root directory
  std::cout << "Test 4: Listing root directory..." << std::endl;
  auto iter = backend->list_directory(mount_point);
  if (!iter) {
    std::cerr << "ERROR: Failed to list root directory" << std::endl;
  }
  else {
    std::cout << "Root directory contents:" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    std::cout << std::left << std::setw(30) << "Name" << std::setw(10) << "Type" << std::setw(15) << "Size"
              << "Modified" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    int count = 0;
    while (iter->has_next()) {
      auto entry = iter->next();
      std::cout << std::left << std::setw(30) << entry.name << std::setw(10)
                << (entry.is_directory ? "[DIR]" : "[FILE]") << std::setw(15) << entry.size << entry.mtime << std::endl;
      count++;
    }
    std::cout << std::string(60, '-') << std::endl;
    std::cout << "✓ Found " << count << " entries" << std::endl;
  }
  std::cout << std::endl;

  // Test 5: Try to open a file (if exists)
  std::cout << "Test 5: File operations..." << std::endl;
  std::string test_file = mount_point + "/README";  // Common file

  if (backend->exists(test_file)) {
    std::cout << "  File '" << test_file << "' exists" << std::endl;

    if (backend->is_file(test_file)) {
      std::cout << "  ✓ Is a regular file" << std::endl;

      int64_t size = backend->file_size(test_file);
      std::cout << "  ✓ Size: " << size << " bytes" << std::endl;

      time_t mtime = backend->modification_time(test_file);
      std::cout << "  ✓ Modified: " << mtime << std::endl;

      mode_t perms = backend->permissions(test_file);
      std::cout << "  ✓ Permissions: " << std::oct << perms << std::dec << std::endl;

      // Try to open and read
      auto handle = backend->open(test_file, O_RDONLY);
      if (handle) {
        std::cout << "  ✓ File opened successfully" << std::endl;

        char buffer[256] = {0};
        ssize_t bytes_read = handle->read(buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
          std::cout << "  ✓ Read " << bytes_read << " bytes" << std::endl;
          std::cout << "  First few bytes: " << buffer << std::endl;
        }

        handle->close();
        std::cout << "  ✓ File closed" << std::endl;
      }
      else {
        std::cout << "  ✗ Failed to open file" << std::endl;
      }
    }
  }
  else {
    std::cout << "  Note: Test file '" << test_file << "' not found (this is OK)" << std::endl;
  }
  std::cout << std::endl;

  // Test 6: Auto-detection
  std::cout << "Test 6: Testing format auto-detection..." << std::endl;
  std::cout << "  Is SquashFS format: " << (BackendFactory::is_squashfs_format(archive_path) ? "yes" : "no")
            << std::endl;

  auto auto_backend = BackendFactory::create_from_file(archive_path);
  if (auto_backend) {
    std::cout << "  ✓ Auto-detected backend: " << auto_backend->backend_name() << std::endl;
  }
  else {
    std::cout << "  ✗ Failed to auto-detect format" << std::endl;
  }
  std::cout << std::endl;

  // Test 7: Unmount
  std::cout << "Test 7: Unmounting..." << std::endl;
  backend->unmount();
  std::cout << "✓ Archive unmounted" << std::endl;
  std::cout << "✓ Is mounted: " << (backend->is_mounted() ? "yes" : "no") << std::endl;
  std::cout << std::endl;

  std::cout << "=== All tests completed ===" << std::endl;
  return 0;
}