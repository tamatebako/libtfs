/**
 * @file package.cpp
 * @brief Implementation of tebako three-part package (tpkg) tooling
 *
 * Copyright (c) 2026 Ribose Inc.
 * All rights reserved.
 */

// The tpkg.h implementation section lives in this translation unit. Per the
// tpkg.h usage notes it must be included before any system header in this TU
// (it defines _POSIX_C_SOURCE itself when needed).
#define TPKG_IMPLEMENTATION
#include <tebako/fs/cli/package.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace tebako {
namespace fs {
namespace cli {

namespace {

namespace fsys = std::filesystem;

constexpr size_t kCopyBufferSize = 1u << 20;  // 1 MiB

PackageResult fail(std::string msg)
{
  return PackageResult{false, std::move(msg)};
}

PackageResult success()
{
  return PackageResult{true, {}};
}

// ---- fd wrappers -----------------------------------------------------------

int open_fd(const fsys::path& p, bool writable)
{
#ifdef _WIN32
  return ::_open(p.string().c_str(), (writable ? _O_RDWR : _O_RDONLY) | _O_BINARY);
#else
  return ::open(p.string().c_str(), writable ? O_RDWR : O_RDONLY);
#endif
}

void close_fd(int fd)
{
#ifdef _WIN32
  ::_close(fd);
#else
  ::close(fd);
#endif
}

int current_pid()
{
#ifdef _WIN32
  return ::_getpid();
#else
  return ::getpid();
#endif
}

// ---- streaming CRC-32 (zlib polynomial, identical to tpkg_crc32) -----------

const uint32_t* crc32_table()
{
  static const uint32_t* table = [] {
    auto* t = new uint32_t[256];
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++) {
        c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
      }
      t[i] = c;
    }
    return t;
  }();
  return table;
}

uint32_t crc32_update(uint32_t state, const void* data, size_t n)
{
  const uint32_t* table = crc32_table();
  const auto* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < n; i++) {
    state = table[(state ^ p[i]) & 0xFFu] ^ (state >> 8);
  }
  return state;
}

// ---- part streaming ----------------------------------------------------------

// A byte range of a file to copy into a package (length UINT64_MAX = to EOF).
struct PartSource {
  fsys::path path;
  uint64_t offset = 0;
  uint64_t length = UINT64_MAX;
};

struct SlotSource {
  PartSource source;
  uint32_t format_id = TPKG_FORMAT_AUTO;
  uint32_t flags = 0;
  std::string mount_point;
};

// Streams `source` into `out`; reports bytes written and (when crc_out is
// non-null) the part's CRC-32. Returns false with `err` set on failure.
bool stream_part(const PartSource& source, std::ofstream& out, uint64_t& written, uint32_t* crc_out, std::string& err)
{
  std::ifstream in(source.path, std::ios::binary);
  if (!in) {
    err = "cannot open part file: " + source.path.string();
    return false;
  }
  in.seekg(0, std::ios::end);
  auto file_size = static_cast<uint64_t>(in.tellg());
  if (source.offset > file_size) {
    err = "part offset " + std::to_string(source.offset) + " is beyond the end of file: " + source.path.string();
    return false;
  }
  uint64_t available = file_size - source.offset;
  uint64_t n = (source.length == UINT64_MAX) ? available : std::min(source.length, available);
  in.seekg(static_cast<std::streamoff>(source.offset));

  std::vector<char> buf(kCopyBufferSize);
  uint32_t crc_state = 0xFFFFFFFFu;
  uint64_t remaining = n;
  while (remaining > 0) {
    auto chunk = static_cast<size_t>(std::min<uint64_t>(remaining, buf.size()));
    in.read(buf.data(), static_cast<std::streamsize>(chunk));
    if (in.gcount() != static_cast<std::streamsize>(chunk)) {
      err = "read failed: " + source.path.string();
      return false;
    }
    out.write(buf.data(), static_cast<std::streamsize>(chunk));
    if (!out) {
      err = "write failed while streaming part: " + source.path.string();
      return false;
    }
    crc_state = crc32_update(crc_state, buf.data(), chunk);
    remaining -= chunk;
  }
  written = n;
  if (crc_out) {
    *crc_out = crc_state ^ 0xFFFFFFFFu;
  }
  return true;
}

// ---- image format sniffing ---------------------------------------------------

