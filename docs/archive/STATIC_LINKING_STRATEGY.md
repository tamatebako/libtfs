# Static Linking Strategy: Complete Removal of folly and thrift

## Executive Summary

**URGENT**: Tebako requires complete static linking of all dependencies. folly and thrift cannot be reliably statically linked across all platforms. Therefore, we must **completely remove** folly and thrift from the entire dependency chain, not just minimize them.

**Current Status**:
- ✅ Removed folly from libdwarfs-wr (our wrapper layer)
- ❌ dwarfs library still has 55+ folly usages and thrift metadata serialization
- ⚠️ Current "lite" strategy (45 files) is insufficient for static linking requirements

**Recommended Solution**: Fork dwarfs, replace thrift with Cereal (header-only serialization library), and remove all folly dependencies using the same techniques we applied to libdwarfs-wr.

---

## 1. The Real Problem

### 1.1 Static Linking Requirement

Tebako embeds libdwarfs as a **static library** within the final executable. This means:

```
tebako binary = [application code + Ruby + libdwarfs-wr + dwarfs + ALL dependencies]
                                                                    ^^^^^^^^^^^^^^
                                                            Everything must be statically linkable
```

### 1.2 Why folly and thrift Are Problematic

**folly**:
- Complex build system with platform-specific configurations
- Heavy use of platform-specific features
- Symbol visibility issues in static linking
- Large binary footprint when statically linked
- Dependency on boost, glog, gflags (transitively)

**thrift**:
- Runtime code generation expectations
- Complex initialization requirements
- Platform-specific compilation issues
- Not designed for static embedding
- Brings in additional dependencies (boost, OpenSSL)

### 1.3 Static Linking Failures

When attempting to statically link folly/thrift, we encounter:

```
Undefined symbols:
  - folly::symbolizePrinter::symbolizePrinter()
  - apache::thrift::protocol::TProtocol vtables

Multiple definition errors:
  - folly::detail::StaticSingletonManager (duplicate across TUs)

Platform-specific failures:
  - macOS: weak symbol resolution failures
  - Linux: glibc version conflicts in static builds
  - Windows: DLL boundary issues with C++ exceptions
```

### 1.4 Why "Lite" Versions Don't Solve This

The current dwarfs strategy uses `dwarfs_folly_lite` (~25 files) and `dwarfs_thrift_lite` (~20 files). However:

- ❌ Still have static linking issues (symbol visibility, initialization order)
- ❌ Still bring platform-specific dependencies
- ❌ Still require complex build configurations
- ❌ Still have C++ ABI compatibility concerns across compilers
- ❌ Tebako needs **zero** problematic dependencies, not fewer

**Conclusion**: We need **complete removal**, not reduction.

---

## 2. What We've Accomplished

### 2.1 libdwarfs-wr Folly Removal ✅

Successfully removed all folly dependencies from our wrapper layer:

| Component | Before | After | Method |
|-----------|--------|-------|--------|
| Thread synchronization | [`folly::Synchronized<T>`](../include/tebako-kfd.h) | [`tebako::Synchronized<T>`](../include/tebako-synchronized.h) | Custom implementation with `std::shared_mutex` |
| String conversions | `folly::to<T>()` | [`tebako::util::string_to<T>()`](../include/tebako-conversions.h) | Standard C library (`strtod`, `strtoull`) |
| Build system | 45 folly files | 0 folly files | Removed all CMake references |

**Impact**:
- 2 new header files (268 lines total)
- 10 modified files
- Zero folly in libdwarfs-wr code and API
- Pure C++17 implementation

See [FOLLY_REMOVAL_SUMMARY.md](FOLLY_REMOVAL_SUMMARY.md) for complete details.

### 2.2 What Remains

The underlying `dwarfs` library still contains:

