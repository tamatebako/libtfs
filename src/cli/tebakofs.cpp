/**
 * @file tebakofs.cpp
 * @brief Implementation of CLI tool for Tebako filesystem library
 *
 * Copyright (c) 2021-2025 Ribose Inc.
 * All rights reserved.
 */

#include <tebako/fs/cli/tebakofs.h>
#include <tebako/fs/cli/package.h>
#include <tebako/fs/backend_factory.h>
#include <tebako/fs/directory_iterator.h>
#include <argtable3.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#include <fnmatch.h>
#endif
#include <functional>

#ifdef _WIN32
namespace {

// Minimal fnmatch(pattern, name, 0) replacement for Windows, which has no
// <fnmatch.h>. Full-string match supporting '*', '?' and [...] classes
// ('!' negation, '\' escape). Patterns are matched against a single path
// component, so FNM_PATHNAME semantics do not apply.
bool wildcard_match(const char* pattern, const char* name)
{
  while (*pattern) {
    switch (*pattern) {
      case '*': {
        while (*pattern == '*')
          ++pattern;
        if (!*pattern)
          return true;
        for (const char* rest = name;; ++rest) {
          if (wildcard_match(pattern, rest))
            return true;
          if (!*rest)
            return false;
        }
      }
      case '?':
        if (!*name)
          return false;
        ++pattern;
        ++name;
        break;
      case '[': {
        if (!*name)
          return false;
        // An unterminated '[' is matched literally (POSIX)
        {
          const char* scan = pattern + 1;
          if (*scan == '!')
            ++scan;
          if (*scan == ']')
            ++scan;
          while (*scan && *scan != ']')
            ++scan;
          if (!*scan) {
            if (*name != '[')
              return false;
            ++pattern;
            ++name;
            break;
          }
        }
        ++pattern;
        bool negate = (*pattern == '!');
        if (negate)
          ++pattern;
        bool matched = false;
        char lo = 0;
        bool have_lo = false;
        if (*pattern == ']') {  // literal ']' as first class member
          matched = (*name == ']');
          ++pattern;
        }
        while (*pattern != ']') {
          if (*pattern == '-' && have_lo && pattern[1] != ']') {
            ++pattern;
            if (*name >= lo && *name <= *pattern)
              matched = true;
            have_lo = false;
          }
          else {
            if (*name == *pattern)
              matched = true;
            lo = *pattern;
            have_lo = true;
          }
          ++pattern;
        }
        ++pattern;  // consume ']'
        if (matched == negate)
          return false;
        ++name;
        break;
      }
      case '\\':
        ++pattern;
        if (!*pattern)
          return false;
        [[fallthrough]];
      default:
        if (*pattern != *name)
          return false;
        ++pattern;
        ++name;
    }
  }
  return *name == '\0';
}

}  // namespace
#endif

// Single-component name match: fnmatch on POSIX, local matcher on Windows
static bool name_matches(const std::string& pattern, const std::string& name)
{
#ifdef _WIN32
  return wildcard_match(pattern.c_str(), name.c_str());
#else
  return fnmatch(pattern.c_str(), name.c_str(), 0) == 0;
#endif
}

