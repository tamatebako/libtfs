/**
 *
 * Copyright (c) 2021-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <tebako/fs/internal/memory_file_view.h>
#include <tebako/fs/internal/memory_file_segment.h>

#include <dwarfs/file_extent.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace tebako {

memory_file_view_impl::memory_file_view_impl(const void* data,
                                             std::size_t size,
                                             std::filesystem::path path)
    : data_(data), size_(size), path_(std::move(path)) {
  if (!data || size == 0) {
    throw std::invalid_argument(
        "memory_file_view_impl: data must not be null and size must be "
        "non-zero");
  }
}

dwarfs::file_segment
memory_file_view_impl::segment_at(dwarfs::file_range range) const {
  auto const offset = range.offset();
  auto const size = range.size();

  // Validate range
  if (offset < 0 || size == 0 ||
      std::cmp_greater(offset + size, this->size())) {
    return {}; // Return empty segment for invalid ranges
  }

  // Create a segment implementation
  return dwarfs::file_segment(std::make_shared<memory_file_segment_impl>(
      shared_from_this(), range));
}

dwarfs::file_extents_iterable
memory_file_view_impl::extents(std::optional<dwarfs::file_range> range) const {
  // For a simple memory buffer, we have a single data extent
  std::vector<dwarfs::detail::file_extent_info> extents;

  if (!range.has_value()) {
    range.emplace(0, size_);
  }

  // Single extent covering the entire buffer
  extents.emplace_back(dwarfs::extent_kind::data, *range);

  return dwarfs::file_extents_iterable{shared_from_this(), extents, *range};
}

std::span<std::byte const> memory_file_view_impl::raw_bytes() const {
  return {reinterpret_cast<std::byte const*>(data_), size_};
}

void memory_file_view_impl::copy_bytes(void* dest, dwarfs::file_range range,
                                      std::error_code& ec) const {
  auto const offset = range.offset();
  auto const size = range.size();

  // Validate parameters
  if (size == 0) {
    ec.clear();
    return;
  }

  if (dest == nullptr || offset < 0) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return;
  }

  if (std::cmp_greater(offset + size, this->size())) {
    ec = std::make_error_code(std::errc::result_out_of_range);
    return;
  }

  // Perform zero-copy data access
  auto src = reinterpret_cast<const std::byte*>(data_) + offset;
  std::memcpy(dest, src, size);
  ec.clear();
}

}  // namespace tebako