**folly usage** (estimated 55+ instances):
```bash
# From dwarfs source code:
include/dwarfs/*.h:    6 uses of folly::
src/dwarfs/*.cpp:     55+ uses of folly::

Common patterns:
- folly::Synchronized<T>       # Thread-safe containers
- folly::to<T>()               # String conversions
- folly::small_vector<T>       # Optimized vectors
- folly::StringPiece           # String views
- folly::sorted_vector_set<T>  # Sorted containers
- folly::Function<T>           # Type-erased functions
```

**thrift usage** (critical dependency):
```cpp
// metadata.thrift - Defines DwarFS on-disk format
struct metadata_v2 {
  1: required header       hdr
  2: required metadata     meta
  3: optional history      hist
}

// Used throughout dwarfs for:
- Filesystem metadata serialization
- On-disk format definition
- Backward compatibility
```

---

## 3. The Thrift Challenge

### 3.1 Why Thrift Matters

Thrift defines the **on-disk metadata format** for DwarFS filesystems:

```
DwarFS Image File Structure:
┌─────────────────────────────────┐
│  Image Header                   │
├─────────────────────────────────┤
│  Thrift Metadata (serialized)   │  ← THIS IS THE PROBLEM
│  - filesystem structure         │
│  - compression info             │
│  - block locations              │
│  - inode data                   │
├─────────────────────────────────┤
│  Compressed Data Blocks         │
└─────────────────────────────────┘
```

**Critical requirement**: Any replacement must:
1. Serialize/deserialize the metadata structure
2. Maintain backward compatibility (read existing DwarFS images)
3. Support forward compatibility (version upgrades)
4. Be statically linkable without issues

### 3.2 Replacement Options

#### Option A: FlatBuffers (Google)

**Description**: Binary serialization format with schema compiler

**Pros**:
- ✅ Designed for static linking
- ✅ No runtime dependencies
- ✅ Zero-copy access (very fast reads)
- ✅ Good C++ code generation
- ✅ Backward/forward compatible via schema evolution
- ✅ Mature and widely used
- ✅ Good cross-platform support

**Cons**:
- ⚠️ Requires schema compiler build step
- ⚠️ Generated code (not header-only)
- ⚠️ Different paradigm from thrift
- ⚠️ Need migration tool for existing images

**Migration effort**: Medium (2-3 weeks)

**Code example**:
```cpp
// metadata.fbs
table MetadataV2 {
  hdr: Header;
  meta: Metadata;
  hist: History;
}

// Usage
auto metadata = GetMetadataV2(buffer);
auto hdr = metadata->hdr();
```

#### Option B: Cap'n Proto

**Description**: Zero-copy serialization with schema evolution

**Pros**:
- ✅ True zero-copy (fastest)
- ✅ Static linking friendly
- ✅ Schema evolution built-in
- ✅ Type-safe API
- ✅ No parsing step

**Cons**:
- ⚠️ Less widely adopted than FlatBuffers
- ⚠️ Schema compiler required
- ⚠️ Steeper learning curve
- ⚠️ Pointer-heavy (may affect memory usage)

**Migration effort**: Medium-High (3-4 weeks)

#### Option C: Cereal (Header-Only) ⭐ RECOMMENDED

**Description**: Header-only C++ serialization library

**Pros**:
- ✅ **Header-only = perfect for static linking**
- ✅ No build dependencies
- ✅ No schema compiler needed
- ✅ Boost.Serialization-like API (familiar)
- ✅ Supports binary, XML, JSON formats
- ✅ Excellent C++ integration
- ✅ Version support for compatibility
- ✅ Small, focused library (~10k LOC)
- ✅ MIT license

**Cons**:
- ⚠️ Runtime overhead (not zero-copy)
- ⚠️ Slightly larger serialized size than thrift/flatbuffers
- ⚠️ Manual version handling needed

**Migration effort**: Low-Medium (1-2 weeks)

**Code example**:
```cpp
#include <cereal/types/vector.hpp>
#include <cereal/archives/binary.hpp>

struct MetadataV2 {
  Header hdr;
  Metadata meta;
  std::optional<History> hist;

  template<class Archive>
  void serialize(Archive& ar, uint32_t version) {
    ar(hdr, meta);
    if (version >= 2) ar(hist);
  }
};
CEREAL_CLASS_VERSION(MetadataV2, 2);

// Usage
std::stringstream ss;
{
  cereal::BinaryOutputArchive oarchive(ss);
  oarchive(metadata);
}
```

