/**
 * @file config.cpp
 * @brief Configuration implementation
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#include <tebako/fs/core/config.h>
#include <tebako/fs/backend_factory.h>

#include <filesystem>

namespace tebako {
namespace fs {

// ===================================================================
// FsConfig Implementation
// ===================================================================

Result<std::unique_ptr<FileSystem>> FsConfig::create() const {
  // Validate configuration
  if (!is_valid()) {
    return Err(ErrorCode::InvalidArgument, validation_error());
  }

  // Create filesystem using factory
  std::unique_ptr<FileSystem> fs = BackendFactory::create_from_file(archive_path);
  if (!fs) {
    return Err(ErrorCode::IOError, "Failed to create filesystem",
               archive_path);
  }

  // Mount the filesystem
  auto result = fs->mount(archive_path, mount_point);
  if (result.is_err()) {
    return Err(result.error());
  }

  return Ok(std::move(fs));
}

Result<std::unique_ptr<FileSystem>> FsConfig::create_from_memory(
    const void* data, size_t size) const {
  // Validate basic requirements
  if (!data || size == 0) {
    return Err(ErrorCode::InvalidArgument, "Invalid memory data");
  }

  if (mount_point.empty()) {
    return Err(ErrorCode::InvalidArgument, "Mount point not specified");
  }

  // Create filesystem using factory
  std::unique_ptr<FileSystem> fs = BackendFactory::create_from_memory(data, size);
  if (!fs) {
    return Err(ErrorCode::IOError, "Failed to detect archive format from memory");
  }

  // Mount the filesystem
  auto result = fs->mount_from_memory(data, size, mount_point);
  if (result.is_err()) {
    return Err(result.error());
  }

  return Ok(std::move(fs));
}

bool FsConfig::is_valid() const {
  return validation_error().empty();
}

std::string FsConfig::validation_error() const {
  if (archive_path.empty()) {
    return "Archive path not specified";
  }

  if (mount_point.empty()) {
    return "Mount point not specified";
  }

  if (!std::filesystem::exists(archive_path)) {
    return "Archive file does not exist: " + archive_path;
  }

  if (cache_size == 0) {
    return "Cache size must be greater than 0";
  }

  if (num_workers < 1) {
    return "Number of workers must be at least 1";
  }

  return "";  // Valid
}

}  // namespace fs
}  // namespace tebako
