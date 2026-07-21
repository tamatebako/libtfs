/**
 * @file test_tpkg.cpp
 * @brief Unit tests for tebako/tpkg.h — tebako package manifest (trailer) mini-lib
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

/* Declarations only — the implementation is compiled once as strict C99 in
 * test_tpkg_c99.c (TPKG_IMPLEMENTATION) and linked into this binary, exactly
 * like a downstream consumer. */
#include <tebako/tpkg.h>

/* Defined in test_tpkg_c99.c — proves the header compiles as strict C99. */
extern "C" int tpkg_c99_smoke(void);

namespace {

// ---------------------------------------------------------------------------
// Wire-format constants, restated from the spec (manifest trailer v1)
// independently of the header under test, so these tests double as an
// encoder cross-check (other languages reimplement this layout).
// ---------------------------------------------------------------------------
constexpr size_t kMagicOff = 0;
constexpr size_t kVersionOff = 10;
constexpr size_t kFlagsOff = 14;
constexpr size_t kSlotCountOff = 18;
constexpr size_t kTableOffOff = 22;
constexpr size_t kRuntimeRefOff = 30;
constexpr size_t kAbiOff = 158;
constexpr size_t kCrcOff = 162;
constexpr size_t kHeaderSize = 166;
constexpr size_t kSlotSize = 280;
constexpr char kMagic[10] = {'T', 'E', 'B', 'A', 'K', 'O', 'T', 'F', 'S', '\0'};

void PutLE32(std::vector<uint8_t>& b, size_t at, uint32_t v)
{
  b.at(at + 0) = static_cast<uint8_t>(v & 0xFFu);
  b.at(at + 1) = static_cast<uint8_t>((v >> 8) & 0xFFu);
  b.at(at + 2) = static_cast<uint8_t>((v >> 16) & 0xFFu);
  b.at(at + 3) = static_cast<uint8_t>((v >> 24) & 0xFFu);
}

void PutLE64(std::vector<uint8_t>& b, size_t at, uint64_t v)
{
  for (int i = 0; i < 8; i++) {
    b.at(at + i) = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
  }
}

uint32_t GetLE32(const uint8_t* p)
{
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t GetLE64(const uint8_t* p)
{
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) {
    v |= static_cast<uint64_t>(p[i]) << (8 * i);
  }
  return v;
}

std::vector<uint8_t> Payload(size_t n)
{
  std::vector<uint8_t> b(n);
  for (size_t i = 0; i < n; i++) {
    b[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);
  }
  return b;
}

std::vector<uint8_t> ReadFile(const std::filesystem::path& p)
{
  std::ifstream in(p, std::ios::binary);
  EXPECT_TRUE(in.good()) << "cannot open " << p;
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteFile(const std::filesystem::path& p, const std::vector<uint8_t>& bytes)
{
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.good()) << "cannot create " << p;
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(out.good()) << "cannot write " << p;
}

class TempFile {
 public:
  explicit TempFile(const char* tag)
  {
    static int counter = 0;
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    path_ = std::filesystem::temp_directory_path() /
            ("tpkg_test_" + std::string(info ? info->name() : "anon") + "_" + tag + "_" + std::to_string(counter++));
  }
  ~TempFile()
  {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }
  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

int OpenFd(const std::filesystem::path& p, bool writable)
{
#ifdef _WIN32
  return ::_open(p.string().c_str(), (writable ? _O_RDWR : _O_RDONLY) | _O_BINARY);
#else
  return ::open(p.string().c_str(), writable ? O_RDWR : O_RDONLY);
#endif
}

void CloseFd(int fd)
{
#ifdef _WIN32
  ::_close(fd);
#else
  ::close(fd);
#endif
}

tpkg_manifest MakeManifest(uint32_t slots)
{
  tpkg_manifest m{};
  m.version = TPKG_VERSION;
  m.package_flags = 0;
  m.slot_count = slots;
  m.launcher_abi = 1;
  for (uint32_t i = 0; i < slots && i < TPKG_MAX_SLOTS; i++) {
    m.slots[i].offset = 4096ull * (i + 1);
    m.slots[i].size = 2048ull * (i + 1);
    m.slots[i].format_id = TPKG_FORMAT_AUTO;
    m.slots[i].flags = 0;
    std::snprintf(m.slots[i].mount_point, TPKG_MOUNT_POINT_LEN, "/mnt/slot%u", i);
  }
  return m;
}

// Creates a stitched file: payload bytes + slot table + trailer header,
// appended via the API under test.
void CreateStitchedFile(const std::filesystem::path& p, const std::vector<uint8_t>& payload, const tpkg_manifest& m)
{
  WriteFile(p, payload);
  int fd = OpenFd(p, true);
  ASSERT_GE(fd, 0);
  EXPECT_EQ(0, tpkg_write_fd(fd, &m)) << "tpkg_write_fd failed: " << tpkg_strerror(tpkg_errno());
  CloseFd(fd);
}

// Builds a 166-byte trailer header by hand (independent LE encoder), using
// the public tpkg_crc32 (itself pinned by the known-vector test) for the crc.
std::vector<uint8_t> CraftHeader(uint32_t version, uint32_t flags, uint32_t slot_count, uint64_t table_off,
                                 const std::string& runtime_ref, uint32_t abi)
{
  std::vector<uint8_t> h(kHeaderSize, 0);
  std::memcpy(h.data() + kMagicOff, kMagic, sizeof kMagic);
  PutLE32(h, kVersionOff, version);
  PutLE32(h, kFlagsOff, flags);
  PutLE32(h, kSlotCountOff, slot_count);
  PutLE64(h, kTableOffOff, table_off);
  std::memcpy(h.data() + kRuntimeRefOff, runtime_ref.data(), std::min<size_t>(runtime_ref.size(), 128));
  PutLE32(h, kAbiOff, abi);
  PutLE32(h, kCrcOff, tpkg_crc32(h.data(), kCrcOff));
  return h;
}

void ExpectManifestEq(const tpkg_manifest& a, const tpkg_manifest& b)
{
  EXPECT_EQ(a.version, b.version);
  EXPECT_EQ(a.package_flags, b.package_flags);
  EXPECT_EQ(a.slot_count, b.slot_count);
  EXPECT_EQ(a.launcher_abi, b.launcher_abi);
  EXPECT_EQ(0, std::memcmp(a.runtime_ref, b.runtime_ref, TPKG_RUNTIME_REF_LEN));
  for (uint32_t i = 0; i < a.slot_count && i < TPKG_MAX_SLOTS; i++) {
    EXPECT_EQ(a.slots[i].offset, b.slots[i].offset) << "slot " << i;
    EXPECT_EQ(a.slots[i].size, b.slots[i].size) << "slot " << i;
    EXPECT_EQ(a.slots[i].format_id, b.slots[i].format_id) << "slot " << i;
    EXPECT_EQ(a.slots[i].flags, b.slots[i].flags) << "slot " << i;
    EXPECT_EQ(0, std::memcmp(a.slots[i].mount_point, b.slots[i].mount_point, TPKG_MOUNT_POINT_LEN)) << "slot " << i;
  }
}

}  // namespace

// ============================================================================
// tpkg_crc32
// ============================================================================

TEST(TpkgCrc32, KnownVector)
{
  /* Classic zlib CRC-32 check value */
  EXPECT_EQ(tpkg_crc32("123456789", 9), 0xCBF43926u);
  EXPECT_EQ(tpkg_crc32("", 0), 0u);
}

// ============================================================================
// tpkg_validate
// ============================================================================

TEST(TpkgValidate, ValidManifestAccepted)
{
  auto single = MakeManifest(1);
  EXPECT_EQ(0, tpkg_validate(&single)) << tpkg_strerror(tpkg_errno());
  auto full = MakeManifest(TPKG_MAX_SLOTS);
  EXPECT_EQ(0, tpkg_validate(&full)) << tpkg_strerror(tpkg_errno());
}

TEST(TpkgValidate, ZeroSlotsRejected)
{
  auto m = MakeManifest(1);
  m.slot_count = 0;
  EXPECT_EQ(-1, tpkg_validate(&m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_SLOTS);
}

TEST(TpkgValidate, TooManySlotsRejected)
{
  auto m = MakeManifest(TPKG_MAX_SLOTS);
  m.slot_count = TPKG_MAX_SLOTS + 1;
  EXPECT_EQ(-1, tpkg_validate(&m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_SLOTS);
}

TEST(TpkgValidate, MountPointNotNulTerminatedRejected)
{
  auto m = MakeManifest(1);
  std::memset(m.slots[0].mount_point, 'x', TPKG_MOUNT_POINT_LEN); /* no NUL */
  EXPECT_EQ(-1, tpkg_validate(&m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_INVALID);
}

TEST(TpkgValidate, RuntimeRefNotNulTerminatedRejected)
{
  auto m = MakeManifest(1);
  std::memset(m.runtime_ref, 'r', TPKG_RUNTIME_REF_LEN); /* no NUL */
  EXPECT_EQ(-1, tpkg_validate(&m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_INVALID);
}

TEST(TpkgValidate, OffsetSizeOverflowRejected)
{
  auto m = MakeManifest(1);
  m.slots[0].offset = UINT64_MAX;
  m.slots[0].size = 2; /* offset + size wraps */
  EXPECT_EQ(-1, tpkg_validate(&m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_INVALID);
}

TEST(TpkgValidate, BadFormatIdRejected)
{
  auto m = MakeManifest(1);
  m.slots[0].format_id = 4;
  EXPECT_EQ(-1, tpkg_validate(&m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_INVALID);
  for (uint32_t f = TPKG_FORMAT_AUTO; f <= TPKG_FORMAT_ZIP; f++) {
    m.slots[0].format_id = f;
    EXPECT_EQ(0, tpkg_validate(&m)) << "format_id " << f;
  }
}

TEST(TpkgValidate, NullArgumentRejected)
{
  auto m = MakeManifest(1);
  EXPECT_EQ(-1, tpkg_validate(nullptr));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_ARG);
  EXPECT_EQ(-1, tpkg_read_mem(nullptr, 100, &m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_ARG);
  uint8_t buf[256] = {0};
  EXPECT_EQ(-1, tpkg_read_mem(buf, sizeof buf, nullptr));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_ARG);
}

// ============================================================================
// tpkg_write_fd input validation
// ============================================================================

TEST(TpkgWriteFd, ZeroSlotsWriteRejected)
{
  TempFile tmp("w0");
  WriteFile(tmp.path(), Payload(64));
  auto m = MakeManifest(1);
  m.slot_count = 0;
  int fd = OpenFd(tmp.path(), true);
  ASSERT_GE(fd, 0);
  EXPECT_EQ(-1, tpkg_write_fd(fd, &m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_SLOTS);
  CloseFd(fd);
  EXPECT_EQ(ReadFile(tmp.path()).size(), 64u) << "rejected write must not append";
}

TEST(TpkgWriteFd, TooManySlotsWriteRejected)
{
  TempFile tmp("w9");
  WriteFile(tmp.path(), Payload(64));
  auto m = MakeManifest(TPKG_MAX_SLOTS);
  m.slot_count = TPKG_MAX_SLOTS + 1;
  int fd = OpenFd(tmp.path(), true);
  ASSERT_GE(fd, 0);
  EXPECT_EQ(-1, tpkg_write_fd(fd, &m));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_SLOTS);
  CloseFd(fd);
  EXPECT_EQ(ReadFile(tmp.path()).size(), 64u) << "rejected write must not append";
}

// ============================================================================
// Round trips: write -> read -> validate
// ============================================================================

TEST(TpkgRoundTrip, SingleSlotFd)
{
  TempFile tmp("rt1");
  auto m = MakeManifest(1);
  m.slots[0].offset = 0;
  m.slots[0].size = 4096;
  m.slots[0].format_id = TPKG_FORMAT_DWARFS;
  std::strcpy(m.slots[0].mount_point, "/__tebako_memfs__");
  CreateStitchedFile(tmp.path(), Payload(4096), m);

  int fd = OpenFd(tmp.path(), false);
  ASSERT_GE(fd, 0);
  tpkg_manifest read{};
  ASSERT_EQ(0, tpkg_read_fd(fd, &read)) << tpkg_strerror(tpkg_errno());
  EXPECT_EQ(tpkg_errno(), TPKG_OK);
  CloseFd(fd);
  EXPECT_EQ(0, tpkg_validate(&read)) << tpkg_strerror(tpkg_errno());
  ExpectManifestEq(m, read);
}

TEST(TpkgRoundTrip, SingleSlotMem)
{
  TempFile tmp("rt2");
  auto m = MakeManifest(1);
  CreateStitchedFile(tmp.path(), Payload(4096), m);

  auto bytes = ReadFile(tmp.path());
  tpkg_manifest read{};
  ASSERT_EQ(0, tpkg_read_mem(bytes.data(), bytes.size(), &read)) << tpkg_strerror(tpkg_errno());
  EXPECT_EQ(0, tpkg_validate(&read));
  ExpectManifestEq(m, read);
}

TEST(TpkgRoundTrip, ThreeSlotsWithRuntimeRef)
{
  TempFile tmp("rt3");
  tpkg_manifest m{};
  m.version = TPKG_VERSION;
  m.package_flags = 0;
  m.slot_count = 3;
  m.launcher_abi = 1;
  std::strcpy(m.runtime_ref, "ruby@3.3.7;tebako=0.15.0");

  m.slots[0].offset = 0;
  m.slots[0].size = 1000;
  m.slots[0].format_id = TPKG_FORMAT_DWARFS;
  m.slots[0].flags = 0;
  std::strcpy(m.slots[0].mount_point, "/app");

  m.slots[1].offset = 1000;
  m.slots[1].size = 2000;
  m.slots[1].format_id = TPKG_FORMAT_SQUASHFS;
  m.slots[1].flags = 1;
  std::strcpy(m.slots[1].mount_point, "/gems");

  m.slots[2].offset = 3000;
  m.slots[2].size = 512;
  m.slots[2].format_id = TPKG_FORMAT_ZIP;
  m.slots[2].flags = 0;
  std::strcpy(m.slots[2].mount_point, "/data");

  CreateStitchedFile(tmp.path(), Payload(3512), m);

  auto bytes = ReadFile(tmp.path());
  tpkg_manifest read{};
  ASSERT_EQ(0, tpkg_read_mem(bytes.data(), bytes.size(), &read)) << tpkg_strerror(tpkg_errno());
  EXPECT_EQ(0, tpkg_validate(&read));
  EXPECT_STREQ(read.runtime_ref, "ruby@3.3.7;tebako=0.15.0");
  ExpectManifestEq(m, read);
}

TEST(TpkgRoundTrip, LeanFlagPreserved)
{
  TempFile tmp("lean");
  ASSERT_EQ(TPKG_FLAG_LEAN, 1u) << "LEAN must be package_flags bit 0";
  auto m = MakeManifest(1);
  m.package_flags = TPKG_FLAG_LEAN;
  std::strcpy(m.runtime_ref, "ruby@3.3.7;tebako=0.15.0");
  CreateStitchedFile(tmp.path(), Payload(2048), m);

  auto bytes = ReadFile(tmp.path());
  tpkg_manifest read{};
  ASSERT_EQ(0, tpkg_read_mem(bytes.data(), bytes.size(), &read)) << tpkg_strerror(tpkg_errno());
  EXPECT_TRUE((read.package_flags & TPKG_FLAG_LEAN) != 0);
  ExpectManifestEq(m, read);
}

TEST(TpkgRoundTrip, MaxSlotsRoundTrip)
{
  TempFile tmp("rt8");
  auto m = MakeManifest(TPKG_MAX_SLOTS);
  CreateStitchedFile(tmp.path(), Payload(8192), m);

  auto bytes = ReadFile(tmp.path());
  ASSERT_EQ(bytes.size(), 8192u + TPKG_MAX_SLOTS * kSlotSize + kHeaderSize);
  tpkg_manifest read{};
  ASSERT_EQ(0, tpkg_read_mem(bytes.data(), bytes.size(), &read)) << tpkg_strerror(tpkg_errno());
  EXPECT_EQ(read.slot_count, TPKG_MAX_SLOTS);
  ExpectManifestEq(m, read);
}

TEST(TpkgRoundTrip, OddFileOffsets)
{
  /* trailer + table written at non-aligned file offsets must round-trip */
  for (size_t payload_size : {size_t{1}, size_t{513}, size_t{4097}}) {
    TempFile tmp("odd");
    auto m = MakeManifest(1);
    m.slots[0].offset = 13; /* odd slot geometry too */
    m.slots[0].size = 4097;
    CreateStitchedFile(tmp.path(), Payload(payload_size), m);

    auto bytes = ReadFile(tmp.path());
    ASSERT_EQ(bytes.size(), payload_size + kSlotSize + kHeaderSize);
    const uint8_t* hdr = bytes.data() + bytes.size() - kHeaderSize;
    EXPECT_EQ(GetLE64(hdr + kTableOffOff), payload_size) << "payload " << payload_size;

    tpkg_manifest read{};
    ASSERT_EQ(0, tpkg_read_mem(bytes.data(), bytes.size(), &read)) << "payload " << payload_size;
    ExpectManifestEq(m, read);

    int fd = OpenFd(tmp.path(), false);
    ASSERT_GE(fd, 0);
    tpkg_manifest read_fd{};
    ASSERT_EQ(0, tpkg_read_fd(fd, &read_fd)) << "payload " << payload_size;
    CloseFd(fd);
    ExpectManifestEq(m, read_fd);
  }
}

// ============================================================================
// Absent vs corrupt trailer
// ============================================================================

TEST(TpkgRead, AbsentTrailerMem)
{
  auto no_trailer = std::vector<uint8_t>(4096, 0xAB);
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(no_trailer.data(), no_trailer.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_NO_TRAILER);

  /* too small to hold a fixed-size header at all */
  auto tiny = std::vector<uint8_t>(100, 0xCD);
  EXPECT_EQ(-1, tpkg_read_mem(tiny.data(), tiny.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_NO_TRAILER);
}

TEST(TpkgRead, AbsentTrailerFd)
{
  TempFile tmp("absent");
  WriteFile(tmp.path(), std::vector<uint8_t>(4096, 0xAB));
  int fd = OpenFd(tmp.path(), false);
  ASSERT_GE(fd, 0);
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_fd(fd, &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_NO_TRAILER);
  CloseFd(fd);
}

TEST(TpkgRead, CorruptMagicRejected)
{
  TempFile tmp("cmagic");
  auto m = MakeManifest(1);
  CreateStitchedFile(tmp.path(), Payload(512), m);
  auto bytes = ReadFile(tmp.path());

  /* flip a magic byte past the 4-byte "TEBA" prefix: prefix matches, full
     magic does not -> corrupt, not absent */
  size_t hdr = bytes.size() - kHeaderSize;
  ASSERT_EQ(bytes[hdr + 6], 'T');
  bytes[hdr + 6] = 'X';
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(bytes.data(), bytes.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_MAGIC);
}

TEST(TpkgRead, MagicPrefixMismatchMeansAbsent)
{
  TempFile tmp("pmagic");
  auto m = MakeManifest(1);
  CreateStitchedFile(tmp.path(), Payload(512), m);
  auto bytes = ReadFile(tmp.path());

  /* break the "TEBA" prefix: indistinguishable from a binary with no trailer */
  size_t hdr = bytes.size() - kHeaderSize;
  bytes[hdr + 1] = 'Z';
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(bytes.data(), bytes.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_NO_TRAILER);
}

TEST(TpkgRead, CorruptCrcRejected)
{
  TempFile tmp("ccrc");
  auto m = MakeManifest(1);
  std::strcpy(m.runtime_ref, "ruby@3.3.7;tebako=0.15.0");
  CreateStitchedFile(tmp.path(), Payload(512), m);
  auto bytes = ReadFile(tmp.path());
  size_t hdr = bytes.size() - kHeaderSize;
  tpkg_manifest read{};

  /* flip a crc-covered payload byte (runtime_ref region) */
  auto corrupted = bytes;
  corrupted[hdr + kRuntimeRefOff + 3] ^= 0xFF;
  EXPECT_EQ(-1, tpkg_read_mem(corrupted.data(), corrupted.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_CRC);

  /* flip a byte of the stored crc field itself */
  corrupted = bytes;
  corrupted[hdr + kCrcOff] ^= 0xFF;
  EXPECT_EQ(-1, tpkg_read_mem(corrupted.data(), corrupted.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_CRC);
}

// ============================================================================
// Hand-crafted hostile headers (valid crc, bad structure)
// ============================================================================

TEST(TpkgRead, CraftedZeroSlotCountRejected)
{
  auto bytes = Payload(256);
  auto hdr = CraftHeader(TPKG_VERSION, 0, /*slot_count=*/0, 256, "", 1);
  bytes.insert(bytes.end(), hdr.begin(), hdr.end());
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(bytes.data(), bytes.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_SLOTS);
}

TEST(TpkgRead, CraftedExcessiveSlotCountRejected)
{
  auto bytes = Payload(256);
  auto hdr = CraftHeader(TPKG_VERSION, 0, /*slot_count=*/TPKG_MAX_SLOTS + 1, 256, "", 1);
  bytes.insert(bytes.end(), hdr.begin(), hdr.end());
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(bytes.data(), bytes.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_SLOTS);
}

TEST(TpkgRead, CraftedUnsupportedVersionRejected)
{
  auto bytes = Payload(256);
  auto hdr = CraftHeader(/*version=*/2, 0, 1, 256, "", 1);
  bytes.insert(bytes.end(), hdr.begin(), hdr.end());
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(bytes.data(), bytes.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_VERSION);
}

TEST(TpkgRead, TruncatedSlotTableRejected)
{
  /* header claims one slot whose table record is not actually in the file */
  auto bytes = Payload(256);
  auto hdr = CraftHeader(TPKG_VERSION, 0, 1, /*table_off=*/256, "", 1);
  bytes.insert(bytes.end(), hdr.begin(), hdr.end()); /* no 280-byte table */
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(bytes.data(), bytes.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_BOUNDS);
}

TEST(TpkgRead, TableOffsetBeyondEofRejected)
{
  auto bytes = Payload(256);
  auto hdr = CraftHeader(TPKG_VERSION, 0, 1, /*table_off=*/1u << 20, "", 1);
  bytes.insert(bytes.end(), hdr.begin(), hdr.end());
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(bytes.data(), bytes.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_BOUNDS);
}

TEST(TpkgRead, CraftedUnterminatedMountPointRejected)
{
  /* valid crc, but the slot's mount_point has no NUL in 256 bytes */
  auto bytes = Payload(256);
  std::vector<uint8_t> rec(kSlotSize, 0);
  PutLE64(rec, 0, 0);
  PutLE64(rec, 8, 128);
  PutLE32(rec, 16, TPKG_FORMAT_DWARFS);
  PutLE32(rec, 20, 0);
  std::memset(rec.data() + 24, 'x', 256);
  auto hdr = CraftHeader(TPKG_VERSION, 0, 1, 256, "", 1);
  bytes.insert(bytes.end(), rec.begin(), rec.end());
  bytes.insert(bytes.end(), hdr.begin(), hdr.end());
  tpkg_manifest read{};
  EXPECT_EQ(-1, tpkg_read_mem(bytes.data(), bytes.size(), &read));
  EXPECT_EQ(tpkg_errno(), TPKG_ERR_INVALID);
}

// ============================================================================
// Wire format (the exact layout other languages will reimplement)
// ============================================================================

TEST(TpkgWireFormat, LayoutAndSizes)
{
  EXPECT_EQ(TPKG_HEADER_SIZE, kHeaderSize);
  EXPECT_EQ(TPKG_SLOT_SIZE, kSlotSize);
  EXPECT_EQ(TPKG_MAX_SLOTS, 8u);
  EXPECT_EQ(TPKG_VERSION, 1u);
  EXPECT_EQ(TPKG_MOUNT_POINT_LEN, 256u);
  EXPECT_EQ(TPKG_RUNTIME_REF_LEN, 128u);
  EXPECT_EQ(TPKG_FORMAT_AUTO, 0u);
  EXPECT_EQ(TPKG_FORMAT_DWARFS, 1u);
  EXPECT_EQ(TPKG_FORMAT_SQUASHFS, 2u);
  EXPECT_EQ(TPKG_FORMAT_ZIP, 3u);

  TempFile tmp("wire");
  tpkg_manifest m{};
  m.version = TPKG_VERSION;
  m.package_flags = TPKG_FLAG_LEAN;
  m.slot_count = 1;
  m.launcher_abi = 1;
  std::strcpy(m.runtime_ref, "ruby@3.3.7;tebako=0.15.0");
  m.slots[0].offset = 7;
  m.slots[0].size = 999;
  m.slots[0].format_id = TPKG_FORMAT_ZIP;
  m.slots[0].flags = 5;
  std::strcpy(m.slots[0].mount_point, "/app");

  CreateStitchedFile(tmp.path(), Payload(111), m);
  auto bytes = ReadFile(tmp.path());
  ASSERT_EQ(bytes.size(), 111u + kSlotSize + kHeaderSize);

  /* slot record at byte 111 */
  const uint8_t* rec = bytes.data() + 111;
  EXPECT_EQ(GetLE64(rec + 0), 7u);
  EXPECT_EQ(GetLE64(rec + 8), 999u);
  EXPECT_EQ(GetLE32(rec + 16), TPKG_FORMAT_ZIP);
  EXPECT_EQ(GetLE32(rec + 20), 5u);
  EXPECT_EQ(0, std::memcmp(rec + 24, "/app", 5));
  EXPECT_EQ(rec[24 + 255], 0) << "mount_point must be NUL-padded";

  /* header at EOF-166 */
  const uint8_t* hdr = bytes.data() + bytes.size() - kHeaderSize;
  EXPECT_EQ(0, std::memcmp(hdr + kMagicOff, kMagic, 10));
  EXPECT_EQ(GetLE32(hdr + kVersionOff), 1u);
  EXPECT_EQ(GetLE32(hdr + kFlagsOff), TPKG_FLAG_LEAN);
  EXPECT_EQ(GetLE32(hdr + kSlotCountOff), 1u);
  EXPECT_EQ(GetLE64(hdr + kTableOffOff), 111u);
  EXPECT_EQ(0, std::memcmp(hdr + kRuntimeRefOff, "ruby@3.3.7;tebako=0.15.0", 24));
  EXPECT_EQ(hdr[kRuntimeRefOff + 127], 0) << "runtime_ref must be NUL-padded";
  EXPECT_EQ(GetLE32(hdr + kAbiOff), 1u);
  EXPECT_EQ(GetLE32(hdr + kCrcOff), tpkg_crc32(hdr, kCrcOff)) << "crc32 covers header bytes [0,162)";
}

// ============================================================================
// strerror / C99 conformance
// ============================================================================

TEST(TpkgStrError, AllCodesHaveStrings)
{
  for (int code : {TPKG_OK, TPKG_ERR_NO_TRAILER, TPKG_ERR_MAGIC, TPKG_ERR_CRC, TPKG_ERR_IO, TPKG_ERR_BOUNDS,
                   TPKG_ERR_SLOTS, TPKG_ERR_INVALID, TPKG_ERR_ARG, TPKG_ERR_VERSION}) {
    const char* s = tpkg_strerror(code);
    ASSERT_NE(s, nullptr) << "code " << code;
    EXPECT_GT(std::strlen(s), 0u) << "code " << code;
  }
  EXPECT_NE(tpkg_strerror(12345), nullptr) << "unknown codes need a fallback string";
}

TEST(TpkgC99, HeaderCompilesAsC99)
{
  EXPECT_EQ(tpkg_c99_smoke(), 0);
}