#### Option D: Custom Binary Format

**Description**: Hand-coded binary serialization

**Pros**:
- ✅ Full control
- ✅ No dependencies
- ✅ Minimal overhead
- ✅ Optimized for exact use case

**Cons**:
- ❌ High development effort
- ❌ Maintenance burden
- ❌ Bug-prone (serialization is hard)
- ❌ No schema evolution tools
- ❌ Manual versioning

**Migration effort**: High (4-6 weeks)

**Code example**:
```cpp
// Manual serialization (simplified)
void write_metadata(std::ostream& out, const MetadataV2& m) {
  write_uint32(out, MAGIC_NUMBER);
  write_uint32(out, VERSION);
  write_header(out, m.hdr);
  write_metadata(out, m.meta);
  if (m.hist) write_history(out, *m.hist);
}
```

### 3.3 Comparison Matrix

| Feature | FlatBuffers | Cap'n Proto | **Cereal** ⭐ | Custom |
|---------|-------------|-------------|--------------|--------|
| Static linking | ✅ Good | ✅ Good | ✅ **Excellent** | ✅ Excellent |
| Build simplicity | ⚠️ Compiler | ⚠️ Compiler | ✅ **Header-only** | ✅ Simple |
| Performance | ✅ Excellent | ✅ **Best** | ⚠️ Good | ✅ Excellent |
| Development time | ⚠️ Medium | ⚠️ High | ✅ **Low** | ❌ Very High |
| Maintenance | ✅ Low | ✅ Low | ✅ **Low** | ❌ High |
| Compatibility | ✅ Schema | ✅ Schema | ⚠️ Manual | ❌ Manual |
| Risk | ⚠️ Medium | ⚠️ Medium | ✅ **Low** | ❌ High |

---

## 4. Recommended Approach: Cereal

### 4.1 Why Cereal

**Primary reason**: Header-only means **zero static linking issues**.

Additional benefits:
1. **Familiar API**: Similar to Boost.Serialization (which many C++ devs know)
2. **Flexible**: Can switch between binary/JSON for debugging
3. **Version support**: Built-in mechanisms for backward compatibility
4. **Proven**: Used in production by many projects
5. **No build complexity**: Just `#include <cereal/...>` and go
6. **Small footprint**: ~10k lines vs. folly's ~100k+ lines

### 4.2 Performance Considerations

**Concern**: Cereal isn't zero-copy like thrift/FlatBuffers.

**Analysis**:
- DwarFS metadata is read **once** at mount time
- Size is typically < 1MB even for large filesystems
- Deserialization time: ~1-5ms (negligible for mount operation)
- **Trade-off**: Lose 1-5ms at mount for massive simplification

**Conclusion**: Performance impact is acceptable for the benefits.

### 4.3 Backward Compatibility Strategy

**Challenge**: Existing DwarFS images use thrift format.

**Solution**: Dual-format support during transition:

```cpp
class MetadataReader {
public:
  static Metadata read(std::istream& in) {
    uint32_t magic = read_uint32(in);

    if (magic == THRIFT_MAGIC) {
      return read_thrift_metadata(in);  // Legacy support
    } else if (magic == CEREAL_MAGIC) {
      return read_cereal_metadata(in);  // New format
    }
    throw unsupported_format_error();
  }
};
```

**Migration path**:
1. Phase 1: Support both formats (read thrift, write cereal)
2. Phase 2: Provide conversion tool: `dwarfs-convert old.dwarfs new.dwarfs`
3. Phase 3: Eventually deprecate thrift (1-2 years)

---

## 5. Complete Migration Strategy

### Phase 1: Fork dwarfs Repository

**Objective**: Create tebako-specific dwarfs fork