uint32_t sniff_format(const fsys::path& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return TPKG_FORMAT_AUTO;
  }
  unsigned char magic[8] = {0};
  in.read(reinterpret_cast<char*>(magic), sizeof magic);
  std::streamsize got = in.gcount();
  if (got >= 6 && std::memcmp(magic, "DWARFS", 6) == 0) {
    return TPKG_FORMAT_DWARFS;
  }
  if (got >= 4 && std::memcmp(magic, "hsqs", 4) == 0) {
    return TPKG_FORMAT_SQUASHFS;
  }
  if (got >= 4 && (std::memcmp(magic, "PK\x03\x04", 4) == 0 || std::memcmp(magic, "PK\x05\x06", 4) == 0)) {
    return TPKG_FORMAT_ZIP;
  }
  return TPKG_FORMAT_AUTO;
}

const char* format_name(uint32_t format_id)
{
  switch (format_id) {
    case TPKG_FORMAT_DWARFS:
      return "dwarfs";
    case TPKG_FORMAT_SQUASHFS:
      return "squashfs";
    case TPKG_FORMAT_ZIP:
      return "zip";
    default:
      return "auto";
  }
}

// ---- assemble core -----------------------------------------------------------

PackageResult assemble(const PartSource& bootstrap,
                       const std::vector<SlotSource>& slots,
                       const std::string& output,
                       const PackageOptions& options)
{
  if (slots.empty() || slots.size() > TPKG_MAX_SLOTS) {
    return fail("slot count out of range (1.." + std::to_string(TPKG_MAX_SLOTS) + ")");
  }
  if (options.runtime_ref.size() >= TPKG_RUNTIME_REF_LEN) {
    return fail("runtime_ref is too long (max " + std::to_string(TPKG_RUNTIME_REF_LEN - 1) + " characters)");
  }
  for (const auto& s : slots) {
    if (s.mount_point.size() >= TPKG_MOUNT_POINT_LEN) {
      return fail("mount point is too long (max " + std::to_string(TPKG_MOUNT_POINT_LEN - 1) +
                  " characters): " + s.mount_point);
    }
  }

  // Refuse to clobber an input part
  fsys::path out_canon = fsys::weakly_canonical(output);
  auto clashes = [&out_canon](const fsys::path& p) { return fsys::weakly_canonical(p) == out_canon; };
  if (clashes(bootstrap.path)) {
    return fail("output path must differ from the bootstrap file: " + bootstrap.path.string());
  }
  for (const auto& s : slots) {
    if (s.source.path != bootstrap.path && clashes(s.source.path)) {
      return fail("output path must differ from the image file: " + s.source.path.string());
    }
  }

  tpkg_manifest m{};
  m.version = TPKG_VERSION;
  m.package_flags = options.package_flags;
  m.slot_count = static_cast<uint32_t>(slots.size());
  m.launcher_abi = options.launcher_abi;
  std::strncpy(m.runtime_ref, options.runtime_ref.c_str(), TPKG_RUNTIME_REF_LEN - 1);

  {
    std::ofstream out(output, std::ios::binary | std::ios::trunc);
    if (!out) {
      return fail("cannot create output file: " + output);
    }
    std::string err;
    uint64_t written = 0;
    uint64_t total = 0;
    if (!stream_part(bootstrap, out, written, nullptr, err)) {
      out.close();
      fsys::remove(output);
      return fail(err);
    }
    total += written;
    for (size_t i = 0; i < slots.size(); i++) {
      written = 0;
      if (!stream_part(slots[i].source, out, written, nullptr, err)) {
        out.close();
        fsys::remove(output);
        return fail(err);
      }
      m.slots[i].offset = total;
      m.slots[i].size = written;
      m.slots[i].format_id = slots[i].format_id;
      m.slots[i].flags = slots[i].flags;
      std::strncpy(m.slots[i].mount_point, slots[i].mount_point.c_str(), TPKG_MOUNT_POINT_LEN - 1);
      total += written;
    }
    out.flush();
    if (!out) {
      out.close();
      fsys::remove(output);
      return fail("write failed: " + output);
    }
  }

  int fd = open_fd(output, true);
  if (fd < 0) {
    fsys::remove(output);
    return fail("cannot open output file for trailer write: " + output);
  }
  int rc = tpkg_write_fd(fd, &m);
  int terr = tpkg_errno();
  close_fd(fd);
  if (rc != 0) {
    fsys::remove(output);
    return fail(std::string("tpkg trailer write failed: ") + tpkg_strerror(terr));
  }
  return success();
}

PackageResult require_manifest(const std::string& binary, tpkg_manifest& m)
{
  int terr = 0;
  if (!package_probe(binary, m, terr)) {
    if (terr == TPKG_ERR_NO_TRAILER) {
      return fail(binary + ": no tpkg manifest trailer present (not a three-part package)");
    }
    if (terr == TPKG_ERR_IO) {
      return fail(binary + ": cannot read file");
    }
    return fail(binary + ": " + tpkg_strerror(terr));
  }
  return success();
}

