#pragma once

#include <dwarfs/detail/file_view_impl.h>
#include <dwarfs/file_extents_iterable.h>
#include <dwarfs/file_range.h>
#include <dwarfs/file_segment.h>
#include <dwarfs/types.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <system_error>
#include <vector>

namespace tebako {

/// Memory-backed file view implementation for DwarFS
///
/// Provides zero-copy access to an in-memory DwarFS filesystem image.
/// Thread-safe and exception-safe implementation following DwarFS patterns.
///
/// This class replaces the obsolete `mmif` interface with modern DwarFS v0.9+
/// `file_view` abstraction.
class memory_file_view_impl final : public dwarfs::detail::file_view_impl,
                                    public std::enable_shared_from_this<memory_file_view_impl> {
 public:
  /// Construct from raw memory buffer
  ///
  /// @param data Pointer to memory buffer (must remain valid for object lifetime)
  /// @param size Size of memory buffer in bytes
  /// @param path Filesystem path for identification (can be synthetic)
  ///
  /// @throws std::invalid_argument if data is null or size is zero
  memory_file_view_impl(const void* data, std::size_t size, std::filesystem::path path = "<memory>");

  ~memory_file_view_impl() override = default;

  // Disable copy (zero-copy design)
  memory_file_view_impl(const memory_file_view_impl&) = delete;
  memory_file_view_impl& operator=(const memory_file_view_impl&) = delete;

  // Disable move (shared_ptr manages lifetime)
  memory_file_view_impl(memory_file_view_impl&&) = delete;
  memory_file_view_impl& operator=(memory_file_view_impl&&) = delete;

  // --- file_view_impl interface ---

  /// Get total size of the memory buffer
  dwarfs::file_size_t size() const override { return size_; }

  /// Get the filesystem path
  std::filesystem::path const& path() const override { return path_; }

  /// Get a file segment at the specified range
  ///
  /// @param range Offset and size to read
  /// @return File segment containing the requested data
  dwarfs::file_segment segment_at(dwarfs::file_range range) const override;

  /// Get file extents information
  ///
  /// For a memory buffer, we use a single data extent covering the entire buffer.
  ///
  /// @param range Optional range to limit extents (defaults to full file)
  /// @return Iterable over file extents
  dwarfs::file_extents_iterable extents(std::optional<dwarfs::file_range> range) const override;

  /// Check if raw bytes access is supported
  ///
  /// Always returns true for memory-backed views (zero-copy design).
  bool supports_raw_bytes() const noexcept override { return true; }

  /// Get direct access to raw bytes (zero-copy)
  ///
  /// @return Span to the entire memory buffer
  std::span<std::byte const> raw_bytes() const override;

  /// Copy bytes from the buffer to destination
  ///
  /// @param dest Destination buffer
  /// @param range Source range (offset + size)
  /// @param ec Error code output
  void copy_bytes(void* dest, dwarfs::file_range range, std::error_code& ec) const override;

  /// Get default segment size for chunked operations
  ///
  /// @return Recommended segment size (64KB)
  std::size_t default_segment_size() const override
  {
    return 64 * 1024;  // 64KB
  }

  /// Release memory up to specified offset (no-op for memory views)
  ///
  /// @param offset Offset to release up to
  /// @param ec Error code output (always success)
  void release_until(dwarfs::file_off_t offset, std::error_code& ec) const override
  {
    (void)offset;
    ec.clear();
  }

 private:
  const void* data_;            ///< Pointer to memory buffer
  std::size_t size_;            ///< Size of memory buffer
  std::filesystem::path path_;  ///< Filesystem path for identification
};

}  // namespace tebako