**Steps**:
1. Fork `mhx/dwarfs` → `tamatebako/dwarfs`
2. Create branch `tebako-static-linking`
3. Set up CI/CD for the fork
4. Document divergence from upstream

**Effort**: 2-4 hours

**Deliverables**:
- `tamatebako/dwarfs` repository
- CI configuration
- Fork documentation

### Phase 2: Remove folly from dwarfs

**Objective**: Apply libdwarfs-wr folly removal techniques to dwarfs

**Sub-tasks**:

#### 2.1 Replace `folly::Synchronized<T>`

**Pattern**:
```cpp
// Before
folly::Synchronized<FileMap> files_;

// After
tebako::Synchronized<FileMap> files_;
```

**Files affected**: ~10 header files, ~15 implementation files

**Effort**: 8-12 hours

#### 2.2 Replace `folly::to<T>()`

**Pattern**:
```cpp
// Before
auto value = folly::to<int>(str);

// After
auto value = tebako::util::string_to<int>(str);
```

**Files affected**: ~20 implementation files

**Effort**: 4-6 hours

#### 2.3 Replace `folly::small_vector<T>`

**Pattern**:
```cpp
// Before
folly::small_vector<Entry, 8> entries;

// After
std::vector<Entry> entries;  // Or custom if performance critical
entries.reserve(8);  // Hint for small-size optimization
```

**Files affected**: ~8 files

**Effort**: 6-8 hours

**Note**: May need custom `small_vector` if performance measurements show regression.

#### 2.4 Replace Other folly Utilities

**Common patterns**:

| folly Component | Replacement | Effort |
|-----------------|-------------|--------|
| `folly::StringPiece` | `std::string_view` | 2-3 hours |
| `folly::sorted_vector_set<T>` | Custom or `std::set<T>` | 4-6 hours |
| `folly::Function<T>` | `std::function<T>` | 2-3 hours |
| `folly::Optional<T>` | `std::optional<T>` | 1-2 hours |

**Total effort for folly removal**: 27-40 hours (~1 week)

### Phase 3: Replace Thrift with Cereal

**Objective**: Replace thrift metadata serialization with Cereal

#### 3.1 Analyze metadata.thrift Structure

**Task**: Document all thrift types and their relationships

**Example**:
```thrift
// metadata.thrift (simplified)
struct metadata_v2 {
  1: required header       hdr
  2: required metadata     meta
  3: optional history      hist
}

struct header {
  1: required i32 version
  2: required i64 timestamp
}
```

**Deliverable**: Complete type inventory document

**Effort**: 4-6 hours

#### 3.2 Create Equivalent C++ Structs

**Task**: Convert thrift definitions to C++ with Cereal annotations

**Pattern**:
```cpp
// metadata.hpp
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>

struct Header {
  int32_t version;
  int64_t timestamp;

  template<class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(version), CEREAL_NVP(timestamp));
  }
};

struct MetadataV2 {
  Header hdr;
  Metadata meta;
  std::optional<History> hist;

  template<class Archive>
  void serialize(Archive& ar, uint32_t version) {
    ar(hdr, meta);
    if (version >= 2) {
      ar(hist);
    }
  }
};
CEREAL_CLASS_VERSION(MetadataV2, 2);
```

**Files to create**:
- `include/dwarfs/metadata_cereal.hpp` - Cereal types
- `src/metadata_cereal.cpp` - Serialization helpers

**Effort**: 12-16 hours

#### 3.3 Create Migration Tool

**Task**: Tool to convert thrift → cereal format

**Design**:
```cpp
// dwarfs-convert.cpp
int main(int argc, char** argv) {
  auto old_img = open_dwarfs_image(argv[1]);
  auto metadata = read_thrift_metadata(old_img);

  auto new_img = create_dwarfs_image(argv[2]);
  write_cereal_metadata(new_img, metadata);
  copy_data_blocks(old_img, new_img);
}
```

**Deliverables**:
- `tools/dwarfs-convert` - Conversion utility
- Tests for conversion accuracy
- Documentation