// Rewrites `binary` from new part sources; the original is replaced (keeping
// its permissions) only after the new file has been written completely.
PackageResult rewrite_in_place(const std::string& binary,
                               const PartSource& bootstrap,
                               const std::vector<SlotSource>& slots,
                               const PackageOptions& options)
{
  std::error_code ec;
  auto perms = fsys::status(binary, ec).permissions();

  static int counter = 0;
  fsys::path tmp = fsys::path(binary).parent_path() / (fsys::path(binary).filename().string() + ".tpkg-tmp-" +
                                                       std::to_string(current_pid()) + "-" + std::to_string(counter++));

  auto res = assemble(bootstrap, slots, tmp.string(), options);
  if (!res.ok) {
    return res;
  }
  if (!ec) {
    fsys::permissions(tmp, perms, ec);
  }
  fsys::rename(tmp, binary, ec);
  if (ec) {
    // Windows cannot rename over an existing file
    std::error_code ec2;
    fsys::remove(binary, ec2);
    ec.clear();
    fsys::rename(tmp, binary, ec);
  }
  if (ec) {
    fsys::remove(tmp, ec);
    return fail("cannot replace " + binary + " with the rewritten package");
  }
  return success();
}

// Slot sources pointing at the byte ranges of an existing package binary.
std::vector<SlotSource> slots_from_manifest(const std::string& binary, const tpkg_manifest& m)
{
  std::vector<SlotSource> slots;
  for (uint32_t i = 0; i < m.slot_count; i++) {
    SlotSource s;
    s.source = PartSource{binary, m.slots[i].offset, m.slots[i].size};
    s.format_id = m.slots[i].format_id;
    s.flags = m.slots[i].flags;
    s.mount_point = m.slots[i].mount_point;
    slots.push_back(std::move(s));
  }
  return slots;
}

PackageOptions options_from_manifest(const tpkg_manifest& m)
{
  PackageOptions opts;
  opts.runtime_ref = m.runtime_ref;
  opts.package_flags = m.package_flags;
  opts.launcher_abi = m.launcher_abi;
  return opts;
}

// ---- JSON writing ------------------------------------------------------------

std::string json_escape(const std::string& s)
{
  std::string out;
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        }
        else {
          out += c;
        }
    }
  }
  return out;
}

// ---- minimal JSON parser (for manifest.json) ---------------------------------

struct JsonValue {
  enum Type { Null, Bool, Number, String, Array, Object };
  Type type = Null;
  bool boolean = false;
  std::string str;                                         // String value, or raw Number text
  std::vector<JsonValue> items;                            // Array
  std::vector<std::pair<std::string, JsonValue>> members;  // Object

  const JsonValue* find(const std::string& key) const
  {
    if (type != Object) {
      return nullptr;
    }
    for (const auto& kv : members) {
      if (kv.first == key) {
        return &kv.second;
      }
    }
    return nullptr;
  }

  bool as_u64(uint64_t& out) const
  {
    if (type != Number) {
      return false;
    }
    char* end = nullptr;
    unsigned long long v = std::strtoull(str.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
      return false;
    }
    out = static_cast<uint64_t>(v);
    return true;
  }

  bool as_string(std::string& out) const
  {
    if (type != String) {
      return false;
    }
    out = str;
    return true;
  }
};

class JsonParser {
 public:
  explicit JsonParser(const std::string& text) : s_(text) {}

  bool parse(JsonValue& out, std::string& err)
  {
    skip_ws();
    if (!parse_value(out, err)) {
      return false;
    }
    skip_ws();
    if (pos_ != s_.size()) {
      err = "trailing characters after JSON value";
      return false;
    }
    return true;
  }

 private:
  const std::string& s_;
  size_t pos_ = 0;

