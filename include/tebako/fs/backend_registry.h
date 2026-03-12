/**
 * @file backend_registry.h
 * @brief Backend Registry for dynamic backend registration
 *
 * Provides a plugin-style architecture where backends can register
 * themselves at startup, removing the need for hardcoded backend
 * detection in the factory.
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#pragma once

#include <tebako/fs/filesystem.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace tebako {
namespace fs {

/**
 * @brief Backend metadata for registration
 */
struct BackendInfo {
  std::string name;                     ///< Backend name (e.g., "zip", "dwarfs")
  std::string description;              ///< Human-readable description
  std::vector<std::string> extensions;  ///< Supported file extensions (e.g., ".zip", ".jar")
  int priority = 0;                     ///< Detection priority (higher = checked first)
};

/**
 * @brief Registry for filesystem backends
 *
 * Allows backends to self-register at program startup using static
 * initialization. This eliminates hardcoded backend detection and
 * makes it easy to add new backends without modifying the factory.
 *
 * Thread Safety: All methods are thread-safe.
 *
 * @example
 * @code
 * // In a backend implementation file:
 * static bool registered = []{
 *     BackendRegistry::instance().register_backend(
 *         {"zip", "ZIP archive backend", {".zip", ".jar"}, 10},
 *         []{ return std::make_unique<ZipBackend>(); },
 *         [](const void* data, size_t size){ return detect_zip(data, size); }
 *     );
 *     return true;
 * }();
 * @endcode
 */
class BackendRegistry {
 public:
  /// Factory function type - creates a new filesystem instance
  using Factory = std::function<std::unique_ptr<FileSystem>()>;

  /// Memory detection function - returns true if data matches format
  using MemoryDetector = std::function<bool(const void* data, size_t size)>;

  /// File detection function - returns true if file matches format
  using FileDetector = std::function<bool(const std::string& path)>;

  /**
   * @brief Get singleton instance
   */
  static BackendRegistry& instance();

  /**
   * @brief Register a backend
   *
   * @param info Backend metadata
   * @param factory Factory function to create backend instances
   * @param memory_detector Function to detect format from memory (optional)
   * @param file_detector Function to detect format from file path (optional)
   */
  void register_backend(const BackendInfo& info,
                        Factory factory,
                        MemoryDetector memory_detector = nullptr,
                        FileDetector file_detector = nullptr);

  /**
   * @brief Create backend by detecting format from memory
   *
   * @param data Archive data
   * @param size Data size
   * @return FileSystem instance, or nullptr if no backend detected
   */
  std::unique_ptr<FileSystem> create_from_memory(const void* data, size_t size);

  /**
   * @brief Create backend by detecting format from file
   *
   * @param path Path to archive file
   * @return FileSystem instance, or nullptr if no backend detected
   */
  std::unique_ptr<FileSystem> create_from_file(const std::string& path);

  /**
   * @brief Create backend by name
   *
   * @param name Backend name
   * @return FileSystem instance, or nullptr if not found
   */
  std::unique_ptr<FileSystem> create_by_name(std::string_view name);

  /**
   * @brief Create backend by file extension
   *
   * @param path File path (extracts extension)
   * @return FileSystem instance, or nullptr if no backend matches
   */
  std::unique_ptr<FileSystem> create_by_extension(const std::string& path);

  /**
   * @brief Get list of registered backend names
   */
  std::vector<std::string> backend_names() const;

  /**
   * @brief Get backend info by name
   *
   * @param name Backend name
   * @return Pointer to BackendInfo, or nullptr if not found
   */
  const BackendInfo* get_info(std::string_view name) const;

  /**
   * @brief Check if a backend is registered
   */
  bool has_backend(std::string_view name) const;

  /**
   * @brief Clear all registered backends (for testing)
   */
  void clear();

 private:
  BackendRegistry() = default;
  ~BackendRegistry() = default;

  BackendRegistry(const BackendRegistry&) = delete;
  BackendRegistry& operator=(const BackendRegistry&) = delete;

  struct RegisteredBackend {
    BackendInfo info;
    Factory factory;
    MemoryDetector memory_detector;
    FileDetector file_detector;
  };

  std::vector<RegisteredBackend> backends_;
  mutable std::mutex mutex_;

  // Sort backends by priority (highest first)
  void sort_by_priority();
};

/**
 * @brief Helper class for automatic backend registration
 *
 * Use the TEBAKO_REGISTER_BACKEND macro instead of this directly.
 */
class BackendRegistrar {
 public:
  BackendRegistrar(const BackendInfo& info,
                   BackendRegistry::Factory factory,
                   BackendRegistry::MemoryDetector memory_detector = nullptr,
                   BackendRegistry::FileDetector file_detector = nullptr)
  {
    BackendRegistry::instance().register_backend(info, std::move(factory), std::move(memory_detector),
                                                 std::move(file_detector));
  }
};

}  // namespace fs
}  // namespace tebako

/**
 * @brief Register a backend at static initialization time
 *
 * Place this macro in a backend's implementation file to automatically
 * register it when the program starts.
 *
 * @param name Backend name (must be a valid C++ identifier)
 * @param info BackendInfo struct with metadata
 * @param factory Lambda returning std::unique_ptr<FileSystem>
 * @param memory_detector Lambda for memory format detection (optional)
 * @param file_detector Lambda for file format detection (optional)
 *
 * @example
 * @code
 * static const tebako::fs::BackendInfo zip_info = {
 *     "zip",
 *     "ZIP archive backend",
 *     {".zip", ".jar", ".apk"},
 *     10  // priority
 * };
 *
 * TEBAKO_REGISTER_BACKEND(zip, zip_info,
 *     []{ return std::make_unique<ZipBackend>(); },
 *     [](const void* data, size_t size) { return detect_zip_magic(data, size); },
 *     [](const std::string& path) { return has_zip_extension(path); }
 * );
 * @endcode
 */
#define TEBAKO_REGISTER_BACKEND(name, info, factory, ...) \
  static ::tebako::fs::BackendRegistrar _tebako_backend_registrar_##name((info), (factory), ##__VA_ARGS__)