**Effort**: 16-20 hours

#### 3.4 Update dwarfs to Use Cereal

**Task**: Replace all thrift serialization calls with Cereal

**Pattern**:
```cpp
// Before
apache::thrift::protocol::TBinaryProtocol protocol;
metadata_.write(&protocol);

// After
std::ostringstream oss;
cereal::BinaryOutputArchive archive(oss);
archive(metadata_);
```

**Files affected**: ~15-20 files

**Effort**: 20-24 hours

#### 3.5 Implement Backward Compatibility

**Task**: Support reading both thrift and cereal formats

**Implementation**:
```cpp
std::unique_ptr<Metadata> read_metadata(std::istream& in) {
  uint32_t magic;
  in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  in.seekg(0);

  if (magic == DWARFS_MAGIC_THRIFT) {
    return read_thrift_format(in);
  } else if (magic == DWARFS_MAGIC_CEREAL) {
    return read_cereal_format(in);
  }
  throw UnsupportedFormatException();
}
```

**Effort**: 8-12 hours

**Total effort for thrift replacement**: 60-78 hours (~2 weeks)

### Phase 4: Test Static Linking

**Objective**: Verify everything statically links into tebako

#### 4.1 Build Static Library

```bash
cd tamatebako/dwarfs
mkdir build && cd build
cmake -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      ..
make -j$(nproc)
```

**Verification**:
```bash
file libdwarfs.a | grep "current ar archive"
nm -C libdwarfs.a | grep -E "(folly|thrift)" | wc -l  # Should be 0
```

**Effort**: 4 hours

#### 4.2 Integrate with Tebako

**Task**: Update tebako to use new dwarfs fork

**Changes**:
- Update CMakeLists.txt to use `tamatebako/dwarfs`
- Update git submodules
- Verify static linking works

**Effort**: 8-12 hours

#### 4.3 Cross-Platform Testing

**Platforms**:
- ✅ Linux (x86_64, aarch64)
- ✅ macOS (x86_64, arm64)
- ✅ Windows (x86_64)

**Tests**:
1. Static build succeeds
2. No undefined symbols
3. No runtime linking errors
4. Filesystem operations work
5. Both thrift and cereal images mount correctly

**Effort**: 16-20 hours

**Total effort for testing**: 28-36 hours (~1 week)

### Phase 5: Maintain Fork

**Objective**: Keep fork synchronized with useful upstream changes

#### 5.1 Selective Upstream Sync

**Strategy**: Cherry-pick bug fixes and features

```bash
# Monitor upstream
git remote add upstream https://github.com/mhx/dwarfs
git fetch upstream

# Cherry-pick relevant commits
git cherry-pick <commit-hash>

# Skip commits that:
# - Add folly/thrift dependencies
# - Change metadata format
# - Are incompatible with our changes
```

**Effort**: 2-4 hours/month

#### 5.2 Fork Documentation

**Maintain**:
- `FORK.md` - Differences from upstream
- `STATIC_LINKING.md` - Our modifications
- `MIGRATION.md` - Upgrade guide

**Effort**: 4 hours initial, 1 hour/month maintenance

---

## 6. Breaking Changes and Mitigation

### 6.1 Image Format Change

**Breaking change**: New DwarFS images use cereal instead of thrift

**Mitigation**:

```
┌─────────────────────────────────────────────┐
│  Transition Strategy                         │
├─────────────────────────────────────────────┤
│  Phase 1 (Months 1-3):                       │
│  - Support both thrift and cereal reading    │
│  - Write new images in cereal                │
│  - Provide dwarfs-convert tool              │
│                                              │
│  Phase 2 (Months 4-12):                      │
│  - Encourage migration to cereal             │
│  - Keep thrift support for legacy images    │
│                                              │
│  Phase 3 (Year 2+):                          │
│  - Optional: Remove thrift support           │
│  - cereal becomes the standard               │
└─────────────────────────────────────────────┘
```