  void skip_ws()
  {
    while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) {
      pos_++;
    }
  }

  bool expect(char c, std::string& err)
  {
    if (pos_ >= s_.size() || s_[pos_] != c) {
      err = std::string("expected '") + c + "'";
      return false;
    }
    pos_++;
    return true;
  }

  bool parse_value(JsonValue& out, std::string& err)
  {
    skip_ws();
    if (pos_ >= s_.size()) {
      err = "unexpected end of input";
      return false;
    }
    char c = s_[pos_];
    if (c == '{') {
      return parse_object(out, err);
    }
    if (c == '[') {
      return parse_array(out, err);
    }
    if (c == '"') {
      out.type = JsonValue::String;
      return parse_string(out.str, err);
    }
    if (s_.compare(pos_, 4, "true") == 0) {
      out.type = JsonValue::Bool;
      out.boolean = true;
      pos_ += 4;
      return true;
    }
    if (s_.compare(pos_, 5, "false") == 0) {
      out.type = JsonValue::Bool;
      out.boolean = false;
      pos_ += 5;
      return true;
    }
    if (s_.compare(pos_, 4, "null") == 0) {
      out.type = JsonValue::Null;
      pos_ += 4;
      return true;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return parse_number(out, err);
    }
    err = "unexpected character in JSON input";
    return false;
  }

  bool parse_object(JsonValue& out, std::string& err)
  {
    out.type = JsonValue::Object;
    pos_++;  // '{'
    skip_ws();
    if (pos_ < s_.size() && s_[pos_] == '}') {
      pos_++;
      return true;
    }
    while (true) {
      skip_ws();
      std::string key;
      if (!parse_string(key, err)) {
        return false;
      }
      skip_ws();
      if (!expect(':', err)) {
        return false;
      }
      JsonValue val;
      if (!parse_value(val, err)) {
        return false;
      }
      out.members.emplace_back(std::move(key), std::move(val));
      skip_ws();
      if (pos_ < s_.size() && s_[pos_] == ',') {
        pos_++;
        continue;
      }
      if (pos_ < s_.size() && s_[pos_] == '}') {
        pos_++;
        return true;
      }
      err = "expected ',' or '}' in object";
      return false;
    }
  }

  bool parse_array(JsonValue& out, std::string& err)
  {
    out.type = JsonValue::Array;
    pos_++;  // '['
    skip_ws();
    if (pos_ < s_.size() && s_[pos_] == ']') {
      pos_++;
      return true;
    }
    while (true) {
      JsonValue val;
      if (!parse_value(val, err)) {
        return false;
      }
      out.items.push_back(std::move(val));
      skip_ws();
      if (pos_ < s_.size() && s_[pos_] == ',') {
        pos_++;
        continue;
      }
      if (pos_ < s_.size() && s_[pos_] == ']') {
        pos_++;
        return true;
      }
      err = "expected ',' or ']' in array";
      return false;
    }
  }

  bool parse_string(std::string& out, std::string& err)
  {
    if (!expect('"', err)) {
      return false;
    }
    out.clear();
    while (pos_ < s_.size()) {
      char c = s_[pos_++];
      if (c == '"') {
        return true;
      }
      if (c == '\\') {
        if (pos_ >= s_.size()) {
          break;
        }
        char esc = s_[pos_++];
        switch (esc) {
          case '"':
            out += '"';
            break;
          case '\\':
            out += '\\';
            break;
          case '/':
            out += '/';
            break;
          case 'b':
            out += '\b';
            break;
          case 'f':
            out += '\f';
            break;
          case 'n':
            out += '\n';
            break;
          case 'r':
            out += '\r';
            break;
          case 't':
            out += '\t';
            break;
          case 'u': {
            if (pos_ + 4 > s_.size()) {
              err = "truncated \\u escape";
              return false;
            }
            unsigned code = 0;
            for (int i = 0; i < 4; i++) {
              char h = s_[pos_++];
              code <<= 4;
              if (h >= '0' && h <= '9') {
                code |= static_cast<unsigned>(h - '0');
              }
              else if (h >= 'a' && h <= 'f') {
                code |= static_cast<unsigned>(h - 'a' + 10);
              }
              else if (h >= 'A' && h <= 'F') {
                code |= static_cast<unsigned>(h - 'A' + 10);
              }
              else {
                err = "invalid \\u escape";
                return false;
              }
            }
            if (code < 0x80) {
              out += static_cast<char>(code);
            }
            else if (code < 0x800) {
              out += static_cast<char>(0xC0 | (code >> 6));
              out += static_cast<char>(0x80 | (code & 0x3F));
            }
            else {
              out += static_cast<char>(0xE0 | (code >> 12));
              out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
              out += static_cast<char>(0x80 | (code & 0x3F));
            }
            break;
          }
          default:
            err = "invalid escape sequence";
            return false;
        }
      }
      else {
        out += c;
      }
    }
    err = "unterminated string";
    return false;
  }

  bool parse_number(JsonValue& out, std::string& err)
  {
    out.type = JsonValue::Number;
    size_t start = pos_;
    if (pos_ < s_.size() && s_[pos_] == '-') {
      pos_++;
    }
    while (pos_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.' ||
                                s_[pos_] == 'e' || s_[pos_] == 'E' || s_[pos_] == '+' || s_[pos_] == '-')) {
      pos_++;
    }
    if (pos_ == start) {
      err = "invalid number";
      return false;
    }
    out.str = s_.substr(start, pos_ - start);
    return true;
  }
};

// Reads a manifest member file name, rejecting paths that escape the
// unbundled directory.
bool safe_part_name(const std::string& name)
{
  fsys::path p(name);
  return !name.empty() && p.filename().string() == name && name != "." && name != "..";
}

