/**
 *
 * Copyright (c) 2024-2025 [Ribose Inc](https://www.ribose.com).
 * All rights reserved.
 * This file is a part of the Tebako project (libtfs).
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

#include <tebako/fs/backend_factory.h>

#include <tebako/fs/backends/zip_backend.h>
#include <tebako/fs/backends/dwarfs_backend.h>
#include <tebako/fs/backends/squashfs_backend.h>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace tebako {
namespace fs {

// ===================================================================
// Magic Number Constants
// ===================================================================

namespace {
// DwarFS: "DWARFS" at offset 0 (confirmed from dwarfs source)
constexpr uint8_t DWARFS_MAGIC[] = {'D', 'W', 'A', 'R', 'F', 'S'};

// ZIP: PK signature at offset 0
constexpr uint8_t ZIP_LOCAL_MAGIC[] = {0x50, 0x4B, 0x03, 0x04};    // PK\x03\x04
constexpr uint8_t ZIP_CENTRAL_MAGIC[] = {0x50, 0x4B, 0x05, 0x06};  // PK\x05\x06

// SquashFS: "hsqs" (LE) or "sqsh" (BE) at offset 0
constexpr uint8_t SQUASHFS_MAGIC_LE[] = {0x68, 0x73, 0x71, 0x73};  // "hsqs"
constexpr uint8_t SQUASHFS_MAGIC_BE[] = {0x73, 0x71, 0x73, 0x68};  // "sqsh"

constexpr size_t MAX_MAGIC_SIZE = 16;
}  // namespace

// ===================================================================
// Primary Factory Methods
// ===================================================================

std::unique_ptr<FileSystem> BackendFactory::create_from_file(const std::string& archive_path)
{
  // Check if file exists first - don't create backends for non-existent files
  std::ifstream check_file(archive_path, std::ios::binary);
  if (!check_file.good()) {
    return nullptr;
  }
  check_file.close();

  // Try magic number detection first (most reliable)
  if (is_dwarfs_format(archive_path)) {
    return create_dwarfs();
  }

  if (is_zip_format(archive_path)) {
    return create_zip();
  }

  if (is_squashfs_format(archive_path)) {
    return create_squashfs();
  }

  // Fallback to extension-based detection
  if (has_extension(archive_path, ".dwarfs") || has_extension(archive_path, ".dfs")) {
    return create_dwarfs();
  }

  if (has_extension(archive_path, ".zip") || has_extension(archive_path, ".jar") ||
      has_extension(archive_path, ".apk") || has_extension(archive_path, ".war") ||
      has_extension(archive_path, ".ear")) {
    return create_zip();
  }

  if (has_extension(archive_path, ".sqfs") ||
      has_extension(archive_path, ".squashfs")) {
    return create_squashfs();
  }

  // Unknown format
  return nullptr;
}

std::unique_ptr<FileSystem> BackendFactory::create_from_memory(const void* data, size_t size)
{
  if (!data || size < 4) {
    return nullptr;
  }

  const uint8_t* bytes = static_cast<const uint8_t*>(data);

  // Check ZIP magic (PK\x03\x04 or PK\x05\x06)
  if (size >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && (bytes[2] == 0x03 && bytes[3] == 0x04)) {
    return create_zip();
  }
  if (size >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && (bytes[2] == 0x05 && bytes[3] == 0x06)) {
    return create_zip();
  }

  // Check SquashFS magic (hsqs or sqsh)
  if (size >= 4 &&
      ((bytes[0] == 'h' && bytes[1] == 's' &&
        bytes[2] == 'q' && bytes[3] == 's') ||
       (bytes[0] == 's' && bytes[1] == 'q' &&
        bytes[2] == 's' && bytes[3] == 'h'))) {
    return create_squashfs();
  }

  // Check DwarFS magic ("DWARFS" at offset 0)
  if (size >= sizeof(DWARFS_MAGIC) && std::memcmp(bytes, DWARFS_MAGIC, sizeof(DWARFS_MAGIC)) == 0) {
    return create_dwarfs();
  }

  return nullptr;
}

std::unique_ptr<FileSystem> BackendFactory::create_dwarfs()
{
  return std::make_unique<DwarfsBackend>();
}

std::unique_ptr<FileSystem> BackendFactory::create_zip()
{
  return std::make_unique<ZipBackend>();
}

std::unique_ptr<FileSystem> BackendFactory::create_squashfs()
{
  return std::make_unique<SquashFSBackend>();
}

// ===================================================================
// Format Detection
// ===================================================================

bool BackendFactory::is_dwarfs_format(const std::string& path)
{
  uint8_t buffer[MAX_MAGIC_SIZE] = {0};

  if (!read_magic_bytes(path, buffer, sizeof(DWARFS_MAGIC))) {
    return false;
  }

  return std::memcmp(buffer, DWARFS_MAGIC, sizeof(DWARFS_MAGIC)) == 0;
}

bool BackendFactory::is_zip_format(const std::string& path)
{
  uint8_t buffer[MAX_MAGIC_SIZE] = {0};

  if (!read_magic_bytes(path, buffer, sizeof(ZIP_LOCAL_MAGIC))) {
    return false;
  }

  // Check for local file header
  if (std::memcmp(buffer, ZIP_LOCAL_MAGIC, sizeof(ZIP_LOCAL_MAGIC)) == 0) {
    return true;
  }

  // Check for central directory (empty ZIP)
  if (std::memcmp(buffer, ZIP_CENTRAL_MAGIC, sizeof(ZIP_CENTRAL_MAGIC)) == 0) {
    return true;
  }

  return false;
}

bool BackendFactory::is_squashfs_format(const std::string& path)
{
  uint8_t buffer[MAX_MAGIC_SIZE] = {0};

  if (!read_magic_bytes(path, buffer, sizeof(SQUASHFS_MAGIC_LE))) {
    return false;
  }

  // Check for little-endian magic
  if (std::memcmp(buffer, SQUASHFS_MAGIC_LE, sizeof(SQUASHFS_MAGIC_LE)) == 0) {
    return true;
  }

  // Check for big-endian magic
  if (std::memcmp(buffer, SQUASHFS_MAGIC_BE, sizeof(SQUASHFS_MAGIC_BE)) == 0) {
    return true;
  }

  return false;
}

// ===================================================================
// Internal Helpers
// ===================================================================

bool BackendFactory::read_magic_bytes(const std::string& path, uint8_t* buffer, size_t size)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  file.read(reinterpret_cast<char*>(buffer), size);
  return file.gcount() == static_cast<std::streamsize>(size);
}

bool BackendFactory::has_extension(const std::string& path, const std::string& ext)
{
  if (path.length() < ext.length()) {
    return false;
  }

  // Case-insensitive comparison
  auto path_end = path.substr(path.length() - ext.length());
  return std::equal(path_end.begin(), path_end.end(), ext.begin(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
  });
}

}  // namespace fs
}  // namespace tebako