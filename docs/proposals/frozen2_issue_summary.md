# Frozen2 Deserializer: string_table.buffer Not Being Read Correctly

## The Problem

When reading legacy DwarFS archives (v0.14.1, Thrift/Frozen2 format), the `compact_names` string table buffer is empty, causing filenames to appear as placeholders instead of actual names.

## Root Cause

The Thrift definition shows `buffer` is NOT optional:

```thrift
struct string_table {
   1: string buffer          // NOT optional!
   2: optional string symtab // IS optional
   ...
}
```

But `frozen2_deserializer.cpp` reads it as if it were `Option<string>`:
- Reads `is_some` boolean from wrong location
- Always sees `is_some=false`, skips buffer entirely

## What We See

```
buffer.size() = 0        // WRONG - should have data
buffer_size = 33         // Correct - we know the size
index = [0, 9, 19, 25, 33]  // Correct - offsets into buffer
```

Archive is valid (verified with `dwarfsck`), but C++ deserializer gets empty buffer.

## The Fix

Read field 1 directly as outlined string (distance + length), NOT as optional:

```cpp
// BEFORE (wrong):
bool is_present = buffer_reader.field_reader(1).read_bool();

// AFTER (correct):
uint32_t distance = buffer_reader.field_reader(1).read_u32();
uint32_t len = buffer_reader.field_reader(2).read_u32();
```

## Questions

1. Is this analysis correct?
2. Any version-specific encoding differences we should know about?
3. Why does Rust implementation work but C++ doesn't?

---

Full details: See `docs/proposals/frozen2_string_table_issue.md`
