/**
 * @file path.cpp
 * @brief Path value class implementation
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 */

#include <tebako/fs/core/path.h>
#include <algorithm>
#include <cctype>
#include <vector>

namespace tebako {
namespace fs {

// =========================================================================
// Private Helpers
// =========================================================================

std::string Path::normalize(std::string_view p)
{
  if (p.empty()) {
    return "";
  }

  std::string result(p);

  // Track if path is absolute BEFORE any modifications
  bool is_absolute = (!result.empty() && result[0] == '/');

  // Step 1: Collapse multiple consecutive slashes
  size_t pos = 0;
  while ((pos = result.find("//", pos)) != std::string::npos) {
    result.replace(pos, 2, "/");
  }

  // Step 2: Remove embedded "/./" segments
  pos = 0;
  while ((pos = result.find("/./", pos)) != std::string::npos) {
    result.replace(pos, 3, "/");
  }

  // Step 3: Handle leading "./"
  if (result.size() >= 2 && result[0] == '.' && result[1] == '/') {
    result = result.substr(2);
  }

  // Step 4: Handle trailing "/." (e.g., "/path/." -> "/path", "/." -> "")
  if (result.size() >= 2 && result.substr(result.size() - 2) == "/.") {
    result = result.substr(0, result.size() - 2);
  }

  // Step 5: Handle ".." components by building a new path
  // This is simpler and more correct than in-place modification
  std::vector<std::string> components;

  // Split by '/'
  std::string segment;
  for (size_t i = 0; i < result.size(); ++i) {
    if (result[i] == '/') {
      if (!segment.empty()) {
        if (segment == "..") {
          // Go up one level
          if (!components.empty() && components.back() != "..") {
            components.pop_back();
          }
          else if (!is_absolute) {
            // Can't go above root for absolute paths
            components.push_back("..");
          }
        }
        else if (segment != ".") {
          components.push_back(segment);
        }
        segment.clear();
      }
    }
    else {
      segment += result[i];
    }
  }

  // Don't forget the last segment
  if (!segment.empty()) {
    if (segment == "..") {
      if (!components.empty() && components.back() != "..") {
        components.pop_back();
      }
      else if (!is_absolute) {
        components.push_back("..");
      }
    }
    else if (segment != ".") {
      components.push_back(segment);
    }
  }

  // Reconstruct the path
  std::string normalized;

  for (size_t i = 0; i < components.size(); ++i) {
    if (i > 0) {
      normalized += "/";
    }
    else if (is_absolute) {
      normalized += "/";
    }
    normalized += components[i];
  }

  // Handle empty result for absolute paths (root)
  if (is_absolute && components.empty()) {
    normalized = "/";
  }

  return normalized;
}

// =========================================================================
// Component Extraction
// =========================================================================

Path Path::parent() const
{
  if (path_.empty() || path_ == "/") {
    return Path();
  }

  size_t last_slash = path_.rfind('/');
  if (last_slash == std::string::npos) {
    // No slash - this is a relative filename with no parent
    return Path();
  }

  if (last_slash == 0) {
    // Only root slash - parent is root
    return Path("/");
  }

  return Path(path_.substr(0, last_slash));
}

std::string Path::filename() const
{
  if (path_.empty() || path_ == "/") {
    return "";
  }

  size_t last_slash = path_.rfind('/');
  if (last_slash == std::string::npos) {
    // No slash - entire path is filename
    return path_;
  }

  return path_.substr(last_slash + 1);
}

std::string Path::extension() const
{
  std::string name = filename();
  if (name.empty()) {
    return "";
  }

  // Don't treat hidden files (starting with .) as having extension
  if (name.front() == '.') {
    return "";
  }

  size_t dot_pos = name.rfind('.');
  if (dot_pos == std::string::npos || dot_pos == 0) {
    return "";
  }

  return name.substr(dot_pos);
}

std::string Path::stem() const
{
  std::string name = filename();
  if (name.empty()) {
    return "";
  }

  // Handle hidden files
  if (name.front() == '.') {
    return name;
  }

  size_t dot_pos = name.rfind('.');
  if (dot_pos == std::string::npos || dot_pos == 0) {
    return name;
  }

  return name.substr(0, dot_pos);
}

// =========================================================================
// Path Operations
// =========================================================================

Path Path::join(const Path& other) const
{
  if (other.empty()) {
    return *this;
  }
  if (empty()) {
    return other;
  }

  // If other is absolute, just return it
  if (other.is_absolute()) {
    // Remove leading slash and join
    std::string other_path = other.path_.substr(1);
    return Path(path_ + "/" + other_path);
  }

  return Path(path_ + "/" + other.path_);
}

Path Path::relative_to(const Path& path, const Path& base)
{
  if (path.empty() || base.empty()) {
    return Path();
  }

  // Both must be absolute or both relative
  if (path.is_absolute() != base.is_absolute()) {
    return Path();
  }

  std::string_view pv = path.view();
  std::string_view bv = base.view();

  // Check if base is longer than path
  if (bv.length() > pv.length()) {
    return Path();
  }

  // Check if path starts with base
  if (pv.substr(0, bv.length()) != bv) {
    return Path();
  }

  // If exact match, return empty (same directory)
  if (pv.length() == bv.length()) {
    return Path();
  }

  // Check for separator after base
  if (bv.length() > 0 && bv.back() != '/') {
    // Base doesn't end with slash, so next char must be slash
    if (pv[bv.length()] != '/') {
      return Path();
    }
    return Path(pv.substr(bv.length() + 1));
  }

  // Base ends with slash
  return Path(pv.substr(bv.length()));
}

// =========================================================================
// Backend-Specific Utilities
// =========================================================================

std::string Path::without_leading_slash() const
{
  if (path_.empty()) {
    return "";
  }
  if (path_.front() == '/') {
    return path_.substr(1);
  }
  return path_;
}

std::string Path::with_trailing_slash() const
{
  if (path_.empty()) {
    return "/";
  }
  if (path_ == "/") {
    return "/";
  }
  if (path_.back() == '/') {
    return path_;
  }
  return path_ + "/";
}

bool Path::has_extension(std::string_view ext) const
{
  std::string current_ext = extension();
  if (current_ext.empty() || ext.empty()) {
    return false;
  }

  // Normalize: ensure both start with dot
  std::string normalized_ext;
  if (ext.front() == '.') {
    normalized_ext = ext;
  }
  else {
    normalized_ext = ".";
    normalized_ext += ext;
  }

  // Case-insensitive comparison
  if (current_ext.length() != normalized_ext.length()) {
    return false;
  }

  return std::equal(current_ext.begin(), current_ext.end(), normalized_ext.begin(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
  });
}

}  // namespace fs
}  // namespace tebako
