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

#pragma once

#include <zip.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace tebako_test {

/**
 * @brief Create a ZIP archive from a directory tree, in-process
 *
 * Replaces the non-portable `system("cd <dir> && zip -r ...")` harness call
 * (no zip executable and no /dev/null on Windows). Entry names are stored
 * under `zip_root_name/...` exactly like `zip -r out.zip <root>/` does.
 */
inline bool create_zip_from_dir(const std::string& zip_path,
                                const std::filesystem::path& content_root,
                                const std::string& zip_root_name)
{
  namespace fs = std::filesystem;

  int err = 0;
  zip_t* archive = zip_open(zip_path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
  if (!archive) {
    return false;
  }

  bool ok = true;

  // Recursive walk; explicit directory entries like Info-ZIP creates.
  // An empty zip_root_name stores entries at the archive root.
  auto add_tree = [&](auto&& self, const fs::path& dir, const std::string& prefix) -> bool {
    for (const auto& entry : fs::directory_iterator(dir)) {
      std::string name = prefix.empty() ? entry.path().filename().generic_string()
                                        : prefix + "/" + entry.path().filename().generic_string();
      if (entry.is_directory()) {
        if (zip_dir_add(archive, (name + "/").c_str(), ZIP_FL_ENC_UTF_8) < 0) {
          return false;
        }
        if (!self(self, entry.path(), name)) {
          return false;
        }
      }
      else if (entry.is_regular_file()) {
        std::ifstream ifs(entry.path(), std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // Hand ownership of the buffer to libzip (freep = 1)
        size_t size = data.size();
        void* buffer = std::malloc(std::max<size_t>(size, 1));
        if (!buffer) {
          return false;
        }
        std::memcpy(buffer, data.data(), size);

        zip_source_t* source = zip_source_buffer(archive, buffer, size, 1);
        if (!source) {
          std::free(buffer);
          return false;
        }
        if (zip_file_add(archive, name.c_str(), source, ZIP_FL_ENC_UTF_8) < 0) {
          zip_source_free(source);
          return false;
        }
      }
    }
    return true;
  };

  if ((!zip_root_name.empty() && zip_dir_add(archive, (zip_root_name + "/").c_str(), ZIP_FL_ENC_UTF_8) < 0) ||
      !add_tree(add_tree, content_root, zip_root_name)) {
    ok = false;
  }

  if (zip_close(archive) < 0) {
    ok = false;
  }

  return ok && fs::exists(zip_path);
}

}  // namespace tebako_test