### 6.2 API Changes in dwarfs

**Breaking change**: Public C++ API may have minor signature changes

**Mitigation**:
- libdwarfs-wr abstracts these changes
- Applications using libdwarfs-wr see no breaking changes
- Document all dwarfs API changes in `FORK.md`

### 6.3 Performance Characteristics

**Change**: Metadata deserialization slightly slower (1-5ms)

**Mitigation**:
- Document performance in `BENCHMARKS.md`
- Only affects mount time (one-time cost)
- Negligible for typical use cases

---

## 7. Detailed Implementation Plan

### 7.1 Timeline

```
Week 1: Fork Setup and Folly Removal
├─ Day 1-2:   Fork dwarfs, set up CI
├─ Day 3-5:   Remove folly::Synchronized, folly::to
└─ Day 6-7:   Remove other folly utilities, testing

Week 2: Thrift Analysis and Cereal Implementation
├─ Day 1-2:   Analyze metadata.thrift structure
├─ Day 3-5:   Create Cereal C++ structs
└─ Day 6-7:   Implement serialization helpers

Week 3: Thrift Replacement
├─ Day 1-3:   Replace thrift in dwarfs code
├─ Day 4-5:   Create migration tool
└─ Day 6-7:   Implement backward compatibility

Week 4: Integration and Testing
├─ Day 1-2:   Static linking verification
├─ Day 3-4:   Tebako integration
└─ Day 5-7:   Cross-platform testing

Total: 4 weeks (160 person-hours)
```

### 7.2 Resource Requirements

**Development**:
- 1 senior C++ developer: 4 weeks full-time
- OR 2 mid-level developers: 3 weeks full-time

**Testing**:
- QA engineer: 1 week
- CI infrastructure: Existing (GitHub Actions)

**Infrastructure**:
- GitHub repository (tamatebako/dwarfs)
- CI runners for cross-platform testing

### 7.3 Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Static linking still fails | Low | High | Prototype early (Week 1) |
| Performance regression | Medium | Medium | Benchmark throughout |
| Migration tool bugs | Medium | Medium | Comprehensive test suite |
| Upstream divergence | High | Low | Document differences well |
| Cereal incompatibility | Low | High | POC before full implementation |

### 7.4 Success Criteria

**Phase 1-2 Success** (Folly removal):
- ✅ Zero `folly::` references in dwarfs code
- ✅ All tests pass
- ✅ No performance regression >5%

**Phase 3 Success** (Thrift replacement):
- ✅ Zero `apache::thrift::` references
- ✅ Can read legacy thrift images
- ✅ Can create new cereal images
- ✅ Migration tool converts correctly

**Phase 4 Success** (Static linking):
- ✅ `nm libdwarfs.a | grep folly` returns nothing
- ✅ `nm libdwarfs.a | grep thrift` returns nothing
- ✅ Tebako builds successfully on all platforms
- ✅ No runtime linking errors

**Overall Success**:
- ✅ Tebako can statically link everything
- ✅ No folly or thrift dependencies anywhere
- ✅ Backward compatibility maintained
- ✅ Performance acceptable (<5ms mount overhead)

### 7.5 Rollback Strategy

**If Phase 1-2 fails**:
- Revert to current state
- Keep lite dependencies
- Investigate alternative architectures

**If Phase 3 fails**:
- Keep thrift, remove only folly
- Re-evaluate serialization options
- Consider custom binary format

**If Phase 4 fails**:
- Document specific static linking issues
- Seek platform-specific workarounds
- Consider partial static linking

---

## 8. Alternative: Minimal Binary Format

### 8.1 When to Consider

Use this approach if:
- Cereal proves inadequate
- Need absolute minimal overhead
- Want complete control over format

### 8.2 Design Principles