// ---- mkdwarfs invocation -----------------------------------------------------

std::string shell_quote(const std::string& arg)
{
  std::string out = "\"";
  for (char c : arg) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  out += "\"";
  return out;
}

std::string find_on_path(const std::string& name)
{
  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return "";
  }
#ifdef _WIN32
  const char sep = ';';
  const std::vector<std::string> names = {name + ".exe", name};
#else
  const char sep = ':';
  const std::vector<std::string> names = {name};
#endif
  std::string paths(path_env);
  size_t start = 0;
  while (start <= paths.size()) {
    size_t end = paths.find(sep, start);
    std::string dir = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!dir.empty()) {
      for (const auto& n : names) {
        fsys::path cand = fsys::path(dir) / n;
        std::error_code ec;
        if (fsys::is_regular_file(cand, ec)) {
#ifndef _WIN32
          if (::access(cand.string().c_str(), X_OK) == 0) {
            return cand.string();
          }
#else
          return cand.string();
#endif
        }
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return "";
}

}  // namespace

// ---- public API --------------------------------------------------------------

std::string package_default_mount(uint32_t slot_index)
{
  if (slot_index == 0) {
    return "/__tebako_memfs__";
  }
  return "/__tebako_memfs_" + std::to_string(slot_index) + "__";
}

PackageImage package_parse_image_spec(const std::string& spec)
{
  PackageImage img;
  auto colon = spec.find_last_of(':');
  if (colon != std::string::npos && colon + 1 < spec.size() && spec[colon + 1] == '/') {
    img.path = spec.substr(0, colon);
    img.mount_point = spec.substr(colon + 1);
  }
  else {
    img.path = spec;
  }
  return img;
}

bool package_probe(const std::string& binary, tpkg_manifest& out, int& tpkg_err)
{
  int fd = open_fd(binary, false);
  if (fd < 0) {
    tpkg_err = TPKG_ERR_IO;
    return false;
  }
  int rc = tpkg_read_fd(fd, &out);
  tpkg_err = tpkg_errno();
  close_fd(fd);
  return rc == 0;
}

PackageResult package_bundle(const std::string& bootstrap,
                             const std::vector<PackageImage>& images,
                             const std::string& output,
                             const PackageOptions& options)
{
  if (images.empty() || images.size() > TPKG_MAX_SLOTS) {
    return fail("image count out of range (1.." + std::to_string(TPKG_MAX_SLOTS) + ")");
  }
  std::error_code ec;
  if (!fsys::is_regular_file(bootstrap, ec)) {
    return fail("bootstrap file not found: " + bootstrap);
  }
  PartSource boot{bootstrap, 0, UINT64_MAX};

  std::vector<SlotSource> slots;
  for (size_t i = 0; i < images.size(); i++) {
    if (!fsys::is_regular_file(images[i].path, ec)) {
      return fail("image file not found: " + images[i].path);
    }
    SlotSource s;
    s.source = PartSource{images[i].path, 0, UINT64_MAX};
    s.format_id = images[i].format_id == TPKG_FORMAT_AUTO ? sniff_format(images[i].path) : images[i].format_id;
    s.flags = 0;
    s.mount_point =
        images[i].mount_point.empty() ? package_default_mount(static_cast<uint32_t>(i)) : images[i].mount_point;
    slots.push_back(std::move(s));
  }
  return assemble(boot, slots, output, options);
}

