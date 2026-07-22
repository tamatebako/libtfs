/**
 * @file package.h
 * @brief tebako three-part package (tpkg) tooling for the tebakofs CLI
 *
 * A tebako "three-part package" is a single executable file composed of:
 *   (1) a bootstrap/runtime executable portion,
 *   (2) one or more filesystem images (slots) appended to it,
 *   (3) a tpkg manifest trailer at the very end describing the slots
 *       (see include/tebako/tpkg.h for the wire format).
 *
 * This module implements the business logic behind the tebakofs package
 * subcommands (bundle/unbundle/reassemble, insert-image/remove-image/
 * set-runtime, mkimage). The CLI layer only parses arguments.
 *
 * Copyright (c) 2026 Ribose Inc.
 * All rights reserved.
 */

#pragma once

#include <tebako/tpkg.h>

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace tebako {
namespace fs {
namespace cli {

/**
 * @brief Result of a package operation
 */
struct PackageResult {
  bool ok = false;
  std::string error;  // human-readable description when !ok
};

/**
 * @brief Manifest trailer fields controlled by the caller (bundle/reassemble)
 */
struct PackageOptions {
  std::string runtime_ref;     // empty = classic bundle (runtime embedded)
  uint32_t package_flags = 0;  // TPKG_FLAG_* bits (e.g. TPKG_FLAG_LEAN)
  uint32_t launcher_abi = 0;   // 0 = no launcher ABI spoken
};

/**
 * @brief An image to pack into a package slot
 */
struct PackageImage {
  std::string path;                       // image file on disk
  std::string mount_point;                // empty = default mount for its slot index
  uint32_t format_id = TPKG_FORMAT_AUTO;  // TPKG_FORMAT_AUTO = sniff magic
};

/**
 * @brief Default mount point for an image slot
 *
 * Slot 0 mounts at "/__tebako_memfs__" (the tebako memfs root); additional
 * slots default to "/__tebako_memfs_<N>__".
 */
std::string package_default_mount(uint32_t slot_index);

/**
 * @brief Parse a CLI image specification "<img[:mountpoint]>"
 *
 * The mountpoint must be absolute (start with '/'); a ':' not followed by
 * '/' is treated as part of the file name (e.g. Windows drive letters).
 */
PackageImage package_parse_image_spec(const std::string& spec);

/**
 * @brief Read the tpkg manifest trailer of a binary
 *
 * @param binary Path to the candidate package file
 * @param out Manifest (valid only when true is returned)
 * @param tpkg_err TPKG_ERR_* code when false is returned; TPKG_ERR_NO_TRAILER
 *        means the file simply has no trailer (not a three-part package)
 * @return true if a valid manifest was read
 */
bool package_probe(const std::string& binary, tpkg_manifest& out, int& tpkg_err);

/**
 * @brief Assemble a three-part binary: bootstrap bytes, image slots, trailer
 *
 * Layout: [bootstrap][image 0]...[image n-1][slot table][trailer header].
 * Images are written contiguously in the order given.
 */
PackageResult package_bundle(const std::string& bootstrap,
                             const std::vector<PackageImage>& images,
                             const std::string& output,
                             const PackageOptions& options);

/**
 * @brief Decompose a three-part binary into a directory
 *
 * Writes <dir>/bootstrap.bin, <dir>/image-<N>.bin per slot and
 * <dir>/manifest.json (slot table: offset, size, format, mountpoint, crc32).
 * Byte ranges between parts that are covered by no slot (padding) are dropped
 * with a warning.
 */
PackageResult package_unbundle(const std::string& binary, const std::string& output_dir);

/**
 * @brief Rebuild a binary from an unbundled directory
 *
 * Reads <dir>/manifest.json and the part files it references. Offsets and
 * sizes are recomputed from the actual parts, so swapped/edited parts are
 * picked up; with unchanged parts the result is byte-identical to the
 * original bundled binary.
 */
PackageResult package_reassemble(const std::string& input_dir, const std::string& output);

/**
 * @brief Append an image slot to a package (rewrites the file in place)
 *
 * @param mount_point empty = default mount for the new slot index
 */
PackageResult package_insert_image(const std::string& binary, const std::string& image, const std::string& mount_point);

/**
 * @brief Remove an image slot from a package (rewrites the file in place)
 *
 * The last remaining slot cannot be removed (a manifest requires >= 1 slot).
 */
PackageResult package_remove_image(const std::string& binary, uint32_t slot_index);

/**
 * @brief Replace the bootstrap (executable) portion of a package
 *
 * For a classic stitched package this swaps the embedded runtime; for a lean
 * package it swaps the bootstrap launcher. Images and trailer fields are
 * preserved. Rewrites the file in place.
 */
PackageResult package_set_runtime(const std::string& binary, const std::string& runtime_file);

/**
 * @brief Create a filesystem image from a directory (wraps mkdwarfs)
 *
 * Only "dwarfs" is supported: the zip backend is read-only. The mkdwarfs
 * binary is located via `tool` when non-empty, then the TEBAKO_MKDWARFS
 * environment variable, then PATH.
 */
PackageResult package_mkimage(const std::string& format,
                              const std::string& source_dir,
                              const std::string& output,
                              const std::string& tool);

}  // namespace cli
}  // namespace fs
}  // namespace tebako
