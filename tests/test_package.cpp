/**
 * @file test_package.cpp
 * @brief Tests for tebakofs three-part package tooling (src/cli/package.cpp)
 *
 * Covers the golden round-trip (bundle -> unbundle -> reassemble identity),
 * manifest.json contents, granular ops (insert-image/remove-image/set-runtime)
 * and mkimage, both through the package API directly and end-to-end through
 * the built tebakofs binary. Fixture binaries are small synthetic byte blobs
 * (no real tebako runtimes).
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define popen _popen
#define pclose _pclose
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <tebako/fs/cli/package.h>

namespace {

namespace fsys = std::filesystem;
using tebako::fs::cli::PackageImage;
using tebako::fs::cli::PackageOptions;
using tebako::fs::cli::PackageResult;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

std::vector<uint8_t> Payload(size_t n, uint8_t seed = 7)
{
  std::vector<uint8_t> b(n);
  for (size_t i = 0; i < n; i++) {
    b[i] = static_cast<uint8_t>((i * 31 + seed) & 0xFF);
  }
  return b;
}

// Bytes that sniff as a dwarfs image (magic at offset 0)
std::vector<uint8_t> Dwarfsish(size_t n)
{
  auto b = Payload(n, 41);
  const char* magic = "DWARFS";
  std::memcpy(b.data(), magic, 6);
  return b;
}

std::vector<uint8_t> ReadFile(const fsys::path& p)
{
  std::ifstream in(p, std::ios::binary);
  EXPECT_TRUE(in.good()) << "cannot open " << p;
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteFile(const fsys::path& p, const std::vector<uint8_t>& bytes)
{
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.good()) << "cannot create " << p;
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(out.good()) << "cannot write " << p;
}

std::string ReadText(const fsys::path& p)
{
  std::ifstream in(p, std::ios::binary);
  EXPECT_TRUE(in.good()) << "cannot open " << p;
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

class TempDir {
 public:
  explicit TempDir(const char* tag)
  {
    static int counter = 0;
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    path_ = fsys::temp_directory_path() /
            ("pkg_test_" + std::string(info ? info->name() : "anon") + "_" + tag + "_" + std::to_string(counter++));
    std::error_code ec;
    fsys::create_directories(path_, ec);
  }
  ~TempDir()
  {
    std::error_code ec;
    fsys::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  const fsys::path& path() const { return path_; }
  fsys::path operator/(const std::string& name) const { return path_ / name; }

 private:
  fsys::path path_;
};

tpkg_manifest Probe(const fsys::path& binary)
{
  tpkg_manifest m{};
  int terr = 0;
  EXPECT_TRUE(tebako::fs::cli::package_probe(binary.string(), m, terr))
      << "probe failed on " << binary << ": " << tpkg_strerror(terr);
  return m;
}

void ExpectOk(const PackageResult& res)
{
  EXPECT_TRUE(res.ok) << res.error;
}

struct CliResult {
  int exit_code = -1;
  std::string output;
};

// Runs the built tebakofs binary (compile-time path) with the given arguments.
CliResult RunCli(const std::string& args)
{
  std::string cmd = "\"" TEBAKOFS_BINARY "\" " + args + " 2>&1";
  CliResult res;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return res;
  }
  char buf[512];
  while (fgets(buf, sizeof buf, pipe)) {
    res.output += buf;
  }
  int status = pclose(pipe);
#ifdef _WIN32
  res.exit_code = status;
#else
  res.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
  return res;
}

std::string Q(const fsys::path& p)
{
  return "\"" + p.string() + "\"";
}

constexpr uint64_t kSlotSize = TPKG_SLOT_SIZE;
constexpr uint64_t kHeaderSize = TPKG_HEADER_SIZE;

}  // namespace

// ============================================================================
// image spec parsing
// ============================================================================

TEST(PackageSpec, Parse)
{
  auto plain = tebako::fs::cli::package_parse_image_spec("app.dwarfs");
  EXPECT_EQ(plain.path, "app.dwarfs");
  EXPECT_TRUE(plain.mount_point.empty());

  auto with_mount = tebako::fs::cli::package_parse_image_spec("app.dwarfs:/app/data");
  EXPECT_EQ(with_mount.path, "app.dwarfs");
  EXPECT_EQ(with_mount.mount_point, "/app/data");

  // ':' not followed by '/' stays part of the file name (Windows drives, relatives)
  auto drive = tebako::fs::cli::package_parse_image_spec("C:\\images\\app.dwarfs");
  EXPECT_EQ(drive.path, "C:\\images\\app.dwarfs");
  EXPECT_TRUE(drive.mount_point.empty());
}

// ============================================================================
// bundle
// ============================================================================

TEST(PackageBundle, BasicLayout)
{
  TempDir tmp("basic");
  auto bootstrap = Payload(5000, 7);
  auto img1 = Dwarfsish(3000);
  auto img2 = Payload(4111, 99);
  WriteFile(tmp / "boot.bin", bootstrap);
  WriteFile(tmp / "i1.bin", img1);
  WriteFile(tmp / "i2.bin", img2);

  PackageOptions opts;
  auto out = (tmp / "pkg.bin").string();
  ExpectOk(tebako::fs::cli::package_bundle(
      (tmp / "boot.bin").string(),
      {PackageImage{(tmp / "i1.bin").string(), ""}, PackageImage{(tmp / "i2.bin").string(), ""}}, out, opts));

  auto m = Probe(tmp / "pkg.bin");
  ASSERT_EQ(m.slot_count, 2u);
  EXPECT_EQ(m.slots[0].offset, 5000u);
  EXPECT_EQ(m.slots[0].size, 3000u);
  EXPECT_EQ(m.slots[0].format_id, TPKG_FORMAT_DWARFS) << "dwarfs magic must be sniffed";
  EXPECT_STREQ(m.slots[0].mount_point, "/__tebako_memfs__");
  EXPECT_EQ(m.slots[1].offset, 8000u);
  EXPECT_EQ(m.slots[1].size, 4111u);
  EXPECT_EQ(m.slots[1].format_id, TPKG_FORMAT_AUTO) << "unknown magic stays auto";
  EXPECT_STREQ(m.slots[1].mount_point, "/__tebako_memfs_1__");

  auto bytes = ReadFile(tmp / "pkg.bin");
  ASSERT_EQ(bytes.size(), 5000u + 3000u + 4111u + 2 * kSlotSize + kHeaderSize);
  EXPECT_EQ(0, std::memcmp(bytes.data(), bootstrap.data(), bootstrap.size())) << "bootstrap bytes at offset 0";
  EXPECT_EQ(0, std::memcmp(bytes.data() + 5000, img1.data(), img1.size())) << "image 0 follows the bootstrap";
  EXPECT_EQ(0, std::memcmp(bytes.data() + 8000, img2.data(), img2.size())) << "image 1 follows image 0";
}

TEST(PackageBundle, CustomOptionsRoundTrip)
{
  TempDir tmp("opts");
  WriteFile(tmp / "boot.bin", Payload(100, 1));
  WriteFile(tmp / "i1.bin", Payload(100, 2));

  PackageOptions opts;
  opts.runtime_ref = "ruby@3.3.7;tebako=0.15.0";
  opts.package_flags = TPKG_FLAG_LEAN;
  opts.launcher_abi = 1;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}},
                                           (tmp / "pkg.bin").string(), opts));

  auto m = Probe(tmp / "pkg.bin");
  EXPECT_EQ(m.version, TPKG_VERSION);
  EXPECT_TRUE((m.package_flags & TPKG_FLAG_LEAN) != 0);
  EXPECT_EQ(m.launcher_abi, 1u);
  EXPECT_STREQ(m.runtime_ref, "ruby@3.3.7;tebako=0.15.0");
}

TEST(PackageBundle, ExplicitMountPointKept)
{
  TempDir tmp("mount");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i1.bin", Payload(64, 2));

  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(),
                                           {PackageImage{(tmp / "i1.bin").string(), "/custom/mount"}},
                                           (tmp / "pkg.bin").string(), opts));
  auto m = Probe(tmp / "pkg.bin");
  EXPECT_STREQ(m.slots[0].mount_point, "/custom/mount");
}

TEST(PackageBundle, NoImagesFails)
{
  TempDir tmp("noimg");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  PackageOptions opts;
  auto res = tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {}, (tmp / "pkg.bin").string(), opts);
  EXPECT_FALSE(res.ok);
  EXPECT_FALSE(fsys::exists(tmp / "pkg.bin")) << "rejected bundle must not leave an output file";
}

TEST(PackageBundle, TooManyImagesFails)
{
  TempDir tmp("maximg");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i.bin", Payload(64, 2));
  std::vector<PackageImage> images(TPKG_MAX_SLOTS + 1, PackageImage{(tmp / "i.bin").string(), ""});
  PackageOptions opts;
  auto res = tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), images, (tmp / "pkg.bin").string(), opts);
  EXPECT_FALSE(res.ok);
}

TEST(PackageBundle, MissingBootstrapFails)
{
  TempDir tmp("noboot");
  WriteFile(tmp / "i1.bin", Payload(64, 2));
  PackageOptions opts;
  auto res = tebako::fs::cli::package_bundle(
      (tmp / "missing.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}}, (tmp / "pkg.bin").string(), opts);
  EXPECT_FALSE(res.ok);
}

TEST(PackageBundle, OutputClobberingInputFails)
{
  TempDir tmp("clobber");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i1.bin", Payload(64, 2));
  PackageOptions opts;
  auto res = tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}},
                                             (tmp / "boot.bin").string(), opts);
  EXPECT_FALSE(res.ok) << "output == bootstrap must be rejected";
  EXPECT_EQ(ReadFile(tmp / "boot.bin"), Payload(64, 1)) << "input must be untouched";
}

// ============================================================================
// golden round-trip: bundle -> unbundle -> reassemble == original
// ============================================================================

TEST(PackageRoundTrip, BundleUnbundleReassembleIdentity)
{
  TempDir tmp("golden");
  auto bootstrap = Payload(5000, 7);
  auto img1 = Dwarfsish(3000);
  auto img2 = Payload(4111, 99);
  WriteFile(tmp / "boot.bin", bootstrap);
  WriteFile(tmp / "i1.bin", img1);
  WriteFile(tmp / "i2.bin", img2);

  PackageOptions opts;
  opts.runtime_ref = "ruby@3.3.7;tebako=0.15.0";
  opts.launcher_abi = 1;
  ExpectOk(tebako::fs::cli::package_bundle(
      (tmp / "boot.bin").string(),
      {PackageImage{(tmp / "i1.bin").string(), ""}, PackageImage{(tmp / "i2.bin").string(), "/data"}},
      (tmp / "pkg.bin").string(), opts));
  auto original = ReadFile(tmp / "pkg.bin");

  // unbundle
  ExpectOk(tebako::fs::cli::package_unbundle((tmp / "pkg.bin").string(), (tmp / "parts").string()));
  EXPECT_EQ(ReadFile(tmp / "parts" / "bootstrap.bin"), bootstrap);
  EXPECT_EQ(ReadFile(tmp / "parts" / "image-0.bin"), img1);
  EXPECT_EQ(ReadFile(tmp / "parts" / "image-1.bin"), img2);

  // manifest.json contents
  auto json = ReadText(tmp / "parts" / "manifest.json");
  EXPECT_NE(json.find("\"format\": \"tpkg\""), std::string::npos);
  EXPECT_NE(json.find("\"format_version\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"launcher_abi\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"runtime_ref\": \"ruby@3.3.7;tebako=0.15.0\""), std::string::npos);
  EXPECT_NE(json.find("\"bootstrap\": { \"file\": \"bootstrap.bin\", \"size\": 5000"), std::string::npos);
  EXPECT_NE(json.find("\"offset\": 5000"), std::string::npos);
  EXPECT_NE(json.find("\"size\": 3000"), std::string::npos);
  EXPECT_NE(json.find("\"format_id\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"format\": \"dwarfs\""), std::string::npos);
  EXPECT_NE(json.find("\"mount_point\": \"/__tebako_memfs__\""), std::string::npos);
  EXPECT_NE(json.find("\"mount_point\": \"/data\""), std::string::npos);
  // per-part crc32 as computed by the tpkg crc
  EXPECT_NE(json.find("\"crc32\": " + std::to_string(tpkg_crc32(img1.data(), img1.size()))), std::string::npos)
      << "image-0 crc32 must match tpkg_crc32";
  EXPECT_NE(json.find("\"crc32\": " + std::to_string(tpkg_crc32(bootstrap.data(), bootstrap.size()))),
            std::string::npos)
      << "bootstrap crc32 must match tpkg_crc32";

  // reassemble -> byte-identical
  ExpectOk(tebako::fs::cli::package_reassemble((tmp / "parts").string(), (tmp / "pkg2.bin").string()));
  EXPECT_EQ(ReadFile(tmp / "pkg2.bin"), original) << "reassemble must be byte-identical when nothing changed";
}

TEST(PackageRoundTrip, ReassembleAfterImageSwap)
{
  TempDir tmp("swap");
  auto bootstrap = Payload(5000, 7);
  auto img1 = Dwarfsish(3000);
  WriteFile(tmp / "boot.bin", bootstrap);
  WriteFile(tmp / "i1.bin", img1);
  WriteFile(tmp / "i2.bin", Payload(4111, 99));

  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle(
      (tmp / "boot.bin").string(),
      {PackageImage{(tmp / "i1.bin").string(), ""}, PackageImage{(tmp / "i2.bin").string(), ""}},
      (tmp / "pkg.bin").string(), opts));
  ExpectOk(tebako::fs::cli::package_unbundle((tmp / "pkg.bin").string(), (tmp / "parts").string()));

  // user swaps image-1 for a different image (different size)
  auto img2_new = Payload(999, 55);
  WriteFile(tmp / "parts" / "image-1.bin", img2_new);

  ExpectOk(tebako::fs::cli::package_reassemble((tmp / "parts").string(), (tmp / "pkg2.bin").string()));
  auto m = Probe(tmp / "pkg2.bin");
  ASSERT_EQ(m.slot_count, 2u);
  EXPECT_EQ(m.slots[0].offset, 5000u);
  EXPECT_EQ(m.slots[0].size, 3000u);
  EXPECT_EQ(m.slots[1].offset, 8000u) << "offsets recomputed";
  EXPECT_EQ(m.slots[1].size, 999u) << "swapped size picked up";

  auto bytes = ReadFile(tmp / "pkg2.bin");
  ASSERT_EQ(bytes.size(), 5000u + 3000u + 999u + 2 * kSlotSize + kHeaderSize);
  EXPECT_EQ(0, std::memcmp(bytes.data(), bootstrap.data(), bootstrap.size()));
  EXPECT_EQ(0, std::memcmp(bytes.data() + 5000, img1.data(), img1.size()));
  EXPECT_EQ(0, std::memcmp(bytes.data() + 8000, img2_new.data(), img2_new.size()));
}

TEST(PackageRoundTrip, ReassemblePicksUpManifestEdits)
{
  TempDir tmp("edit");
  WriteFile(tmp / "boot.bin", Payload(128, 7));
  WriteFile(tmp / "i1.bin", Payload(256, 8));

  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}},
                                           (tmp / "pkg.bin").string(), opts));
  ExpectOk(tebako::fs::cli::package_unbundle((tmp / "pkg.bin").string(), (tmp / "parts").string()));

  // user edits manifest.json metadata
  auto json = ReadText(tmp / "parts" / "manifest.json");
  auto pos = json.find("/__tebako_memfs__");
  ASSERT_NE(pos, std::string::npos);
  json.replace(pos, std::strlen("/__tebako_memfs__"), "/edited/mount");
  {
    std::ofstream out(tmp / "parts" / "manifest.json", std::ios::binary | std::ios::trunc);
    out << json;
  }

  ExpectOk(tebako::fs::cli::package_reassemble((tmp / "parts").string(), (tmp / "pkg2.bin").string()));
  auto m = Probe(tmp / "pkg2.bin");
  EXPECT_STREQ(m.slots[0].mount_point, "/edited/mount");
}

TEST(PackageRoundTrip, UnbundleNonPackageFails)
{
  TempDir tmp("nopkg");
  WriteFile(tmp / "plain.bin", Payload(4096, 3));
  auto res = tebako::fs::cli::package_unbundle((tmp / "plain.bin").string(), (tmp / "parts").string());
  EXPECT_FALSE(res.ok);
  EXPECT_NE(res.error.find("no tpkg manifest trailer"), std::string::npos);
}

TEST(PackageRoundTrip, ReassembleMissingManifestFails)
{
  TempDir tmp("nodir");
  auto res = tebako::fs::cli::package_reassemble((tmp / "parts").string(), (tmp / "pkg.bin").string());
  EXPECT_FALSE(res.ok);
}

// ============================================================================
// insert-image
// ============================================================================

TEST(PackageInsert, AppendWithMount)
{
  TempDir tmp("ins");
  auto bootstrap = Payload(5000, 7);
  auto img1 = Dwarfsish(3000);
  auto img2 = Payload(2000, 13);
  WriteFile(tmp / "boot.bin", bootstrap);
  WriteFile(tmp / "i1.bin", img1);
  WriteFile(tmp / "i2.bin", img2);

  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}},
                                           (tmp / "pkg.bin").string(), opts));

  ExpectOk(tebako::fs::cli::package_insert_image((tmp / "pkg.bin").string(), (tmp / "i2.bin").string(), "/data"));

  auto m = Probe(tmp / "pkg.bin");
  ASSERT_EQ(m.slot_count, 2u);
  EXPECT_EQ(m.slots[0].offset, 5000u);
  EXPECT_EQ(m.slots[0].size, 3000u);
  EXPECT_EQ(m.slots[1].offset, 8000u);
  EXPECT_EQ(m.slots[1].size, 2000u);
  EXPECT_STREQ(m.slots[1].mount_point, "/data");

  auto bytes = ReadFile(tmp / "pkg.bin");
  ASSERT_EQ(bytes.size(), 5000u + 3000u + 2000u + 2 * kSlotSize + kHeaderSize);
  EXPECT_EQ(0, std::memcmp(bytes.data(), bootstrap.data(), bootstrap.size())) << "bootstrap preserved";
  EXPECT_EQ(0, std::memcmp(bytes.data() + 5000, img1.data(), img1.size())) << "slot 0 payload preserved";
  EXPECT_EQ(0, std::memcmp(bytes.data() + 8000, img2.data(), img2.size())) << "inserted payload present";
}

TEST(PackageInsert, DefaultMount)
{
  TempDir tmp("insdef");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i1.bin", Payload(64, 2));
  WriteFile(tmp / "i2.bin", Payload(64, 3));

  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}},
                                           (tmp / "pkg.bin").string(), opts));
  ExpectOk(tebako::fs::cli::package_insert_image((tmp / "pkg.bin").string(), (tmp / "i2.bin").string(), ""));
  auto m = Probe(tmp / "pkg.bin");
  EXPECT_STREQ(m.slots[1].mount_point, "/__tebako_memfs_1__");
}

#ifndef _WIN32
TEST(PackageInsert, PermissionsPreserved)
{
  TempDir tmp("insperm");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i1.bin", Payload(64, 2));
  WriteFile(tmp / "i2.bin", Payload(64, 3));

  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}},
                                           (tmp / "pkg.bin").string(), opts));
  ASSERT_EQ(0, ::chmod((tmp / "pkg.bin").string().c_str(), 0755));
  ExpectOk(tebako::fs::cli::package_insert_image((tmp / "pkg.bin").string(), (tmp / "i2.bin").string(), ""));
  struct stat st;
  ASSERT_EQ(0, ::stat((tmp / "pkg.bin").string().c_str(), &st));
  EXPECT_EQ(st.st_mode & 0777, 0755) << "in-place rewrite must keep the executable bit";
}
#endif

TEST(PackageInsert, NonPackageFails)
{
  TempDir tmp("insnp");
  WriteFile(tmp / "plain.bin", Payload(4096, 3));
  WriteFile(tmp / "i.bin", Payload(64, 2));
  auto res = tebako::fs::cli::package_insert_image((tmp / "plain.bin").string(), (tmp / "i.bin").string(), "");
  EXPECT_FALSE(res.ok);
  EXPECT_NE(res.error.find("no tpkg manifest trailer"), std::string::npos);
}

TEST(PackageInsert, MaxSlotsFails)
{
  TempDir tmp("insmax");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i.bin", Payload(64, 2));
  std::vector<PackageImage> images(TPKG_MAX_SLOTS, PackageImage{(tmp / "i.bin").string(), ""});
  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), images, (tmp / "pkg.bin").string(), opts));

  auto res = tebako::fs::cli::package_insert_image((tmp / "pkg.bin").string(), (tmp / "i.bin").string(), "");
  EXPECT_FALSE(res.ok);
}

TEST(PackageInsert, SelfInsertFails)
{
  TempDir tmp("insself");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i1.bin", Payload(64, 2));
  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}},
                                           (tmp / "pkg.bin").string(), opts));
  auto res = tebako::fs::cli::package_insert_image((tmp / "pkg.bin").string(), (tmp / "pkg.bin").string(), "");
  EXPECT_FALSE(res.ok) << "inserting the package into itself must be rejected";
}

// ============================================================================
// remove-image
// ============================================================================

TEST(PackageRemove, MiddleSlot)
{
  TempDir tmp("rm");
  auto bootstrap = Payload(1000, 7);
  auto img1 = Payload(100, 1);
  auto img2 = Payload(200, 2);
  auto img3 = Payload(300, 3);
  WriteFile(tmp / "boot.bin", bootstrap);
  WriteFile(tmp / "i1.bin", img1);
  WriteFile(tmp / "i2.bin", img2);
  WriteFile(tmp / "i3.bin", img3);

  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle(
      (tmp / "boot.bin").string(),
      {PackageImage{(tmp / "i1.bin").string(), ""}, PackageImage{(tmp / "i2.bin").string(), "/two"},
       PackageImage{(tmp / "i3.bin").string(), "/three"}},
      (tmp / "pkg.bin").string(), opts));

  ExpectOk(tebako::fs::cli::package_remove_image((tmp / "pkg.bin").string(), 1));

  auto m = Probe(tmp / "pkg.bin");
  ASSERT_EQ(m.slot_count, 2u);
  EXPECT_EQ(m.slots[0].offset, 1000u);
  EXPECT_EQ(m.slots[0].size, 100u);
  EXPECT_EQ(m.slots[1].offset, 1100u) << "slots compacted";
  EXPECT_EQ(m.slots[1].size, 300u);
  EXPECT_STREQ(m.slots[1].mount_point, "/three") << "mount points travel with their slot";

  auto bytes = ReadFile(tmp / "pkg.bin");
  ASSERT_EQ(bytes.size(), 1000u + 100u + 300u + 2 * kSlotSize + kHeaderSize);
  EXPECT_EQ(0, std::memcmp(bytes.data() + 1000, img1.data(), img1.size()));
  EXPECT_EQ(0, std::memcmp(bytes.data() + 1100, img3.data(), img3.size()));
}

TEST(PackageRemove, LastSlotFails)
{
  TempDir tmp("rmlast");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i1.bin", Payload(64, 2));
  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle((tmp / "boot.bin").string(), {PackageImage{(tmp / "i1.bin").string(), ""}},
                                           (tmp / "pkg.bin").string(), opts));
  auto before = ReadFile(tmp / "pkg.bin");
  auto res = tebako::fs::cli::package_remove_image((tmp / "pkg.bin").string(), 0);
  EXPECT_FALSE(res.ok) << "the last slot cannot be removed";
  EXPECT_EQ(ReadFile(tmp / "pkg.bin"), before) << "failed remove must leave the package untouched";
}

TEST(PackageRemove, OutOfRangeFails)
{
  TempDir tmp("rmoor");
  WriteFile(tmp / "boot.bin", Payload(64, 1));
  WriteFile(tmp / "i1.bin", Payload(64, 2));
  WriteFile(tmp / "i2.bin", Payload(64, 3));
  PackageOptions opts;
  ExpectOk(tebako::fs::cli::package_bundle(
      (tmp / "boot.bin").string(),
      {PackageImage{(tmp / "i1.bin").string(), ""}, PackageImage{(tmp / "i2.bin").string(), ""}},
      (tmp / "pkg.bin").string(), opts));
  auto res = tebako::fs::cli::package_remove_image((tmp / "pkg.bin").string(), 5);
  EXPECT_FALSE(res.ok);
}

// ============================================================================
// set-runtime
// ============================================================================

TEST(PackageSetRuntime, SwapsBootstrap)
{
  TempDir tmp("setrt");
  auto img1 = Dwarfsish(3000);
  auto img2 = Payload(4111, 99);
  auto runtime_new = Payload(777, 21);
  WriteFile(tmp / "boot.bin", Payload(5000, 7));
  WriteFile(tmp / "i1.bin", img1);
  WriteFile(tmp / "i2.bin", img2);
  WriteFile(tmp / "rt.bin", runtime_new);

  PackageOptions opts;
  opts.runtime_ref = "ruby@3.3.7;tebako=0.15.0";
  ExpectOk(tebako::fs::cli::package_bundle(
      (tmp / "boot.bin").string(),
      {PackageImage{(tmp / "i1.bin").string(), ""}, PackageImage{(tmp / "i2.bin").string(), ""}},
      (tmp / "pkg.bin").string(), opts));

  ExpectOk(tebako::fs::cli::package_set_runtime((tmp / "pkg.bin").string(), (tmp / "rt.bin").string()));

  auto m = Probe(tmp / "pkg.bin");
  ASSERT_EQ(m.slot_count, 2u);
  EXPECT_EQ(m.slots[0].offset, 777u) << "slot offsets shifted by the new bootstrap size";
  EXPECT_EQ(m.slots[0].size, 3000u);
  EXPECT_EQ(m.slots[1].offset, 777u + 3000u);
  EXPECT_EQ(m.slots[1].size, 4111u);
  EXPECT_STREQ(m.runtime_ref, "ruby@3.3.7;tebako=0.15.0") << "trailer fields preserved";

  auto bytes = ReadFile(tmp / "pkg.bin");
  ASSERT_EQ(bytes.size(), 777u + 3000u + 4111u + 2 * kSlotSize + kHeaderSize);
  EXPECT_EQ(0, std::memcmp(bytes.data(), runtime_new.data(), runtime_new.size())) << "new runtime in place";
  EXPECT_EQ(0, std::memcmp(bytes.data() + 777, img1.data(), img1.size()));
  EXPECT_EQ(0, std::memcmp(bytes.data() + 777 + 3000, img2.data(), img2.size()));
}

TEST(PackageSetRuntime, NonPackageFails)
{
  TempDir tmp("setnp");
  WriteFile(tmp / "plain.bin", Payload(4096, 3));
  WriteFile(tmp / "rt.bin", Payload(64, 1));
  auto res = tebako::fs::cli::package_set_runtime((tmp / "plain.bin").string(), (tmp / "rt.bin").string());
  EXPECT_FALSE(res.ok);
}

// ============================================================================
// info (package_probe behavior on non-package files)
// ============================================================================

TEST(PackageInfo, PlainDwarfsImageHasNoTrailer)
{
  fsys::path fixture = "tests/fixtures/dwarfs/simple.dwarfs";
  if (!fsys::exists(fixture)) {
    GTEST_SKIP() << "fixture not generated: " << fixture;
  }
  tpkg_manifest m{};
  int terr = 0;
  EXPECT_FALSE(tebako::fs::cli::package_probe(fixture.string(), m, terr));
  EXPECT_EQ(terr, TPKG_ERR_NO_TRAILER) << "a plain image must report 'no trailer', not an error";
}

TEST(PackageInfo, MissingFileIsIoError)
{
  tpkg_manifest m{};
  int terr = 0;
  EXPECT_FALSE(tebako::fs::cli::package_probe("/nonexistent/pkg.bin", m, terr));
  EXPECT_EQ(terr, TPKG_ERR_IO);
}

// ============================================================================
// mkimage
// ============================================================================

TEST(PackageMkimage, DwarfsViaExplicitTool)
{
  if (!fsys::exists(TEBAKO_TEST_MKDWARFS)) {
    GTEST_SKIP() << "mkdwarfs not available at " << TEBAKO_TEST_MKDWARFS;
  }
  TempDir tmp("mkimg");
  fsys::create_directories(tmp / "src" / "sub");
  WriteFile(tmp / "src" / "hello.txt", {'h', 'e', 'l', 'l', 'o'});
  WriteFile(tmp / "src" / "sub" / "data.bin", Payload(128, 5));

  auto out = (tmp / "app.dwarfs").string();
  ExpectOk(tebako::fs::cli::package_mkimage("dwarfs", (tmp / "src").string(), out, TEBAKO_TEST_MKDWARFS));

  auto bytes = ReadFile(tmp / "app.dwarfs");
  ASSERT_GT(bytes.size(), 6u);
  EXPECT_EQ(0, std::memcmp(bytes.data(), "DWARFS", 6)) << "output must be a dwarfs image";
}

TEST(PackageMkimage, ZipRejected)
{
  TempDir tmp("mkzip");
  fsys::create_directories(tmp / "src");
  auto res = tebako::fs::cli::package_mkimage("zip", (tmp / "src").string(), (tmp / "out.zip").string(), "");
  EXPECT_FALSE(res.ok);
  EXPECT_NE(res.error.find("read-only"), std::string::npos);
}

TEST(PackageMkimage, BadToolFails)
{
  TempDir tmp("mkbad");
  fsys::create_directories(tmp / "src");
  auto res = tebako::fs::cli::package_mkimage("dwarfs", (tmp / "src").string(), (tmp / "out.dwarfs").string(),
                                              "/nonexistent/mkdwarfs");
  EXPECT_FALSE(res.ok);
}

TEST(PackageMkimage, MissingSourceFails)
{
  TempDir tmp("mksrc");
  auto res = tebako::fs::cli::package_mkimage("dwarfs", (tmp / "missing").string(), (tmp / "out.dwarfs").string(),
                                              TEBAKO_TEST_MKDWARFS);
  EXPECT_FALSE(res.ok);
}

// ============================================================================
// end-to-end through the built tebakofs binary
// ============================================================================

TEST(PackageCli, BundleInfoUnbundleReassemble)
{
  TempDir tmp("e2e");
  auto bootstrap = Payload(5000, 7);
  auto img1 = Dwarfsish(3000);
  auto img2 = Payload(4111, 99);
  WriteFile(tmp / "boot.bin", bootstrap);
  WriteFile(tmp / "i1.bin", img1);
  WriteFile(tmp / "i2.bin", img2);

  // bundle
  auto r = RunCli("bundle --bootstrap " + Q(tmp / "boot.bin") + " --image " + Q(tmp / "i1.bin") + " --image " +
                  Q(tmp / "i2.bin") + ":/data -o " + Q(tmp / "pkg.bin"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  auto original = ReadFile(tmp / "pkg.bin");
  ASSERT_EQ(original.size(), 5000u + 3000u + 4111u + 2 * kSlotSize + kHeaderSize);

  // info dumps the trailer
  r = RunCli("info " + Q(tmp / "pkg.bin"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  EXPECT_NE(r.output.find("three-part package"), std::string::npos);
  EXPECT_NE(r.output.find("Slots: 2"), std::string::npos);
  EXPECT_NE(r.output.find("offset=5000 size=3000"), std::string::npos);
  EXPECT_NE(r.output.find("format=dwarfs"), std::string::npos);
  EXPECT_NE(r.output.find("mount=/__tebako_memfs__"), std::string::npos);
  EXPECT_NE(r.output.find("mount=/data"), std::string::npos);
  EXPECT_NE(r.output.find("Trailer: valid"), std::string::npos);

  // unbundle + reassemble -> byte-identical
  r = RunCli("unbundle " + Q(tmp / "pkg.bin") + " -o " + Q(tmp / "parts"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  EXPECT_EQ(ReadFile(tmp / "parts" / "bootstrap.bin"), bootstrap);
  EXPECT_EQ(ReadFile(tmp / "parts" / "image-0.bin"), img1);
  EXPECT_EQ(ReadFile(tmp / "parts" / "image-1.bin"), img2);

  r = RunCli("reassemble " + Q(tmp / "parts") + " -o " + Q(tmp / "pkg2.bin"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  EXPECT_EQ(ReadFile(tmp / "pkg2.bin"), original);

  // bundle without images is a usage error
  r = RunCli("bundle --bootstrap " + Q(tmp / "boot.bin") + " -o " + Q(tmp / "pkg3.bin"));
  EXPECT_NE(r.exit_code, 0);
}

TEST(PackageCli, InfoPlainDwarfsImageStillWorks)
{
  fsys::path fixture = "tests/fixtures/dwarfs/simple.dwarfs";
  if (!fsys::exists(fixture)) {
    GTEST_SKIP() << "fixture not generated: " << fixture;
  }
  auto r = RunCli("info " + fixture.string());
  ASSERT_EQ(r.exit_code, 0) << r.output;
  EXPECT_NE(r.output.find("Type: DwarFS"), std::string::npos);
  EXPECT_NE(r.output.find("Files:"), std::string::npos) << "archive info path must be preserved";
  EXPECT_EQ(r.output.find("three-part package"), std::string::npos);
}

TEST(PackageCli, GranularOps)
{
  TempDir tmp("e2eg");
  WriteFile(tmp / "boot.bin", Payload(1000, 7));
  WriteFile(tmp / "i1.bin", Dwarfsish(100));
  WriteFile(tmp / "i2.bin", Payload(200, 2));
  WriteFile(tmp / "rt.bin", Payload(555, 21));

  auto r = RunCli("bundle --bootstrap " + Q(tmp / "boot.bin") + " --image " + Q(tmp / "i1.bin") + " -o " +
                  Q(tmp / "pkg.bin"));
  ASSERT_EQ(r.exit_code, 0) << r.output;

  r = RunCli("insert-image " + Q(tmp / "pkg.bin") + " " + Q(tmp / "i2.bin") + ":/extra");
  ASSERT_EQ(r.exit_code, 0) << r.output;
  r = RunCli("info " + Q(tmp / "pkg.bin"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  EXPECT_NE(r.output.find("Slots: 2"), std::string::npos);
  EXPECT_NE(r.output.find("mount=/extra"), std::string::npos);

  r = RunCli("remove-image " + Q(tmp / "pkg.bin") + " 0");
  ASSERT_EQ(r.exit_code, 0) << r.output;
  r = RunCli("info " + Q(tmp / "pkg.bin"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  EXPECT_NE(r.output.find("Slots: 1"), std::string::npos);
  EXPECT_NE(r.output.find("mount=/extra"), std::string::npos) << "remaining slot keeps its mount point";

  r = RunCli("set-runtime " + Q(tmp / "pkg.bin") + " " + Q(tmp / "rt.bin"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  r = RunCli("info " + Q(tmp / "pkg.bin"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  EXPECT_NE(r.output.find("Bootstrap size: 555 bytes"), std::string::npos);

  // in-place ops on a non-package fail cleanly
  WriteFile(tmp / "plain.bin", Payload(64, 1));
  r = RunCli("insert-image " + Q(tmp / "plain.bin") + " " + Q(tmp / "i2.bin"));
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.output.find("no tpkg manifest trailer"), std::string::npos);
}

TEST(PackageCli, MkimageViaEnv)
{
  if (!fsys::exists(TEBAKO_TEST_MKDWARFS)) {
    GTEST_SKIP() << "mkdwarfs not available at " << TEBAKO_TEST_MKDWARFS;
  }
  TempDir tmp("e2emk");
  fsys::create_directories(tmp / "src");
  WriteFile(tmp / "src" / "hello.txt", {'h', 'i'});

#ifdef _WIN32
  _putenv(("TEBAKO_MKDWARFS=" + std::string(TEBAKO_TEST_MKDWARFS)).c_str());
#else
  setenv("TEBAKO_MKDWARFS", TEBAKO_TEST_MKDWARFS, 1);
#endif
  auto r = RunCli("mkimage --format dwarfs " + Q(tmp / "src") + " -o " + Q(tmp / "app.dwarfs"));
  ASSERT_EQ(r.exit_code, 0) << r.output;
  auto bytes = ReadFile(tmp / "app.dwarfs");
  ASSERT_GT(bytes.size(), 6u);
  EXPECT_EQ(0, std::memcmp(bytes.data(), "DWARFS", 6));

  // zip is rejected with a clear message
  r = RunCli("mkimage --format zip " + Q(tmp / "src") + " -o " + Q(tmp / "app.zip"));
  EXPECT_NE(r.exit_code, 0);
  EXPECT_NE(r.output.find("read-only"), std::string::npos);
}