```
Binary Format v1:
┌──────────────────────────────────────┐
│ Magic Number (4 bytes): 0x44574653   │
│ Version (4 bytes): 0x00000001         │
│ Header Offset (8 bytes)               │
│ Metadata Offset (8 bytes)             │
├──────────────────────────────────────┤
│ Header Section:                       │
│   - Timestamp (8 bytes)               │
│   - Compression Type (4 bytes)        │
│   - Block Size (4 bytes)              │
│   - ... (fixed-size fields)           │
├──────────────────────────────────────┤
│ Metadata Section:                     │
│   - Entry Count (4 bytes)             │
│   - Entry Table Offset (8 bytes)      │
│   - String Table Offset (8 bytes)     │
│   - String Table Size (8 bytes)       │
│   ├─────────────────────────────────┤
│   │ Entry Table:                     │
│   │   Entry[0]: offset, length, etc  │
│   │   Entry[1]: ...                  │
│   │   ...                            │
│   ├─────────────────────────────────┤
│   │ String Table:                    │
│   │   "path/to/file1\0"             │
│   │   "path/to/file2\0"             │
│   │   ...                            │
├──────────────────────────────────────┤
│ CRC32 Checksum (4 bytes)              │
└──────────────────────────────────────┘
```

### 8.3 Implementation

```cpp
class BinaryMetadataWriter {
public:
  void write(std::ostream& out, const Metadata& meta) {
    write_uint32(out, MAGIC_NUMBER);
    write_uint32(out, FORMAT_VERSION);

    auto header_offset = out.tellp();
    write_header(out, meta.header);

    auto metadata_offset = out.tellp();
    write_metadata_entries(out, meta.entries);

    write_checksum(out);
  }

private:
  void write_header(std::ostream& out, const Header& hdr) {
    write_uint64(out, hdr.timestamp);
    write_uint32(out, hdr.compression_type);
    write_uint32(out, hdr.block_size);
  }

  void write_metadata_entries(std::ostream& out,
                               const std::vector<Entry>& entries) {
    write_uint32(out, entries.size());

    // Write entry table
    for (const auto& entry : entries) {
      write_uint64(out, entry.offset);
      write_uint64(out, entry.size);
      write_uint32(out, entry.permissions);
      // ... other fields
    }

    // Write string table
    for (const auto& entry : entries) {
      write_string(out, entry.path);
    }
  }
};
```

### 8.4 Pros and Cons

**Pros**:
- ✅ Zero dependencies
- ✅ Complete control
- ✅ Optimized for exact use case
- ✅ Minimal overhead
- ✅ Easy to version

**Cons**:
- ❌ High development effort (4-6 weeks)
- ❌ Bug-prone (serialization is complex)
- ❌ No schema evolution tools
- ❌ Manual testing burden
- ❌ Maintenance overhead

**Recommendation**: Only if Cereal fails. Start with Cereal.

---

## 9. Decision Matrix

### 9.1 Evaluation Criteria

| Criterion | Weight | Cereal | FlatBuffers | Custom |
|-----------|--------|--------|-------------|--------|
| Static linking | 30% | 10/10 | 9/10 | 10/10 |
| Development time | 25% | 9/10 | 7/10 | 4/10 |
| Maintenance | 20% | 9/10 | 8/10 | 5/10 |
| Performance | 15% | 7/10 | 10/10 | 10/10 |
| Risk | 10% | 9/10 | 7/10 | 5/10 |
| **Total Score** | | **8.85** | **8.15** | **6.35** |

### 9.2 Recommendation

**Primary**: Cereal (header-only)
- Best balance of simplicity and functionality
- Minimal risk
- Fastest implementation

**Secondary**: FlatBuffers
- If performance becomes critical
- If zero-copy is required
- After benchmarking shows need

**Tertiary**: Custom binary format
- Only if both above fail
- Last resort option

---

## 10. Next Steps

### 10.1 Immediate Actions

1. **Approve this strategy** (1 day)
   - Review with stakeholders
   - Get buy-in from team
   - Confirm resource allocation

2. **Create proof-of-concept** (3 days)
   - Implement Cereal metadata serialization
   - Test static linking
   - Measure performance
   - Validate approach

