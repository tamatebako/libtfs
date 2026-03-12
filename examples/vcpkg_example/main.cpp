/**
 * @file main.cpp
 * @brief Example demonstrating libtfs unified interface for multiple archive formats
 */

#include <tebako/fs/backend_factory.h>
#include <tebako/fs/file_handle.h>
#include <tebako/fs/directory_iterator.h>

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

using namespace tebako::fs;

/**
 * @brief Display file information
 */
void print_file_info(Backend& backend, const std::string& path)
{
  if (backend.is_file(path)) {
    auto size = backend.file_size(path);
    auto mtime = backend.modification_time(path);
    auto perms = backend.permissions(path);

    std::cout << "  File: " << path << "\n"
              << "    Size: " << size << " bytes\n"
              << "    Modified: " << mtime << "\n"
              << "    Permissions: " << std::oct << perms << std::dec << "\n";
  }
}

/**
 * @brief List directory contents recursively
 */
void list_directory(Backend& backend, const std::string& path, int depth = 0)
{
  std::string indent(depth * 2, ' ');

  auto iter = backend.list_directory(path);
  if (!iter) {
    std::cerr << indent << "Failed to list directory: " << path << "\n";
    return;
  }

  while (iter->has_next()) {
    auto entry = iter->next();
    std::string full_path = path + "/" + entry.name;

    if (entry.is_directory) {
      std::cout << indent << "[DIR]  " << entry.name << "\n";
      list_directory(backend, full_path, depth + 1);
    }
    else {
      std::cout << indent << "[FILE] " << entry.name << " (" << entry.size << " bytes)\n";
    }
  }
}

/**
 * @brief Read and display file contents
 */
void read_file(Backend& backend, const std::string& path)
{
  auto handle = backend.open(path, O_RDONLY);
  if (!handle) {
    std::cerr << "Failed to open file: " << path << "\n";
    return;
  }

  std::cout << "\n--- Contents of " << path << " ---\n";

  char buffer[4096];
  ssize_t bytes_read;

  while ((bytes_read = handle->read(buffer, sizeof(buffer))) > 0) {
    std::cout.write(buffer, bytes_read);
  }

  std::cout << "\n--- End of file ---\n\n";
}

/**
 * @brief Process a single archive file
 */
void process_archive(const std::string& archive_path)
{
  std::cout << "\n" << std::string(70, '=') << "\n";
  std::cout << "Processing: " << archive_path << "\n";
  std::cout << std::string(70, '=') << "\n";

  // Create backend using factory
  auto backend = BackendFactory::create_from_file(archive_path);
  if (!backend) {
    std::cerr << "ERROR: Failed to create backend for " << archive_path << "\n";
    return;
  }

  std::cout << "Backend: " << backend->backend_name() << "\n";
  std::cout << "Version: " << backend->backend_version() << "\n";

  // Mount the archive
  std::string mount_point = "/mnt/" + backend->backend_name();
  if (!backend->mount(archive_path, mount_point)) {
    std::cerr << "ERROR: Failed to mount " << archive_path << "\n";
    return;
  }

  std::cout << "Mounted at: " << mount_point << "\n\n";

  // List contents
  std::cout << "Directory structure:\n";
  list_directory(*backend, mount_point);

  // Read a sample file if it exists
  std::string sample_file = mount_point + "/README.txt";
  if (backend->exists(sample_file)) {
    read_file(*backend, sample_file);
  }

  // Demonstrate file operations
  std::cout << "\nFile operations demo:\n";
  auto iter = backend->list_directory(mount_point);
  if (iter && iter->has_next()) {
    auto entry = iter->next();
    if (!entry.is_directory) {
      std::string file_path = mount_point + "/" + entry.name;
      print_file_info(*backend, file_path);

      // Demonstrate seeking
      auto handle = backend->open(file_path, O_RDONLY);
      if (handle) {
        std::cout << "\nSeek operations:\n";
        std::cout << "  Initial position: " << handle->tell() << "\n";

        handle->seek(10, SEEK_SET);
        std::cout << "  After SEEK_SET(10): " << handle->tell() << "\n";

        handle->seek(5, SEEK_CUR);
        std::cout << "  After SEEK_CUR(5): " << handle->tell() << "\n";

        handle->seek(-10, SEEK_END);
        std::cout << "  After SEEK_END(-10): " << handle->tell() << "\n";
      }
    }
  }

  // Unmount
  backend->unmount();
  std::cout << "\nUnmounted successfully.\n";
}

int main(int argc, char* argv[])
{
  std::cout << "libtfs Example - Unified Archive Interface\n";
  std::cout << "==========================================\n";

  // Process command-line archives or use defaults
  std::vector<std::string> archives;

  if (argc > 1) {
    for (int i = 1; i < argc; ++i) {
      archives.push_back(argv[i]);
    }
  }
  else {
    // Use built-in test archives
    archives = {"test_archives/sample.dwarfs", "test_archives/sample.zip"};
  }

  // Process each archive
  for (const auto& archive : archives) {
    try {
      process_archive(archive);
    }
    catch (const std::exception& e) {
      std::cerr << "ERROR: Exception processing " << archive << ": " << e.what() << "\n";
    }
  }

  std::cout << "\n" << std::string(70, '=') << "\n";
  std::cout << "Example completed successfully!\n";

  return 0;
}