PackageResult package_unbundle(const std::string& binary, const std::string& output_dir)
{
  tpkg_manifest m;
  auto res = require_manifest(binary, m);
  if (!res.ok) {
    return res;
  }

  std::error_code ec;
  uint64_t total_size = fsys::file_size(binary, ec);
  if (ec) {
    return fail("cannot stat " + binary + ": " + ec.message());
  }

  // The manifest struct does not expose the raw header fields; re-read the
  // fixed-size header for slot_table_offset / header_crc32 reporting.
  uint64_t table_off = total_size - TPKG_HEADER_SIZE - static_cast<uint64_t>(m.slot_count) * TPKG_SLOT_SIZE;
  uint32_t header_crc = 0;
  {
    std::ifstream in(binary, std::ios::binary);
    if (!in) {
      return fail("cannot open " + binary);
    }
    in.seekg(static_cast<std::streamoff>(total_size - TPKG_HEADER_SIZE));
    uint8_t hdr[TPKG_HEADER_SIZE];
    in.read(reinterpret_cast<char*>(hdr), sizeof hdr);
    if (in.gcount() != static_cast<std::streamsize>(sizeof hdr)) {
      return fail("cannot read trailer header of " + binary);
    }
    table_off = 0;
    for (int i = 0; i < 8; i++) {
      table_off |= static_cast<uint64_t>(hdr[22 + i]) << (8 * i);
    }
    header_crc = static_cast<uint32_t>(hdr[162]) | (static_cast<uint32_t>(hdr[163]) << 8) |
                 (static_cast<uint32_t>(hdr[164]) << 16) | (static_cast<uint32_t>(hdr[165]) << 24);
  }

  // Slot ranges must lie entirely before the slot table.
  for (uint32_t i = 0; i < m.slot_count; i++) {
    if (m.slots[i].offset > table_off || m.slots[i].size > table_off - m.slots[i].offset) {
      return fail("slot " + std::to_string(i) + " byte range is out of bounds (corrupt package)");
    }
  }
  // The bootstrap is everything before slot 0.
  uint64_t bootstrap_size = m.slot_count > 0 ? m.slots[0].offset : 0;

  // Warn about byte ranges covered by no slot (dropped on reassemble).
  uint64_t expected = bootstrap_size;
  for (uint32_t i = 0; i < m.slot_count; i++) {
    if (m.slots[i].offset > expected) {
      std::cerr << "unbundle: warning: dropping " << (m.slots[i].offset - expected) << " gap byte(s) before slot " << i
                << std::endl;
    }
    expected = m.slots[i].offset + m.slots[i].size;
  }
  if (expected < table_off) {
    std::cerr << "unbundle: warning: dropping " << (table_off - expected) << " trailing gap byte(s)" << std::endl;
  }

  fsys::create_directories(output_dir, ec);
  if (ec) {
    return fail("cannot create output directory " + output_dir + ": " + ec.message());
  }

  // Stream the parts out, computing per-part checksums.
  struct PartOut {
    std::string file;
    uint64_t size;
    uint32_t crc;
  };
  std::vector<PartOut> parts;
  auto stream_out = [&](const PartSource& src, const std::string& name) -> PackageResult {
    fsys::path dest = fsys::path(output_dir) / name;
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out) {
      return fail("cannot create part file: " + dest.string());
    }
    uint64_t written = 0;
    uint32_t crc = 0;
    std::string err;
    if (!stream_part(src, out, written, &crc, err)) {
      out.close();
      fsys::remove(dest);
      return fail(err);
    }
    out.close();
    parts.push_back(PartOut{name, written, crc});
    return success();
  };

  res = stream_out(PartSource{binary, 0, bootstrap_size}, "bootstrap.bin");
  if (!res.ok) {
    return res;
  }
  for (uint32_t i = 0; i < m.slot_count; i++) {
    res = stream_out(PartSource{binary, m.slots[i].offset, m.slots[i].size}, "image-" + std::to_string(i) + ".bin");
    if (!res.ok) {
      return res;
    }
  }

  // manifest.json (deterministic formatting; checksums via the tpkg CRC-32)
  std::ostringstream json;
  json << "{\n";
  json << "  \"format\": \"tpkg\",\n";
  json << "  \"format_version\": " << m.version << ",\n";
  json << "  \"package_flags\": " << m.package_flags << ",\n";
  json << "  \"launcher_abi\": " << m.launcher_abi << ",\n";
  json << "  \"runtime_ref\": \"" << json_escape(m.runtime_ref) << "\",\n";
  json << "  \"slot_table_offset\": " << table_off << ",\n";
  json << "  \"header_crc32\": " << header_crc << ",\n";
  json << "  \"bootstrap\": { \"file\": \"" << parts[0].file << "\", \"size\": " << parts[0].size
       << ", \"crc32\": " << parts[0].crc << " },\n";
  json << "  \"slots\": [\n";
  for (uint32_t i = 0; i < m.slot_count; i++) {
    const auto& p = parts[i + 1];
    json << "    { \"index\": " << i << ", \"file\": \"" << p.file << "\", \"offset\": " << m.slots[i].offset
         << ", \"size\": " << m.slots[i].size << ", \"format_id\": " << m.slots[i].format_id << ", \"format\": \""
         << format_name(m.slots[i].format_id) << "\", \"flags\": " << m.slots[i].flags << ", \"mount_point\": \""
         << json_escape(m.slots[i].mount_point) << "\", \"crc32\": " << p.crc << " }";
    json << (i + 1 < m.slot_count ? ",\n" : "\n");
  }
  json << "  ]\n";
  json << "}\n";

  fsys::path manifest_path = fsys::path(output_dir) / "manifest.json";
  {
    std::ofstream out(manifest_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      return fail("cannot create " + manifest_path.string());
    }
    out << json.str();
    if (!out) {
      return fail("write failed: " + manifest_path.string());
    }
  }
  return success();
}

