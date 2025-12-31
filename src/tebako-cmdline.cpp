/**
 *
 * Copyright (c) 2024 [Ribose Inc](https://www.ribose.com).
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
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <tebako-pch.h>
#include <tebako-pch-pp.h>
#include <tebako/fs/common.h>
#include <tebako-io-inner.h>
#include <tebako-io-root.h>
#include <tebako/fs/memfs.h>
#include <tebako/fs/internal/memfs_table.h>
#include <tebako/fs/internal/mount_table.h>

#include <tebako-cmdline.h>

namespace tebako {

void cmdline_args::build_arguments(const char* fs_mount_point, const char* fs_entry_point)
{
  if (fs_mount_point == nullptr || fs_entry_point == nullptr || fs_mount_point[0] == 0 || fs_entry_point[0] == 0) {
    throw std::invalid_argument("Internal error: fs_mount_point and fs_entry_point must be non-null and non-empty");
  }

  if (extract) {
    build_arguments_for_extract(fs_mount_point);
  }
  else {
    build_arguments_for_run(fs_mount_point, fs_entry_point);
  }
}

// build_arguments_for_extract
//  Builds command line arguments to process --tebako-extract
//  ruby -e "require 'fileutils'; FileUtils.copy_entry '<tebako::fs_mount_point>',argv[2] || 'source_filesystem'"
void cmdline_args::build_arguments_for_extract(const char* fs_mount_point)
{
  std::string cmd =
      std::string("require 'fileutils'; FileUtils.copy_entry '") + fs_mount_point + "', '" + extract_folder + "'";
  printf("Extracting tebako image to '%s' \n", extract_folder.c_str());
  size_t new_argv_size = 3 + cmd.size() + 1 + strlen(argv[0]) + 1;
  new_argv = new char*[3];
  char* argv_memory = new_argv_memory = new char[new_argv_size];
  if (new_argv != nullptr && argv_memory != nullptr) {
    strcpy(argv_memory, argv[0]);
    new_argv[0] = argv_memory;
    argv_memory += (strlen(argv[0]) + 1);
    strcpy(argv_memory, "-e");
    new_argv[1] = argv_memory;
    argv_memory += 3;
    strcpy(argv_memory, cmd.c_str());
    new_argv[2] = argv_memory;
    new_argc = 3;
  }
  else {
    throw std::bad_alloc();
  }
}

void cmdline_args::build_arguments_for_run(const char* fs_mount_point, const char* fs_entry_point)
{
  size_t new_argv_size = strlen(fs_mount_point) + strlen(fs_entry_point) + 1;
  for (const auto& arg : other_args) {
    new_argv_size += (arg.length() + 1);
  }

  char* argv_memory = new_argv_memory = new char[new_argv_size];
  if (!argv_memory) {
    throw std::bad_alloc();
  }

  // Build the new argv array
  new_argv = new char*[other_args.size() + 1];
  memcpy(argv_memory, other_args[0].c_str(), other_args[0].length() + 1);
  new_argv[0] = argv_memory;
  argv_memory += (other_args[0].length() + 1);

  // Add tebako fs_mount_point and fs_entry_point
  memcpy(argv_memory, fs_mount_point, strlen(fs_mount_point));
  new_argv[1] = argv_memory;
  argv_memory += strlen(fs_mount_point);

  memcpy(argv_memory, fs_entry_point, strlen(fs_entry_point) + 1);
  argv_memory += (strlen(fs_entry_point) + 1);

  // Copy remaining arguments
  for (size_t i = 1; i < other_args.size(); i++) {
    memcpy(argv_memory, other_args[i].c_str(), other_args[i].length() + 1);
    new_argv[i + 1] = argv_memory;
    argv_memory += (other_args[i].length() + 1);
  }

  new_argc = other_args.size() + 1;
}

void cmdline_args::parse_arguments(void)
{
  const std::string error_msg =
      "Error: --tebako-mount shall be followed by a rule (e.g., --tebako-mount <mount point>:<target>)";

  const std::string error_msg_run =
      "Error: --tebako-run shall be followed by the application image file name (e.g., --tebako-run=<image file name>)";

  const std::string error_msg_run_nodup = "Error: --tebako-run option can be provided only once";

  const std::string run_key = "--tebako-run";
  const std::string run_key_ex = run_key + "=";
  const std::string mount_key = "--tebako-mount";
  const std::string mount_key_ex = mount_key + "=";
  const std::string extract_key = "--tebako-extract";
  const std::string extract_key_ex = extract_key + "=";
  const std::string extract_dest = "source_filesystem";

  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];

    // Handle "--tebako-run=value" case
    if (arg.rfind(run_key_ex, 0) == 0) {
      std::string value = arg.substr(13);

      if (!value.empty()) {
        if (run) {
          throw std::invalid_argument(error_msg_run_nodup);
        }
        run = true;
        app_image = std::move(value);
        continue;
      }
      else {
        throw std::invalid_argument(error_msg_run);
      }
    }

    // Handle "--tebako-mount=value" case
    if (arg.rfind(mount_key_ex, 0) == 0) {
      std::string value = arg.substr(15);

      if (!value.empty()) {
        mountpoints.push_back(std::move(value));
        continue;
      }
      else {
        throw std::invalid_argument(error_msg);
      }
    }

    // Handle "--tebako-extract=value" case
    if (arg.rfind(extract_key_ex, 0) == 0) {
      extract = true;
      std::string value = arg.substr(17);

      if (!value.empty()) {
        extract_folder = std::move(value);
      }
      else {
        extract_folder = std::move(extract_dest);
      }
      return;
    }

    // Handle "--tebako-run" without '='
    if (arg == run_key) {
      if (run) {
        throw std::invalid_argument(error_msg_run_nodup);
      }
      run = true;
      // Ensure there is a next argument
      if (i + 1 < argc) {
        std::string next_arg = argv[i + 1];

        // Check if the next argument is valid
        if (next_arg[0] != '-') {  // It's not a flag
          app_image = std::move(next_arg);
          i += 1;  // Skip the next argument as it is the rule
          continue;
        }
        else {
          throw std::invalid_argument(error_msg_run);
        }
      }
      // If "--tebako-mount" is at the end of args without a rule, raise an error
      throw std::invalid_argument(error_msg_run);
    }

    // Handle "--tebako-mount" without '='
    if (arg == mount_key) {
      // Ensure there is a next argument
      if (i + 1 < argc) {
        std::string next_arg = argv[i + 1];

        // Check if the next argument is valid
        if (next_arg[0] != '-') {  // It's not a flag
          mountpoints.push_back(std::move(next_arg));
          i += 1;  // Skip the next argument as it is the rule
          continue;
        }
        else {
          throw std::invalid_argument(error_msg);
        }
      }
      // If "--tebako-mount" is at the end of args without a rule, raise an error
      throw std::invalid_argument(error_msg);
    }

    // Handle "--tebako-extract" without '='
    if (arg.rfind(extract_key, 0) == 0) {
      extract = true;
      if (i + 1 < argc) {
        std::string next_arg = argv[i + 1];

        // Check if the next argument is valid
        if (next_arg[0] != '-') {  // It's not a flag
          extract_folder = std::move(next_arg);
          i += 1;
        }
        else {
          extract_folder = std::move(extract_dest);
        }
      }
      else {
        extract_folder = std::move(extract_dest);
      }
      return;
    }

    // Add other arguments as they are
    other_args.push_back(std::move(arg));
  }
}

void cmdline_args::process_mountpoints()
{
  for (const auto& item : mountpoints) {
    // Split item by the first ':' or '>'
    size_t separator_pos = item.find_first_of(":>");
    if (separator_pos != std::string::npos) {
      char separator = item[separator_pos];
      // Extract the first and second parts
      std::string mountpoint = item.substr(0, separator_pos);
      std::string target = item.substr(separator_pos + 1);

      // Split file_path into path and filename
      stdfs::path p(mountpoint);

      if (p.is_absolute()) {
        throw std::invalid_argument("Path " + mountpoint + " is not within tebako memfs");
      }

      std::string path = p.parent_path().string();
      std::string filename = p.filename().string();

      // Check that both filename and target are not empty
      if (filename.empty() || target.empty()) {
        throw std::invalid_argument("Invalid input: path or filename or target is empty in " + item);
      }

      struct stat st;
      std::string lnk;
      uint32_t root = sync_tebako_memfs_table::get_tebako_memfs_table().get(0)->get_root_inode();
      int res = dwarfs_inode_relative_stat(root, path, &st, lnk, false);
      if (res == DWARFS_IO_ERROR) {
        throw std::invalid_argument("Path " + path + " does not exist or is not accessible");
      }
      if (res == DWARFS_S_LINK_OUTSIDE) {
        throw std::invalid_argument("Path " + path + " is not within tebako memfs");
      }

      if (separator == ':') {
        sync_tebako_mount_table::get_tebako_mount_table().insert(st.st_ino, filename, target);
      }
      else {  // assume that  (separator == '>')
        std::ifstream file(target, std::ios::binary | std::ios::ate);
        if (!file) {
          throw std::invalid_argument("Path " + target + " does not exist");
        }
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer;
        buffer.resize(size);

        if (!file.read(buffer.data(), size)) {
          throw std::invalid_argument("Failed to load filesystem image from " + target);
        }

        int ret = mount_memfs(buffer.data(), size, "auto", st.st_ino, filename.c_str());
        if (ret < 1) {
          throw std::invalid_argument("Failed to mount filesystem image from " + target);
        }
      }
    }
    else {
      throw std::invalid_argument("Invalid input: missing ':' or '>' separator in " + item);
    }
  }
}

void cmdline_args::process_package()
{
  std::ifstream file(app_image, std::ios::binary | std::ios::ate);
  if (!file) {
    throw std::invalid_argument("Path " + app_image + " does not exist");
  }
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);

  package.resize(size);

  if (!file.read(package.data(), size)) {
    throw std::invalid_argument("Failed to load filesystem image from " + app_image);
  }

  descriptor = package_descriptor(package);
}

}  // namespace tebako
