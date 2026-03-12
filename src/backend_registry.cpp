/**
 * @file backend_registry.cpp
 * @brief Backend Registry implementation
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#include <tebako/fs/backend_registry.h>

#include <algorithm>
#include <cctype>

namespace tebako {
namespace fs {

// ===================================================================
// Singleton Instance
// ===================================================================

BackendRegistry& BackendRegistry::instance()
{
  static BackendRegistry registry;
  return registry;
}

// ===================================================================
// Registration
// ===================================================================

void BackendRegistry::register_backend(const BackendInfo& info,
                                       Factory factory,
                                       MemoryDetector memory_detector,
                                       FileDetector file_detector)
{
  std::lock_guard lock(mutex_);

  // Check if already registered
  for (const auto& backend : backends_) {
    if (backend.info.name == info.name) {
      return;  // Already registered
    }
  }

  backends_.push_back({info, std::move(factory), std::move(memory_detector), std::move(file_detector)});
  sort_by_priority();
}

// ===================================================================
// Factory Methods
// ===================================================================

std::unique_ptr<FileSystem> BackendRegistry::create_from_memory(const void* data, size_t size)
{
  std::lock_guard lock(mutex_);

  for (const auto& backend : backends_) {
    if (backend.memory_detector && backend.memory_detector(data, size)) {
      return backend.factory();
    }
  }

  return nullptr;
}

std::unique_ptr<FileSystem> BackendRegistry::create_from_file(const std::string& path)
{
  std::lock_guard lock(mutex_);

  // First try file detection (magic numbers)
  for (const auto& backend : backends_) {
    if (backend.file_detector && backend.file_detector(path)) {
      return backend.factory();
    }
  }

  // Fallback to extension-based detection
  return create_by_extension(path);
}

std::unique_ptr<FileSystem> BackendRegistry::create_by_name(std::string_view name)
{
  std::lock_guard lock(mutex_);

  for (const auto& backend : backends_) {
    if (backend.info.name == name) {
      return backend.factory();
    }
  }

  return nullptr;
}

std::unique_ptr<FileSystem> BackendRegistry::create_by_extension(const std::string& path)
{
  std::lock_guard lock(mutex_);

  // Extract extension from path
  size_t dot_pos = path.rfind('.');
  if (dot_pos == std::string::npos || dot_pos == path.length() - 1) {
    return nullptr;
  }

  std::string ext = path.substr(dot_pos);
  // Convert to lowercase for comparison
  for (auto& c : ext) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  // Find backend that handles this extension
  for (const auto& backend : backends_) {
    for (const auto& backend_ext : backend.info.extensions) {
      std::string lower_ext = backend_ext;
      for (auto& c : lower_ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
      if (lower_ext == ext) {
        return backend.factory();
      }
    }
  }

  return nullptr;
}

// ===================================================================
// Query Methods
// ===================================================================

std::vector<std::string> BackendRegistry::backend_names() const
{
  std::lock_guard lock(mutex_);

  std::vector<std::string> names;
  names.reserve(backends_.size());
  for (const auto& backend : backends_) {
    names.push_back(backend.info.name);
  }
  return names;
}

const BackendInfo* BackendRegistry::get_info(std::string_view name) const
{
  std::lock_guard lock(mutex_);

  for (const auto& backend : backends_) {
    if (backend.info.name == name) {
      return &backend.info;
    }
  }
  return nullptr;
}

bool BackendRegistry::has_backend(std::string_view name) const
{
  std::lock_guard lock(mutex_);

  for (const auto& backend : backends_) {
    if (backend.info.name == name) {
      return true;
    }
  }
  return false;
}

void BackendRegistry::clear()
{
  std::lock_guard lock(mutex_);
  backends_.clear();
}

// ===================================================================
// Private Methods
// ===================================================================

void BackendRegistry::sort_by_priority()
{
  // Sort by priority descending (highest priority first)
  std::sort(backends_.begin(), backends_.end(),
            [](const RegisteredBackend& a, const RegisteredBackend& b) { return a.info.priority > b.info.priority; });
}

}  // namespace fs
}  // namespace tebako