PackageResult package_reassemble(const std::string& input_dir, const std::string& output)
{
  fsys::path dir = input_dir;
  fsys::path manifest_path = dir / "manifest.json";
  std::error_code ec;
  if (!fsys::is_regular_file(manifest_path, ec)) {
    return fail("manifest.json not found in " + input_dir + " (not an unbundled package directory)");
  }
  std::string text;
  {
    std::ifstream in(manifest_path, std::ios::binary);
    if (!in) {
      return fail("cannot read " + manifest_path.string());
    }
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }

  JsonValue root;
  std::string err;
  if (!JsonParser(text).parse(root, err)) {
    return fail("cannot parse " + manifest_path.string() + ": " + err);
  }
  if (root.type != JsonValue::Object) {
    return fail(manifest_path.string() + ": top-level value must be an object");
  }
  if (const JsonValue* fmt = root.find("format")) {
    std::string f;
    if (!fmt->as_string(f) || f != "tpkg") {
      return fail(manifest_path.string() + ": unsupported format (expected \"tpkg\")");
    }
  }

  PackageOptions opts;
  uint64_t v = 0;
  if (const JsonValue* f = root.find("package_flags")) {
    if (!f->as_u64(v)) {
      return fail(manifest_path.string() + ": package_flags must be an unsigned integer");
    }
    opts.package_flags = static_cast<uint32_t>(v);
  }
  if (const JsonValue* a = root.find("launcher_abi")) {
    if (!a->as_u64(v)) {
      return fail(manifest_path.string() + ": launcher_abi must be an unsigned integer");
    }
    opts.launcher_abi = static_cast<uint32_t>(v);
  }
  if (const JsonValue* r = root.find("runtime_ref")) {
    if (!r->as_string(opts.runtime_ref)) {
      return fail(manifest_path.string() + ": runtime_ref must be a string");
    }
  }

  std::string bootstrap_name = "bootstrap.bin";
  if (const JsonValue* b = root.find("bootstrap")) {
    if (const JsonValue* f = b->find("file")) {
      if (!f->as_string(bootstrap_name)) {
        return fail(manifest_path.string() + ": bootstrap.file must be a string");
      }
    }
  }
  if (!safe_part_name(bootstrap_name)) {
    return fail(manifest_path.string() + ": unsafe bootstrap file name: " + bootstrap_name);
  }
  fsys::path bootstrap_path = dir / bootstrap_name;
  if (!fsys::is_regular_file(bootstrap_path, ec)) {
    return fail("bootstrap part not found: " + bootstrap_path.string());
  }

  const JsonValue* slots_json = root.find("slots");
  if (slots_json == nullptr || slots_json->type != JsonValue::Array || slots_json->items.empty() ||
      slots_json->items.size() > TPKG_MAX_SLOTS) {
    return fail(manifest_path.string() + ": slots must be an array of 1.." + std::to_string(TPKG_MAX_SLOTS) +
                " entries");
  }

  std::vector<SlotSource> slots;
  for (size_t i = 0; i < slots_json->items.size(); i++) {
    const JsonValue& sj = slots_json->items[i];
    std::string file;
    const JsonValue* fj = sj.find("file");
    if (fj == nullptr || !fj->as_string(file)) {
      return fail(manifest_path.string() + ": slots[" + std::to_string(i) + "].file is required");
    }
    if (!safe_part_name(file)) {
      return fail(manifest_path.string() + ": unsafe slot file name: " + file);
    }
    fsys::path part_path = dir / file;
    if (!fsys::is_regular_file(part_path, ec)) {
      return fail("slot part not found: " + part_path.string());
    }

    SlotSource s;
    s.source = PartSource{part_path, 0, UINT64_MAX};
    s.mount_point = package_default_mount(static_cast<uint32_t>(i));
    s.format_id = sniff_format(part_path);
    s.flags = 0;
    if (const JsonValue* mp = sj.find("mount_point")) {
      if (!mp->as_string(s.mount_point)) {
        return fail(manifest_path.string() + ": slots[" + std::to_string(i) + "].mount_point must be a string");
      }
    }
    if (const JsonValue* fi = sj.find("format_id")) {
      if (!fi->as_u64(v)) {
        return fail(manifest_path.string() + ": slots[" + std::to_string(i) +
                    "].format_id must be an unsigned integer");
      }
      s.format_id = static_cast<uint32_t>(v);
    }
    if (const JsonValue* fl = sj.find("flags")) {
      if (!fl->as_u64(v)) {
        return fail(manifest_path.string() + ": slots[" + std::to_string(i) + "].flags must be an unsigned integer");
      }
      s.flags = static_cast<uint32_t>(v);
    }
    slots.push_back(std::move(s));
  }

  return assemble(PartSource{bootstrap_path, 0, UINT64_MAX}, slots, output, opts);
}

