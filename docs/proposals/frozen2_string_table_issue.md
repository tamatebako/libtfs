# Issue Report: Frozen2 Deserializer `string_table` Buffer Reading

## Summary

When reading legacy DwarFS archives (v0.14.1, Thrift/Frozen2 format) with the C++ frozen2_deserializer, the `compact_names` string table buffer is not being correctly populated. This causes filenames to appear as placeholders (`file_1`, `file_2`, etc.) instead of actual names (`hello.txt`, `subdir`, etc.).

## Environment

- **Archive created by**: Homebrew mkdwarfs v0.14.1 (macOS ARM64)
- **Archive format**: Legacy Thrift/Frozen2 (not FlatBuffers)
- **Reader**: C++ frozen2_deserializer from dwarfs v0.16.0.x (vcpkg build)
- **Platform**: macOS 15.x (Darwin 24.x)

## Root Cause Analysis

### Thrift Definition

From `thrift/metadata.thrift`:

```thrift
struct string_table {
   1: string buffer          // NOT optional!
   2: optional string symtab
   3: list<UInt32> index
   4: bool packed_index
}
```

**Key observation**: `buffer` (field 1) is a plain `string`, NOT `optional string`.

### Current Behavior

The C++ `frozen2_deserializer.cpp` reads field 1 as if it were `Option<string>`:

```cpp
// Current code treats buffer as optional
auto buffer_reader = field_reader(1);
if (buffer_reader.layout_ && !buffer_reader.layout_->fields.is_empty()) {
    bool is_present = buffer_reader.field_reader(1).read_bool();  // WRONG!
    if (is_present) {
        table.buffer = buffer_reader.field_reader(2).read_string();
    }
}
```

**Problem**: Reading `is_present` from field 1's sub-field 1 reads the wrong bits, always appearing as `false`.

### Expected Behavior

In the Frozen2 format, non-optional strings are encoded as outlined strings with:
- Field 1: `distance` (uint32_t) - offset from `storage_start`
- Field 2: `length` (uint32_t) - size of the string data

The correct reading should be:

```cpp
// Correct: Read buffer directly (NOT as optional)
auto buffer_reader = field_reader(1);
if (buffer_reader.layout_ && !buffer_reader.layout_->fields.is_empty()) {
    uint32_t distance = buffer_reader.field_reader(1).read_u32();
    uint32_t len = buffer_reader.field_reader(2).read_u32();
    if (len > 0 && storage_start_ + distance + len <= data_.size()) {
        uint32_t start = storage_start_ + distance;
        table.buffer = std::string(
            reinterpret_cast<char const*>(data_.data() + start), len);
    }
}
```

## Debug Output

When the bug manifests:

```
[DEBUG] read_string_table: table.buffer.size()=0
[DEBUG] buffer_size=33, buffer_distance=0, symtab_distance=0
[DEBUG] index.size()=5, values=[0, 9, 19, 25, 33]
```

The buffer is empty (`size()=0`) but the index is correctly read.

Archive verification with Homebrew's `dwarfsck`:

```
metadata memory usage:
  4 compact_names...........................37 bytes  33.9%   9.2 bytes/item
    |- data                                 33 bytes  30.3%   8.2 bytes/item
    '- index                                 4 bytes   3.7%   1.0 bytes/item
```

The archive is valid - it has 33 bytes of compact_names data.

## Comparison with Rust Implementation

The Rust implementation in `dwarfs-rs` uses serde's type system which correctly handles non-optional fields. The `BString` type is deserialized directly without an `Option<>` wrapper:

```rust
pub struct StringTable {
    pub buffer: BString,        // NOT optional
    pub symtab: Option<BString>, // Optional
    pub index: Vec<u32>,
    pub packed_index: bool,
}
```

## Questions for DwarFS Team

1. Is our analysis correct that `buffer` should be read as a non-optional outlined string?

2. Is there a difference in how legacy archives (v0.14.1) encode `string_table` vs newer formats?

3. Could there be a version-specific encoding difference we're missing?

4. The Rust implementation works correctly - is there something specific about the C++ frozen2 deserializer that differs?

## Proposed Fix

In `src/metadata/legacy/frozen2_deserializer.cpp`, modify `read_string_table()`:

1. Read field 1 directly as an outlined string (distance + length)
2. Do NOT check for an `is_some` boolean first
3. Apply the same logic for `symtab` (which IS optional)

## Test Case

Archive: `simple.dwarfs` with files:
- `hello.txt` (15 bytes: "Hello, DwarFS!\n")
- `test.txt` (18 bytes: "Test file content")
- `subdir/nested.txt` (12 bytes: "Nested file")

Expected: Correct filenames from `compact_names`
Actual: Placeholder names (`file_1`, `file_2`, etc.)

---

**Contact**: libtfs/tamatebako project
**Date**: 2026-02-19