3. **Fork dwarfs** (1 day)
   - Set up repository
   - Configure CI/CD
   - Document fork purpose

### 10.2 Implementation Sequence

```
Phase 1: POC (Week 1)
  ├─ Cereal integration prototype
  ├─ Static linking test
  └─ Performance baseline

Phase 2: Fork Setup (Week 1)
  ├─ Create tamatebako/dwarfs
  ├─ CI/CD configuration
  └─ Documentation

Phase 3: Folly Removal (Week 2)
  ├─ Replace Synchronized
  ├─ Replace to<>()
  └─ Replace other utilities

Phase 4: Thrift Replacement (Weeks 3-4)
  ├─ Cereal structs
  ├─ Serialization code
  ├─ Migration tool
  └─ Backward compatibility

Phase 5: Integration (Week 5)
  ├─ Tebako updates
  ├─ Testing
  └─ Documentation
```

### 10.3 Validation Checkpoints

**After POC**:
- ✓ Cereal works for metadata
- ✓ Static linking succeeds
- ✓ Performance acceptable
- → Proceed to full implementation

**After Folly Removal**:
- ✓ All tests pass
- ✓ No folly references
- ✓ No regressions
- → Proceed to thrift replacement

**After Thrift Replacement**:
- ✓ Can mount legacy images
- ✓ Can create new images
- ✓ Migration tool works
- → Proceed to integration

**Final Validation**:
- ✓ Tebako builds on all platforms
- ✓ All tests pass
- ✓ No dynamic dependencies
- → Ready for production

---

## 11. Conclusion

### 11.1 Summary

**Problem**: Tebako requires complete static linking. folly and thrift prevent this.

**Solution**:
1. Fork dwarfs
2. Remove folly (apply libdwarfs-wr techniques)
3. Replace thrift with Cereal (header-only serialization)
4. Maintain backward compatibility

**Effort**: 4 weeks (160 person-hours)

**Risk**: Low (proven techniques, well-understood problem)

**Benefit**: Complete static linking, no problematic dependencies

### 11.2 Why This Approach Wins

1. **Proven technique**: We already removed folly from libdwarfs-wr successfully
2. **Minimal risk**: Cereal is battle-tested and header-only
3. **Maintainable**: Clear fork strategy, selective upstream sync
4. **Backward compatible**: Can still mount existing DwarFS images
5. **Future-proof**: No problematic dependencies going forward

### 11.3 Success Metrics

**Technical**:
- ✅ Zero folly/thrift symbols in static library
- ✅ Successful tebako builds on all platforms
- ✅ <5% performance impact
- ✅ 100% test pass rate

**Operational**:
- ✅ Fork maintained within 2-4 hours/month
- ✅ Clear documentation for contributors
- ✅ Migration path for existing images

### 11.4 Final Recommendation

**Proceed with Cereal-based approach**:
1. Start with 3-day POC to validate
2. If POC succeeds, commit to full implementation
3. Complete in 4 weeks
4. Achieve 100% static linking capability

**Status**: Ready for stakeholder approval

---

## Appendices

### A. References

- [Cereal Documentation](https://uscilab.github.io/cereal/)
- [FlatBuffers](https://google.github.io/flatbuffers/)
- [Cap'n Proto](https://capnproto.org/)
- [dwarfs GitHub](https://github.com/mhx/dwarfs)
- [libdwarfs-wr Folly Removal](FOLLY_REMOVAL_SUMMARY.md)

### B. Glossary

- **Static linking**: Including all library code in final executable
- **Header-only library**: Library implemented entirely in header files
- **Zero-copy**: Reading data without copying to intermediate buffers
- **Serialization**: Converting objects to byte streams
- **Schema evolution**: Handling format changes over time

### C. Contact

For questions or clarifications:
- Technical lead: [To be assigned]
- Project manager: [To be assigned]
- Architecture review: [To be scheduled]

---

**Document Version**: 1.0
**Last Updated**: 2025-10-28
**Status**: DRAFT - Pending Approval
**Next Review**: After POC completion