PackageResult package_insert_image(const std::string& binary, const std::string& image, const std::string& mount_point)
{
  tpkg_manifest m;
  auto res = require_manifest(binary, m);
  if (!res.ok) {
    return res;
  }
  if (m.slot_count >= TPKG_MAX_SLOTS) {
    return fail(binary + ": package already has the maximum of " + std::to_string(TPKG_MAX_SLOTS) + " image slots");
  }
  std::error_code ec;
  if (!fsys::is_regular_file(image, ec)) {
    return fail("image file not found: " + image);
  }
  if (fsys::weakly_canonical(image) == fsys::weakly_canonical(binary)) {
    return fail("cannot insert the package into itself");
  }

  auto slots = slots_from_manifest(binary, m);
  SlotSource s;
  s.source = PartSource{image, 0, UINT64_MAX};
  s.format_id = sniff_format(image);
  s.flags = 0;
  s.mount_point = mount_point.empty() ? package_default_mount(m.slot_count) : mount_point;
  slots.push_back(std::move(s));

  return rewrite_in_place(binary, PartSource{binary, 0, m.slots[0].offset}, slots, options_from_manifest(m));
}

PackageResult package_remove_image(const std::string& binary, uint32_t slot_index)
{
  tpkg_manifest m;
  auto res = require_manifest(binary, m);
  if (!res.ok) {
    return res;
  }
  if (slot_index >= m.slot_count) {
    return fail(binary + ": slot index " + std::to_string(slot_index) + " out of range (package has " +
                std::to_string(m.slot_count) + " slot(s))");
  }
  if (m.slot_count == 1) {
    return fail(binary + ": cannot remove the last image slot (a manifest requires at least one slot)");
  }

  auto slots = slots_from_manifest(binary, m);
  slots.erase(slots.begin() + slot_index);

  return rewrite_in_place(binary, PartSource{binary, 0, m.slots[0].offset}, slots, options_from_manifest(m));
}

PackageResult package_set_runtime(const std::string& binary, const std::string& runtime_file)
{
  tpkg_manifest m;
  auto res = require_manifest(binary, m);
  if (!res.ok) {
    return res;
  }
  std::error_code ec;
  if (!fsys::is_regular_file(runtime_file, ec)) {
    return fail("runtime file not found: " + runtime_file);
  }
  if (fsys::weakly_canonical(runtime_file) == fsys::weakly_canonical(binary)) {
    return fail("cannot use the package as its own runtime");
  }

  auto slots = slots_from_manifest(binary, m);
  return rewrite_in_place(binary, PartSource{runtime_file, 0, UINT64_MAX}, slots, options_from_manifest(m));
}

PackageResult package_mkimage(const std::string& format,
                              const std::string& source_dir,
                              const std::string& output,
                              const std::string& tool)
{
  std::string fmt;
  for (char c : format) {
    fmt += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (fmt == "zip") {
    return fail("mkimage --format zip is not supported: the zip backend is read-only (only 'dwarfs' can be written)");
  }
  if (fmt == "squashfs") {
    return fail("mkimage --format squashfs is not supported (LGPL; opt-in source builds only)");
  }
  if (fmt != "dwarfs") {
    return fail("unsupported image format '" + format + "' (supported: dwarfs)");
  }

  std::error_code ec;
  if (!fsys::is_directory(source_dir, ec)) {
    return fail("source directory not found: " + source_dir);
  }

  std::string exe = tool;
  if (exe.empty()) {
    const char* env = std::getenv("TEBAKO_MKDWARFS");
    if (env != nullptr && env[0] != '\0') {
      exe = env;
    }
  }
  if (exe.empty()) {
    exe = find_on_path("mkdwarfs");
  }
  if (exe.empty()) {
    return fail("mkdwarfs not found on PATH (install dwarfs or set TEBAKO_MKDWARFS)");
  }
  if (!fsys::is_regular_file(exe, ec)) {
    return fail("mkdwarfs not found: " + exe);
  }

  std::string cmd =
      shell_quote(exe) + " -i " + shell_quote(source_dir) + " -o " + shell_quote(output) + " --no-progress --force";
  int rc = std::system(cmd.c_str());
  if (rc != 0) {
    return fail("mkdwarfs failed (exit code " + std::to_string(rc) + "): " + cmd);
  }
  if (!fsys::is_regular_file(output, ec)) {
    return fail("mkdwarfs did not produce an output file: " + output);
  }
  return success();
}

}  // namespace cli
}  // namespace fs
}  // namespace tebako
