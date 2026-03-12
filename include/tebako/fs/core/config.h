/**
 * @file config.h
 * @brief Configuration objects for filesystem creation
 *
 * Provides a builder-style API for configuring and creating filesystem
 * instances with sensible defaults.
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#pragma once

#include <tebako/fs/core/result.h>
#include <tebako/fs/filesystem.h>

#include <cstdint>
#include <memory>
#include <string>

namespace tebako {
namespace fs {

/**
 * @brief Log level for filesystem operations
 */
enum class LogLevel : int {
  None = 0,      ///< No logging
  Error = 1,     ///< Only errors
  Warning = 2,   ///< Warnings and errors
  Info = 3,      ///< Info, warnings, and errors
  Debug = 4,     ///< Debug and above
  Trace = 5      ///< All messages
};

/**
 * @brief Configuration for filesystem creation
 *
 * Provides all configuration options for creating a filesystem instance
 * with sensible defaults.
 *
 * @example
 * @code
 * FsConfig config;
 * config.archive_path = "/path/to/archive.zip";
 * config.mount_point = "/mnt/data";
 * config.cache_size = 4096;
 * config.log_level = LogLevel::Warning;
 *
 * auto result = config.create();
 * if (result.is_ok()) {
 *     std::unique_ptr<FileSystem> fs = std::move(result).unwrap();
 *     // Use filesystem...
 * }
 * @endcode
 */
struct FsConfig {
  // Required parameters
  std::string archive_path;     ///< Path to archive file
  std::string mount_point;      ///< Virtual mount point

  // Optional parameters with defaults
  size_t cache_size = 1024;     ///< Cache size in entries
  int num_workers = 2;          ///< Number of worker threads
  bool enable_logging = false;  ///< Enable logging
  LogLevel log_level = LogLevel::Warning;  ///< Log level

  /**
   * @brief Create filesystem from this configuration
   *
   * Auto-detects backend format from archive path.
   *
   * @return Result containing FileSystem or Error
   */
  Result<std::unique_ptr<FileSystem>> create() const;

  /**
   * @brief Create filesystem from memory
   *
   * @param data Archive data
   * @param size Data size
   * @return Result containing FileSystem or Error
   */
  Result<std::unique_ptr<FileSystem>> create_from_memory(
      const void* data, size_t size) const;

  /**
   * @brief Validate configuration
   *
   * @return true if configuration is valid
   */
  bool is_valid() const;

  /**
   * @brief Get validation error message
   *
   * @return Error message, or empty string if valid
   */
  std::string validation_error() const;
};

/**
 * @brief Builder for filesystem configuration
 *
 * Provides a fluent API for building filesystem configurations.
 *
 * @example
 * @code
 * auto result = FsBuilder()
 *     .archive("/path/to/archive.zip")
 *     .mount_point("/mnt/data")
 *     .cache_size(4096)
 *     .log_level(LogLevel::Warning)
 *     .create();
 *
 * if (result.is_ok()) {
 *     std::unique_ptr<FileSystem> fs = std::move(result).unwrap();
 *     // Use filesystem...
 * }
 * @endcode
 */
class FsBuilder {
 public:
  FsBuilder() = default;

  /**
   * @brief Set archive path
   * @param path Path to archive file
   * @return Reference to this builder
   */
  FsBuilder& archive(std::string path) {
    config_.archive_path = std::move(path);
    return *this;
  }

  /**
   * @brief Set mount point
   * @param point Virtual mount point path
   * @return Reference to this builder
   */
  FsBuilder& mount_point(std::string point) {
    config_.mount_point = std::move(point);
    return *this;
  }

  /**
   * @brief Set cache size
   * @param size Cache size in entries
   * @return Reference to this builder
   */
  FsBuilder& cache_size(size_t size) {
    config_.cache_size = size;
    return *this;
  }

  /**
   * @brief Set number of worker threads
   * @param workers Number of workers
   * @return Reference to this builder
   */
  FsBuilder& workers(int workers) {
    config_.num_workers = workers;
    return *this;
  }

  /**
   * @brief Enable or disable logging
   * @param enable True to enable logging
   * @return Reference to this builder
   */
  FsBuilder& enable_logging(bool enable = true) {
    config_.enable_logging = enable;
    return *this;
  }

  /**
   * @brief Set log level
   * @param level Log level
   * @return Reference to this builder
   */
  FsBuilder& log_level(LogLevel level) {
    config_.log_level = level;
    config_.enable_logging = (level != LogLevel::None);
    return *this;
  }

  /**
   * @brief Build configuration
   *
   * Does not validate - use create() for validation.
   *
   * @return Configuration object
   */
  FsConfig build() const {
    return config_;
  }

  /**
   * @brief Create filesystem from configuration
   *
   * Validates configuration and creates filesystem.
   *
   * @return Result containing FileSystem or Error
   */
  Result<std::unique_ptr<FileSystem>> create() const {
    return config_.create();
  }

  /**
   * @brief Create filesystem from memory
   *
   * @param data Archive data
   * @param size Data size
   * @return Result containing FileSystem or Error
   */
  Result<std::unique_ptr<FileSystem>> create_from_memory(
      const void* data, size_t size) const {
    return config_.create_from_memory(data, size);
  }

 private:
  FsConfig config_;
};

}  // namespace fs
}  // namespace tebako
