#pragma once

#include <dwarfs/detail/file_segment_impl.h>
#include <dwarfs/file_range.h>
#include <dwarfs/io_advice.h>
#include <dwarfs/types.h>

#include <cstddef>
#include <memory>
#include <span>
#include <system_error>

namespace tebako {

// Forward declaration
class memory_file_view_impl;

/// Memory-backed file segment implementation
///
/// Represents a contiguous region of memory within a memory-backed file view.
/// Provides zero-copy access to the underlying memory buffer.
class memory_file_segment_impl final
    : public dwarfs::detail::file_segment_impl {
 public:
  /// Construct a segment from a memory view
  ///
  /// @param view Shared pointer to the parent memory file view
  /// @param range The range (offset + size) this segment represents
  memory_file_segment_impl(
      std::shared_ptr<memory_file_view_impl const> view,
      dwarfs::file_range range);

  ~memory_file_segment_impl() override = default;

  // Disable copy and move
  memory_file_segment_impl(const memory_file_segment_impl&) = delete;
  memory_file_segment_impl& operator=(const memory_file_segment_impl&) = delete;
  memory_file_segment_impl(memory_file_segment_impl&&) = delete;
  memory_file_segment_impl& operator=(memory_file_segment_impl&&) = delete;

  // --- file_segment_impl interface ---

  /// Get the offset of this segment in the file
  dwarfs::file_off_t offset() const noexcept override {
    return range_.offset();
  }

  /// Get the size of this segment
  dwarfs::file_size_t size() const noexcept override { return range_.size(); }

  /// Get the range of this segment
  dwarfs::file_range range() const noexcept override { return range_; }

  /// Check if this segment is all zeros (always false for memory segments)
  bool is_zero() const noexcept override { return false; }

  /// Get raw bytes for this segment (zero-copy)
  std::span<std::byte const> raw_bytes() const override;

  /// Provide I/O advice (no-op for memory segments)
  void advise(dwarfs::io_advice advice, std::error_code& ec) const override {
    (void)advice;
    ec.clear();
  }

  /// Lock this segment in memory (no-op, already in memory)
  void lock(std::error_code& ec) const override { ec.clear(); }

 private:
  std::shared_ptr<memory_file_view_impl const> view_;  ///< Parent view
  dwarfs::file_range range_;  ///< Range this segment represents
};

}  // namespace tebako