namespace tebako {
namespace fs {
namespace cli {

TebakofsCLI::TebakofsCLI() {}

TebakofsCLI::~TebakofsCLI() {}

int TebakofsCLI::run(int argc, char* argv[])
{
  if (argc < 2) {
    return cmd_help("");
  }

  std::string command = argv[1];

  if (command == "help" || command == "--help" || command == "-h") {
    std::string subcommand = argc >= 3 ? argv[2] : "";
    return cmd_help(subcommand);
  }

  // Parse command-specific arguments
  if (command == "ls") {
    struct arg_lit* recursive = arg_lit0("r", "recursive", "list recursively");
    struct arg_lit* long_fmt = arg_lit0("l", "long", "use long listing format");
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_lit* quiet = arg_lit0("q", "quiet", "quiet output");
    struct arg_file* archive = arg_file1(NULL, NULL, "<archive>", "archive file");
    struct arg_str* path = arg_str0(NULL, NULL, "[path]", "path within archive (default: /)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {recursive, long_fmt, verbose, quiet, archive, path, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.recursive = recursive->count > 0;
      opts.long_format = long_fmt->count > 0;
      opts.verbose = verbose->count > 0;
      opts.quiet = quiet->count > 0;

      std::string archive_path = archive->filename[0];
      std::string list_path = path->count > 0 ? path->sval[0] : "/";

      int result = cmd_ls(archive_path, list_path, opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs ls");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "info") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* archive = arg_file1(NULL, NULL, "<archive>", "archive file");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, archive, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;
      std::string archive_path = archive->filename[0];

      int result = cmd_info(archive_path, opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs info");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "cat") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* archive = arg_file1(NULL, NULL, "<archive>", "archive file");
    struct arg_str* file = arg_str1(NULL, NULL, "<file>", "file to display");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, archive, file, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;
      std::string archive_path = archive->filename[0];
      std::string file_path = file->sval[0];

      int result = cmd_cat(archive_path, file_path, opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs cat");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "tree") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* archive = arg_file1(NULL, NULL, "<archive>", "archive file");
    struct arg_str* path = arg_str0(NULL, NULL, "[path]", "path within archive (default: /)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, archive, path, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;
      std::string archive_path = archive->filename[0];
      std::string tree_path = path->count > 0 ? path->sval[0] : "/";

      int result = cmd_tree(archive_path, tree_path, opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs tree");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "stat") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* archive = arg_file1(NULL, NULL, "<archive>", "archive file");
    struct arg_str* path = arg_str1(NULL, NULL, "<path>", "path to show metadata for");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, archive, path, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;
      std::string archive_path = archive->filename[0];
      std::string stat_path = path->sval[0];

      int result = cmd_stat(archive_path, stat_path, opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs stat");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "extract") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_lit* quiet = arg_lit0("q", "quiet", "quiet output");
    struct arg_str* dest = arg_str0("d", "dest", "<dir>", "destination directory");
    struct arg_file* archive = arg_file1(NULL, NULL, "<archive>", "archive file");
    struct arg_str* files = arg_strn(NULL, NULL, "[file]", 0, 100, "files/directories to extract");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, quiet, dest, archive, files, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;
      opts.quiet = quiet->count > 0;
      opts.dest_dir = dest->count > 0 ? dest->sval[0] : ".";

      std::string archive_path = archive->filename[0];
      std::vector<std::string> file_list;
      for (int i = 0; i < files->count; i++) {
        file_list.push_back(files->sval[i]);
      }

      int result = cmd_extract(archive_path, file_list, opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs extract");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "find") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* archive = arg_file1(NULL, NULL, "<archive>", "archive file");
    struct arg_str* pattern = arg_str1(NULL, NULL, "<pattern>", "search pattern (glob)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, archive, pattern, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;
      std::string archive_path = archive->filename[0];
      std::string search_pattern = pattern->sval[0];

      int result = cmd_find(archive_path, search_pattern, opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs find");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "bundle") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* bootstrap = arg_file1(NULL, "bootstrap", "<exe>", "bootstrap/runtime executable");
    struct arg_file* images =
        arg_filen(NULL, "image", "<img[:mountpoint]>", 1, TPKG_MAX_SLOTS, "image files (repeatable)");
    struct arg_file* output = arg_file1("o", "output", "<file>", "output binary");
    struct arg_str* runtime_ref = arg_str0(NULL, "runtime-ref", "<ref>", "runtime reference for lean packages");
    struct arg_lit* lean = arg_lit0(NULL, "lean", "mark the package as lean (bootstrap + images only)");
    struct arg_int* abi = arg_int0(NULL, "launcher-abi", "<n>", "launcher ABI version (default: 0)");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, bootstrap, images, output, runtime_ref, lean, abi, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;
      opts.runtime_ref = runtime_ref->count > 0 ? runtime_ref->sval[0] : "";
      opts.lean = lean->count > 0;
      opts.launcher_abi = abi->count > 0 ? abi->ival[0] : 0;

      std::vector<std::string> image_list;
      for (int i = 0; i < images->count; i++) {
        image_list.push_back(images->filename[i]);
      }

      int result = cmd_bundle(bootstrap->filename[0], image_list, output->filename[0], opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs bundle");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "unbundle") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* binary = arg_file1(NULL, NULL, "<binary>", "three-part package binary");
    struct arg_file* output = arg_file1("o", "output", "<dir>", "output directory");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, binary, output, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;

      int result = cmd_unbundle(binary->filename[0], output->filename[0], opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs unbundle");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "reassemble") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* dir = arg_file1(NULL, NULL, "<dir>", "unbundled package directory");
    struct arg_file* output = arg_file1("o", "output", "<file>", "output binary");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, dir, output, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;

      int result = cmd_reassemble(dir->filename[0], output->filename[0], opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs reassemble");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "insert-image") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* binary = arg_file1(NULL, NULL, "<binary>", "three-part package binary (rewritten in place)");
    struct arg_file* image = arg_file1(NULL, NULL, "<img[:mountpoint]>", "image to append");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, binary, image, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;

      int result = cmd_insert_image(binary->filename[0], image->filename[0], opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs insert-image");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "remove-image") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* binary = arg_file1(NULL, NULL, "<binary>", "three-part package binary (rewritten in place)");
    struct arg_int* slot = arg_int1(NULL, NULL, "<slot>", "slot index to remove");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, binary, slot, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;

      int result = cmd_remove_image(binary->filename[0], static_cast<uint32_t>(slot->ival[0]), opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs remove-image");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "set-runtime") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_file* binary = arg_file1(NULL, NULL, "<binary>", "three-part package binary (rewritten in place)");
    struct arg_file* runtime = arg_file1(NULL, NULL, "<runtime-file>", "new bootstrap/runtime executable");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, binary, runtime, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;

      int result = cmd_set_runtime(binary->filename[0], runtime->filename[0], opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs set-runtime");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else if (command == "mkimage") {
    struct arg_lit* verbose = arg_lit0("v", "verbose", "verbose output");
    struct arg_str* format = arg_str1(NULL, "format", "<format>", "image format (dwarfs)");
    struct arg_file* srcdir = arg_file1(NULL, NULL, "<srcdir>", "source directory");
    struct arg_file* output = arg_file1("o", "output", "<img>", "output image file");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {verbose, format, srcdir, output, end};

    int nerrors = arg_parse(argc - 1, argv + 1, argtable);

    if (nerrors == 0) {
      CLIOptions opts;
      opts.verbose = verbose->count > 0;

      int result = cmd_mkimage(format->sval[0], srcdir->filename[0], output->filename[0], opts);

      arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
      return result;
    }

    arg_print_errors(stderr, end, "tebakofs mkimage");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 1;
  }
  else {
    std::cerr << "Error: Unknown command: " << command << std::endl;
    std::cerr << "Use 'tebakofs help' for usage information" << std::endl;
    return 1;
  }
}

std::unique_ptr<FileSystem> TebakofsCLI::open_archive(const std::string& path)
{
  try {
    auto backend = BackendFactory::create_from_file(path);
    if (!backend) {
      std::cerr << "Error: Failed to open archive: " << path << std::endl;
      if (!BackendFactory::squashfs_supported() && BackendFactory::is_squashfs_format(path)) {
        // A SquashFS image on a build without the backend is a capability
        // gap, not an unknown format — say so loudly (ENOTSUP-style)
        std::cerr << "       SquashFS image detected, but this tebakofs build does NOT include SquashFS support"
                  << std::endl;
        std::cerr << "       (the SquashFS backend is POSIX-only; build with -DWITH_SQUASHFS=ON on a POSIX platform)"
                  << std::endl;
      }
      else {
        std::cerr << "       Unsupported format or file does not exist" << std::endl;
      }
      return nullptr;
    }

    auto mount_result = backend->mount(path, "/mnt");
    if (mount_result.is_err()) {
      std::cerr << "Error: Failed to mount archive: " << path << std::endl;
      std::cerr << "       " << mount_result.error().message << std::endl;
      return nullptr;
    }

    return backend;
  }
  catch (const std::exception& e) {
    std::cerr << "Error: Exception while opening archive: " << e.what() << std::endl;
    return nullptr;
  }
}

int TebakofsCLI::cmd_ls(const std::string& archive, const std::string& path, const CLIOptions& opts)
{
  auto fs = open_archive(archive);
  if (!fs)
    return 1;

  // Construct full path properly - if path is "/" use "/mnt", otherwise "/mnt" + path
  std::string full_path;
  if (path == "/" || path.empty()) {
    full_path = "/mnt";
  }
  else if (path[0] == '/') {
    full_path = "/mnt" + path;
  }
  else {
    full_path = "/mnt/" + path;
  }

  // Don't check exists for mount point itself, only for subdirectories
  if (full_path != "/mnt" && !fs->exists(full_path)) {
    std::cerr << "Error: Path does not exist: " << path << std::endl;
    return 1;
  }

  if (fs->is_file(full_path)) {
    // For single file, just show its details
    DirectoryEntry entry;
    entry.name = path.substr(path.find_last_of('/') + 1);
    if (entry.name.empty())
      entry.name = path;
    entry.is_directory = false;

    auto size_result = fs->file_size(full_path);
    entry.size = size_result.is_ok() ? size_result.unwrap() : 0;

    auto mtime_result = fs->modification_time(full_path);
    entry.mtime = mtime_result.is_ok() ? mtime_result.unwrap() : 0;

    print_entry(entry, path, opts.long_format);
    return 0;
  }

  if (opts.recursive) {
    // Recursive listing
    list_recursive(fs.get(), full_path, "", opts.long_format);
  }
  else {
    // Single directory listing
    auto iter_result = fs->list_directory(full_path);
    if (iter_result.is_err()) {
      std::cerr << "Error: Failed to list directory: " << path << std::endl;
      return 1;
    }
    auto iter = std::move(iter_result).unwrap();

    while (iter->has_next()) {
      auto entry = iter->next();
      std::string display_path = path;
      if (display_path.back() != '/')
        display_path += "/";
      display_path += entry.name;
      print_entry(entry, display_path, opts.long_format);
    }
  }

  return 0;
}

void TebakofsCLI::list_recursive(FileSystem* fs, const std::string& path, const std::string& prefix, bool long_format)
{
  auto iter_result = fs->list_directory(path);
  if (iter_result.is_err())
    return;
  auto iter = std::move(iter_result).unwrap();

  while (iter->has_next()) {
    auto entry = iter->next();
    std::string entry_path = path + "/" + entry.name;
    std::string display_path = entry_path.substr(4);  // Remove "/mnt"

    print_entry(entry, display_path, long_format);

    if (entry.is_directory) {
      list_recursive(fs, entry_path, prefix + "  ", long_format);
    }
  }
}

void TebakofsCLI::print_entry(const DirectoryEntry& entry, const std::string& path, bool long_format)
{
  if (long_format) {
    mode_t mode = entry.is_directory ? 0755 : 0644;
    std::cout << format_permissions(mode) << "  " << std::setw(10) << format_size(entry.size) << "  "
              << format_time(entry.mtime) << "  " << path << std::endl;
  }
  else {
    std::cout << path << std::endl;
  }
}

int TebakofsCLI::cmd_info(const std::string& archive, const CLIOptions& opts)
{
  // A three-part package (bootstrap + images + tpkg trailer) is detected and
  // dumped first; plain image files fall through to archive mounting below.
  tpkg_manifest manifest;
  int tpkg_err = 0;
  if (package_probe(archive, manifest, tpkg_err)) {
    std::error_code ec;
    uint64_t total_size = std::filesystem::file_size(archive, ec);

    std::cout << "Package: " << archive << std::endl;
    std::cout << "Format: tebako three-part package (tpkg v" << manifest.version << ")" << std::endl;
    if (!ec) {
      std::cout << "Total size: " << format_size(static_cast<int64_t>(total_size)) << " (" << total_size << " bytes)"
                << std::endl;
    }
    std::cout << "Flags: 0x" << std::hex << manifest.package_flags << std::dec;
    if (manifest.package_flags & TPKG_FLAG_LEAN)
      std::cout << " (LEAN)";
    std::cout << std::endl;
    std::cout << "Launcher ABI: " << manifest.launcher_abi << std::endl;
    std::cout << "Runtime ref: " << (manifest.runtime_ref[0] ? manifest.runtime_ref : "(none — classic bundle)")
              << std::endl;
    std::cout << "Bootstrap size: " << manifest.slots[0].offset << " bytes" << std::endl;
    std::cout << "Slots: " << manifest.slot_count << std::endl;
    for (uint32_t i = 0; i < manifest.slot_count; i++) {
      const tpkg_slot& s = manifest.slots[i];
      std::cout << "  [" << i << "] offset=" << s.offset << " size=" << s.size << " format=";
      switch (s.format_id) {
        case TPKG_FORMAT_DWARFS:
          std::cout << "dwarfs";
          break;
        case TPKG_FORMAT_SQUASHFS:
          std::cout << "squashfs";
          break;
        case TPKG_FORMAT_ZIP:
          std::cout << "zip";
          break;
        default:
          std::cout << "auto";
          break;
      }
      std::cout << " flags=" << s.flags << " mount=" << s.mount_point << std::endl;
    }
    std::cout << "Trailer: valid (magic and crc32 ok)" << std::endl;
    return 0;
  }
  if (tpkg_err != TPKG_ERR_NO_TRAILER) {
    std::cerr << "Error: " << archive << ": " << tpkg_strerror(tpkg_err) << std::endl;
    return 1;
  }

  auto fs = open_archive(archive);
  if (!fs)
    return 1;

  std::cout << "Archive: " << archive << std::endl;

  // Determine archive type
  std::string ext = archive.substr(archive.find_last_of('.') + 1);
  std::string type = "Unknown";
  if (ext == "zip")
    type = "ZIP";
  else if (ext == "sqfs" || ext == "squashfs")
    type = "SquashFS";
  else if (ext == "dwarfs")
    type = "DwarFS";

  std::cout << "Type: " << type << std::endl;

  // Count files and directories
  int file_count = 0;
  int dir_count = 0;
  int64_t total_size = 0;

  std::function<void(const std::string&)> count_recursive = [&](const std::string& path) {
    auto iter_result = fs->list_directory(path);
    if (iter_result.is_err())
      return;
    auto iter = std::move(iter_result).unwrap();

    while (iter->has_next()) {
      auto entry = iter->next();
      if (entry.is_directory) {
        dir_count++;
        count_recursive(path + "/" + entry.name);
      }
      else {
        file_count++;
        total_size += entry.size;
      }
    }
  };

  count_recursive("/mnt");

  std::cout << "Files: " << file_count << std::endl;
  std::cout << "Directories: " << dir_count << std::endl;
  std::cout << "Total size: " << format_size(total_size) << " (" << total_size << " bytes)" << std::endl;

  return 0;
}

int TebakofsCLI::cmd_cat(const std::string& archive, const std::string& file, const CLIOptions& opts)
{
  auto fs = open_archive(archive);
  if (!fs)
    return 1;

  std::string full_path;
  if (file == "/" || file.empty()) {
    full_path = "/mnt";
  }
  else if (file[0] == '/') {
    full_path = "/mnt" + file;
  }
  else {
    full_path = "/mnt/" + file;
  }

  if (!fs->exists(full_path)) {
    std::cerr << "Error: File does not exist: " << file << std::endl;
    return 1;
  }

  if (fs->is_directory(full_path)) {
    std::cerr << "Error: Path is a directory: " << file << std::endl;
    return 1;
  }

  auto handle_result = fs->open(full_path, O_RDONLY);
  if (handle_result.is_err()) {
    std::cerr << "Error: Failed to open file: " << file << std::endl;
    return 1;
  }
  auto handle = std::move(handle_result).unwrap();

  char buffer[4096];
  while (true) {
    ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
    if (bytes_read <= 0)
      break;
    std::cout.write(buffer, bytes_read);
  }

  return 0;
}

int TebakofsCLI::cmd_tree(const std::string& archive, const std::string& path, const CLIOptions& opts)
{
  auto fs = open_archive(archive);
  if (!fs)
    return 1;

  std::string full_path;
  if (path == "/" || path.empty()) {
    full_path = "/mnt";
  }
  else if (path[0] == '/') {
    full_path = "/mnt" + path;
  }
  else {
    full_path = "/mnt/" + path;
  }

  // Don't check exists for mount point itself
  if (full_path != "/mnt" && !fs->exists(full_path)) {
    std::cerr << "Error: Path does not exist: " << path << std::endl;
    return 1;
  }

  std::cout << path << std::endl;
  print_tree(fs.get(), full_path, 0, "");

  return 0;
}

void TebakofsCLI::print_tree(FileSystem* fs, const std::string& path, int depth, const std::string& prefix)
{
  auto iter_result = fs->list_directory(path);
  if (iter_result.is_err())
    return;
  auto iter = std::move(iter_result).unwrap();

  std::vector<DirectoryEntry> entries;
  while (iter->has_next()) {
    entries.push_back(iter->next());
  }

  for (size_t i = 0; i < entries.size(); i++) {
    const auto& entry = entries[i];
    bool is_last = (i == entries.size() - 1);

    std::cout << prefix << (is_last ? "└── " : "├── ") << entry.name;
    if (entry.is_directory)
      std::cout << "/";
    std::cout << std::endl;

    if (entry.is_directory) {
      std::string new_prefix = prefix + (is_last ? "    " : "│   ");
      print_tree(fs, path + "/" + entry.name, depth + 1, new_prefix);
    }
  }
}

int TebakofsCLI::cmd_stat(const std::string& archive, const std::string& path, const CLIOptions& opts)
{
  auto fs = open_archive(archive);
  if (!fs)
    return 1;

  std::string full_path;
  if (path == "/" || path.empty()) {
    full_path = "/mnt";
  }
  else if (path[0] == '/') {
    full_path = "/mnt" + path;
  }
  else {
    full_path = "/mnt/" + path;
  }

  // Don't check exists for mount point itself
  if (full_path != "/mnt" && !fs->exists(full_path)) {
    std::cerr << "Error: Path does not exist: " << path << std::endl;
    return 1;
  }

  std::cout << "File: " << path << std::endl;
  std::cout << "Type: " << (fs->is_directory(full_path) ? "directory" : "file") << std::endl;

  if (!fs->is_directory(full_path)) {
    auto size_result = fs->file_size(full_path);
    if (size_result.is_ok()) {
      int64_t size = size_result.unwrap();
      std::cout << "Size: " << format_size(size) << " (" << size << " bytes)" << std::endl;
    }

    auto mtime_result = fs->modification_time(full_path);
    if (mtime_result.is_ok()) {
      time_t mtime = mtime_result.unwrap();
      std::cout << "Modified: " << format_time(mtime) << std::endl;
    }
  }

  return 0;
}

int TebakofsCLI::cmd_extract(const std::string& archive, const std::vector<std::string>& files, const CLIOptions& opts)
{
  auto fs = open_archive(archive);
  if (!fs)
    return 1;

  std::string dest = opts.dest_dir;

  bool success;
  if (files.empty()) {
    // Extract entire archive
    if (opts.verbose) {
      std::cout << "Extracting entire archive to: " << dest << std::endl;
    }
    success = extract_all(fs.get(), dest);
  }
  else {
    // Extract specific files
    if (opts.verbose) {
      std::cout << "Extracting " << files.size() << " item(s) to: " << dest << std::endl;
    }
    success = extract_selected(fs.get(), files, dest);
  }

  if (success && !opts.quiet) {
    std::cout << "Extraction complete" << std::endl;
  }

  return success ? 0 : 1;
}

bool TebakofsCLI::extract_selected(FileSystem* fs, const std::vector<std::string>& files, const std::string& dest_base)
{
  bool all_success = true;

  for (const auto& file : files) {
    std::string full_path;
    if (file == "/" || file.empty()) {
      full_path = "/mnt";
    }
    else if (file[0] == '/') {
      full_path = "/mnt" + file;
    }
    else {
      full_path = "/mnt/" + file;
    }

    if (!fs->exists(full_path)) {
      std::cerr << "Warning: Path does not exist: " << file << std::endl;
      all_success = false;
      continue;
    }

    if (fs->is_directory(full_path)) {
      if (options_.verbose) {
        std::cout << "Extracting directory: " << file << std::endl;
      }
      if (!extract_directory(fs, full_path, dest_base + "/" + file)) {
        all_success = false;
      }
    }
    else {
      if (options_.verbose) {
        std::cout << "Extracting file: " << file << std::endl;
      }
      if (!extract_file(fs, full_path, dest_base + "/" + file)) {
        all_success = false;
      }
    }
  }

  return all_success;
}

bool TebakofsCLI::extract_all(FileSystem* fs, const std::string& dest)
{
  return extract_directory(fs, "/mnt", dest);
}

bool TebakofsCLI::extract_file(FileSystem* fs, const std::string& src, const std::string& dest)
{
  // Create parent directories
  size_t last_slash = dest.find_last_of('/');
  if (last_slash != std::string::npos) {
    std::string parent = dest.substr(0, last_slash);
    std::string cmd = "mkdir -p \"" + parent + "\"";
    system(cmd.c_str());
  }

  auto handle_result = fs->open(src, O_RDONLY);
  if (handle_result.is_err()) {
    std::cerr << "Error: Failed to open file: " << src << std::endl;
    return false;
  }
  auto handle = std::move(handle_result).unwrap();

  std::ofstream out(dest, std::ios::binary);
  if (!out) {
    std::cerr << "Error: Failed to create output file: " << dest << std::endl;
    return false;
  }

  char buffer[8192];
  while (true) {
    ssize_t bytes_read = handle->read(buffer, sizeof(buffer));
    if (bytes_read <= 0)
      break;
    out.write(buffer, bytes_read);
  }

  return true;
}

bool TebakofsCLI::extract_directory(FileSystem* fs, const std::string& src, const std::string& dest)
{
  // Create destination directory
  std::string cmd = "mkdir -p \"" + dest + "\"";
  system(cmd.c_str());

  auto iter_result = fs->list_directory(src);
  if (iter_result.is_err()) {
    std::cerr << "Error: Failed to list directory: " << src << std::endl;
    return false;
  }
  auto iter = std::move(iter_result).unwrap();

  bool all_success = true;
  while (iter->has_next()) {
    auto entry = iter->next();
    std::string src_path = src + "/" + entry.name;
    std::string dest_path = dest + "/" + entry.name;

    if (entry.is_directory) {
      if (!extract_directory(fs, src_path, dest_path)) {
        all_success = false;
      }
    }
    else {
      if (!extract_file(fs, src_path, dest_path)) {
        all_success = false;
      }
    }
  }

  return all_success;
}

int TebakofsCLI::cmd_find(const std::string& archive, const std::string& pattern, const CLIOptions& opts)
{
  auto fs = open_archive(archive);
  if (!fs)
    return 1;

  std::function<void(const std::string&)> search_recursive = [&](const std::string& path) {
    auto iter_result = fs->list_directory(path);
    if (iter_result.is_err())
      return;
    auto iter = std::move(iter_result).unwrap();

    while (iter->has_next()) {
      auto entry = iter->next();
      std::string entry_path = path + "/" + entry.name;
      std::string display_path = entry_path.substr(4);  // Remove "/mnt"

      // Match against pattern
      if (name_matches(pattern, entry.name)) {
        std::cout << display_path << std::endl;
      }

      if (entry.is_directory) {
        search_recursive(entry_path);
      }
    }
  };

  search_recursive("/mnt");

  return 0;
}

int TebakofsCLI::cmd_bundle(const std::string& bootstrap,
                            const std::vector<std::string>& images,
                            const std::string& output,
                            const CLIOptions& opts)
{
  std::vector<PackageImage> image_list;
  for (const auto& spec : images) {
    image_list.push_back(package_parse_image_spec(spec));
  }

  PackageOptions pkg_opts;
  pkg_opts.runtime_ref = opts.runtime_ref;
  pkg_opts.package_flags = opts.lean ? TPKG_FLAG_LEAN : 0;
  pkg_opts.launcher_abi = opts.launcher_abi < 0 ? 0 : static_cast<uint32_t>(opts.launcher_abi);

  auto res = package_bundle(bootstrap, image_list, output, pkg_opts);
  if (!res.ok) {
    std::cerr << "Error: bundle failed: " << res.error << std::endl;
    return 1;
  }
  if (opts.verbose) {
    std::cout << "Wrote package: " << output << " (" << image_list.size() << " image slot(s))" << std::endl;
  }
  return 0;
}

int TebakofsCLI::cmd_unbundle(const std::string& binary, const std::string& output_dir, const CLIOptions& opts)
{
  auto res = package_unbundle(binary, output_dir);
  if (!res.ok) {
    std::cerr << "Error: unbundle failed: " << res.error << std::endl;
    return 1;
  }
  if (opts.verbose) {
    std::cout << "Unbundled " << binary << " into: " << output_dir << std::endl;
  }
  return 0;
}

int TebakofsCLI::cmd_reassemble(const std::string& input_dir, const std::string& output, const CLIOptions& opts)
{
  auto res = package_reassemble(input_dir, output);
  if (!res.ok) {
    std::cerr << "Error: reassemble failed: " << res.error << std::endl;
    return 1;
  }
  if (opts.verbose) {
    std::cout << "Reassembled " << input_dir << " into: " << output << std::endl;
  }
  return 0;
}

int TebakofsCLI::cmd_insert_image(const std::string& binary, const std::string& image_spec, const CLIOptions& opts)
{
  auto image = package_parse_image_spec(image_spec);
  auto res = package_insert_image(binary, image.path, image.mount_point);
  if (!res.ok) {
    std::cerr << "Error: insert-image failed: " << res.error << std::endl;
    return 1;
  }
  if (opts.verbose) {
    std::cout << "Inserted " << image.path << " into: " << binary << std::endl;
  }
  return 0;
}

int TebakofsCLI::cmd_remove_image(const std::string& binary, uint32_t slot_index, const CLIOptions& opts)
{
  auto res = package_remove_image(binary, slot_index);
  if (!res.ok) {
    std::cerr << "Error: remove-image failed: " << res.error << std::endl;
    return 1;
  }
  if (opts.verbose) {
    std::cout << "Removed slot " << slot_index << " from: " << binary << std::endl;
  }
  return 0;
}

int TebakofsCLI::cmd_set_runtime(const std::string& binary, const std::string& runtime_file, const CLIOptions& opts)
{
  auto res = package_set_runtime(binary, runtime_file);
  if (!res.ok) {
    std::cerr << "Error: set-runtime failed: " << res.error << std::endl;
    return 1;
  }
  if (opts.verbose) {
    std::cout << "Replaced the bootstrap portion of: " << binary << std::endl;
  }
  return 0;
}

int TebakofsCLI::cmd_mkimage(const std::string& format,
                             const std::string& source_dir,
                             const std::string& output,
                             const CLIOptions& opts)
{
  auto res = package_mkimage(format, source_dir, output, "");
  if (!res.ok) {
    std::cerr << "Error: mkimage failed: " << res.error << std::endl;
    return 1;
  }
  if (opts.verbose) {
    std::cout << "Wrote " << format << " image: " << output << std::endl;
  }
  return 0;
}

int TebakofsCLI::cmd_help(const std::string& command)
{
  if (command.empty()) {
    std::cout << "tebakofs - Tebako filesystem CLI tool\n\n";
    std::cout << "Usage: tebakofs <command> [options] <archive> [args...]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  ls       List directory contents\n";
    std::cout << "  info     Show archive information (or dump a three-part package trailer)\n";
    std::cout << "  cat      Display file contents\n";
    std::cout << "  tree     Show directory tree\n";
    std::cout << "  stat     Show file/directory metadata\n";
    std::cout << "  extract  Extract archive contents\n";
    std::cout << "  find     Search for files\n";
    std::cout << "  bundle        Assemble a three-part package (bootstrap + images + trailer)\n";
    std::cout << "  unbundle      Decompose a three-part package into a directory\n";
    std::cout << "  reassemble    Rebuild a binary from an unbundled directory\n";
    std::cout << "  insert-image  Append an image slot to a package (in place)\n";
    std::cout << "  remove-image  Remove an image slot from a package (in place)\n";
    std::cout << "  set-runtime   Replace the bootstrap portion of a package (in place)\n";
    std::cout << "  mkimage       Create a dwarfs image from a directory (wraps mkdwarfs)\n";
    std::cout << "  help     Show help for a command\n\n";
    std::cout << "Use 'tebakofs help <command>' for more information about a command.\n";
  }
  else if (command == "ls") {
    std::cout << "Usage: tebakofs ls [options] <archive> [path]\n\n";
    std::cout << "List directory contents.\n\n";
    std::cout << "Options:\n";
    std::cout << "  -r, --recursive  List recursively\n";
    std::cout << "  -l, --long       Use long listing format\n";
    std::cout << "  -v, --verbose    Verbose output\n";
    std::cout << "  -q, --quiet      Quiet output\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs ls archive.zip\n";
    std::cout << "  tebakofs ls -rl archive.zip /subdir\n";
  }
  else if (command == "extract") {
    std::cout << "Usage: tebakofs extract [options] <archive> [files...]\n\n";
    std::cout << "Extract archive contents.\n\n";
    std::cout << "Options:\n";
    std::cout << "  -d, --dest <dir>  Destination directory (default: .)\n";
    std::cout << "  -v, --verbose     Verbose output\n";
    std::cout << "  -q, --quiet       Quiet output\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs extract archive.zip\n";
    std::cout << "  tebakofs extract archive.zip file1.txt file2.txt\n";
    std::cout << "  tebakofs extract -d /tmp/out archive.sqfs\n";
  }
  else if (command == "info") {
    std::cout << "Usage: tebakofs info [options] <archive>\n\n";
    std::cout << "Show archive information. When the file carries a tpkg manifest trailer\n";
    std::cout << "(a tebako three-part package), the trailer is dumped instead: slot count,\n";
    std::cout << "offsets, sizes, mountpoints and trailer validity.\n\n";
    std::cout << "Options:\n";
    std::cout << "  -v, --verbose  Verbose output\n";
  }
  else if (command == "bundle") {
    std::cout << "Usage: tebakofs bundle --bootstrap <exe> --image <img[:mountpoint]>... --output <file>\n\n";
    std::cout << "Assemble a tebako three-part package: copy the bootstrap bytes, append each\n";
    std::cout << "image as a slot, and write the tpkg manifest trailer. The default mountpoint\n";
    std::cout << "for image slot 0 is /__tebako_memfs__ (slot N: /__tebako_memfs_N__); image\n";
    std::cout << "formats are sniffed from magic bytes (dwarfs/squashfs/zip, else auto).\n\n";
    std::cout << "Options:\n";
    std::cout << "  --bootstrap <exe>        Bootstrap/runtime executable (required)\n";
    std::cout << "  --image <img[:mount]>    Image file, repeatable (1.." << TPKG_MAX_SLOTS << ")\n";
    std::cout << "  -o, --output <file>      Output binary (required)\n";
    std::cout << "  --runtime-ref <ref>      Runtime reference recorded in the trailer\n";
    std::cout << "  --lean                   Mark the package as lean (TPKG_FLAG_LEAN)\n";
    std::cout << "  --launcher-abi <n>       Launcher ABI version (default: 0)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs bundle --bootstrap runtime --image app.dwarfs -o myapp\n";
    std::cout << "  tebakofs bundle --bootstrap boot --image app.dwarfs:/app --image data.dwarfs:/data -o myapp\n";
  }
  else if (command == "unbundle") {
    std::cout << "Usage: tebakofs unbundle <binary> --output <dir>\n\n";
    std::cout << "Decompose a three-part package: writes bootstrap.bin, image-<N>.bin per slot\n";
    std::cout << "and manifest.json (slot table: offset, size, format, mountpoint, crc32).\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <dir>  Output directory (required)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs unbundle myapp -o parts/\n";
  }
  else if (command == "reassemble") {
    std::cout << "Usage: tebakofs reassemble <dir> --output <file>\n\n";
    std::cout << "Rebuild a binary from an unbundled directory. With unchanged parts the result\n";
    std::cout << "is byte-identical to the original; swapped or edited parts (and manifest.json\n";
    std::cout << "metadata edits) are picked up and offsets/sizes recomputed.\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output <file>  Output binary (required)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs reassemble parts/ -o myapp.patched\n";
  }
  else if (command == "insert-image") {
    std::cout << "Usage: tebakofs insert-image <binary> <img[:mountpoint]>\n\n";
    std::cout << "Append an image slot to a three-part package (rewrites the file in place).\n";
    std::cout << "Default mountpoint: /__tebako_memfs_<N>__ for the new slot index N.\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs insert-image myapp extra.dwarfs:/extra\n";
  }
  else if (command == "remove-image") {
    std::cout << "Usage: tebakofs remove-image <binary> <slot>\n\n";
    std::cout << "Remove an image slot from a three-part package (rewrites the file in place).\n";
    std::cout << "The last remaining slot cannot be removed (a manifest requires >= 1 slot).\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs remove-image myapp 1\n";
  }
  else if (command == "set-runtime") {
    std::cout << "Usage: tebakofs set-runtime <binary> <runtime-file>\n\n";
    std::cout << "Replace the bootstrap (executable) portion of a three-part package: for a\n";
    std::cout << "classic stitched package this swaps the embedded runtime, for a lean package\n";
    std::cout << "the bootstrap launcher. Images and trailer fields are preserved.\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs set-runtime myapp tebako-runtime-3.3.7-macos-arm64\n";
  }
  else if (command == "mkimage") {
    std::cout << "Usage: tebakofs mkimage --format dwarfs <srcdir> --output <img>\n\n";
    std::cout << "Create a filesystem image from a directory. Thin wrapper around mkdwarfs,\n";
    std::cout << "located via TEBAKO_MKDWARFS or PATH. Only the dwarfs format is supported:\n";
    std::cout << "the zip backend is read-only.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --format <format>   Image format: dwarfs (required)\n";
    std::cout << "  -o, --output <img>  Output image file (required)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  tebakofs mkimage --format dwarfs app/ -o app.dwarfs\n";
  }
  else {
    std::cout << "Use 'tebakofs help' for general help.\n";
  }

  return 0;
}

std::string TebakofsCLI::format_size(int64_t size)
{
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size_d = static_cast<double>(size);

  while (size_d >= 1024.0 && unit_index < 4) {
    size_d /= 1024.0;
    unit_index++;
  }

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << size_d << " " << units[unit_index];
  return oss.str();
}

std::string TebakofsCLI::format_permissions(mode_t mode)
{
  std::string perms;
  perms += (mode & S_IFDIR) ? 'd' : '-';
  perms += (mode & S_IRUSR) ? 'r' : '-';
  perms += (mode & S_IWUSR) ? 'w' : '-';
  perms += (mode & S_IXUSR) ? 'x' : '-';
  perms += (mode & S_IRGRP) ? 'r' : '-';
  perms += (mode & S_IWGRP) ? 'w' : '-';
  perms += (mode & S_IXGRP) ? 'x' : '-';
  perms += (mode & S_IROTH) ? 'r' : '-';
  perms += (mode & S_IWOTH) ? 'w' : '-';
  perms += (mode & S_IXOTH) ? 'x' : '-';
  return perms;
}

std::string TebakofsCLI::format_time(time_t mtime)
{
  char buffer[32];
  struct tm* tm_info = localtime(&mtime);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
  return std::string(buffer);
}

}  // namespace cli
}  // namespace fs
}  // namespace tebako