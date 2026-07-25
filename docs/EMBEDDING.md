# Embedding libtfs

libtfs lets **any** program mount [DwarFS](https://github.com/mhx/dwarfs),
ZIP, or SquashFS disk images that are attached to its own binary (or shipped
as separate files) — one engine, one C ABI, thin per-language adapters. It is
the filesystem engine of the [Tebako](https://www.tebako.org) packager, but
it is a general-purpose library and does not depend on Tebako.

The contract you bind to is the **C ABI** in
[`include/tebako/fs/c_api.h`](../include/tebako/fs/c_api.h):
`tebako_fs_*` / `tebako_get_*` / `tebako_strerror` / `tebako_is_initialized`
functions over POD types and opaque handles, with a thread-local errno
channel. No C++ types, no callbacks, no exceptions cross the boundary.

## Getting the library

| Artifact | Notes |
|---|---|
| `libtfs.a` | Static library; primary artifact. Self-contained except for the transitive dependency closure (see the CMake package config / `libtfs-deps` release package). |
| `libtfs.so` / `libtfs.dylib` | Shared library exporting **only** the `tebako_*` C ABI (everything else, including all backend and third-party symbols, is hidden). Windows `.dll` is deferred to the release pipeline. |
| `libtfs.pc` | pkg-config file for both modes (see below). |
| `libtfsConfig.cmake` | CMake package config (`find_package(libtfs)`, target `libtfs::tfs`). This is the precise route for **static** consumption. |

pkg-config:

```sh
# Shared consumption — this is all you need:
cc main.c $(pkg-config --cflags --libs libtfs) -o main

# Static consumption needs the transitive closure:
cc main.c $(pkg-config --cflags --libs --static libtfs) -o main
```

## Attaching images to your binary

Two paths; both end with the app calling a mount function at run time.

**Compile-time (incbin).** Your build embeds the image bytes into a section
of the executable; the app mounts them from memory:

```c
extern const uint8_t image_start[], image_end[];
tebako_fs_init(image_start, image_end - image_start, "/__app__");
```

**Post-build (tebakofs append).** The `tebakofs` tool attaches image slots
plus a `tpkg` manifest trailer to an *existing* binary — no rebuild, fully
language-agnostic. The app then mounts each image as a byte region of its
own executable:

```c
tebako_fs_init_from_file_at(argv[0], slot_offset, slot_length, "/__app__");
```

## Mounting: single and multi

- `tebako_fs_init*()` is the compat single-mount API: it fails with
  `EEXIST` if anything is already mounted.
- `tebako_fs_mount_from_file()` / `tebako_fs_mount_from_file_at()` /
  `tebako_fs_mount_from_memory()` mount **additional** images, each at its
  own mount point, and return a `tebako_mount_t` handle.
  `tebako_fs_unmount_handle(h)` unmounts one mount (its open fds/dirs are
  force-closed and fail with `EBADF` afterwards); `tebako_fs_unmount()`
  tears down *all* mounts.
- Paths are dispatched to the owning mount by **longest mount-point
  prefix** match; nested mount points are allowed, duplicates are rejected
  with `EEXIST`.
- `tebako_path_is_embedded(path)` tells you whether a path would be served
  by libtfs (useful for intercepting `open()` in your own runtime).

## Read-only model

All backends are read-only. `tebako_fs_open()` accepts only `O_RDONLY`;
anything else fails with `errno = EROFS`. If you need files on disk,
`tebako_fs_extract_all(dest)` extracts the mounted tree(s) — a single mount
extracts directly into `dest`; with multiple mounts each tree goes into its
own `<dest>/<mount-point-basename>` subtree.

## Threading

Today the C API serializes everything through a **single process-wide
mutex** (per mounted filesystem the underlying readers are themselves
thread-safe). Calls are safe from any thread; they just won't overlap. The
errno channel is **thread-local**: each thread reads its own
`tebako_get_errno()`.

## Error model

- Functions return `-1` (or `NULL`) on failure; the cause is a standard
  `errno` value available via `tebako_get_errno()` (thread-local) and
  printable via `tebako_strerror()`.
- A successful call resets the error state to `0`.
- There are **no callbacks** in the ABI today. If progress/logging hooks
  are ever added, they will be plain function pointers with documented
  ownership, and no exceptions will cross the boundary.

## ABI versioning

`TEBAKO_FS_ABI_VERSION` (header macro) and `tebako_fs_abi_version()`
(library call) carry the ABI version, independent of the release version.
The version is bumped only on ABI-incompatible changes; additive changes
(new symbols) keep the version and are feature-detected by symbol presence.
An adapter should compare at load time:

```c
if (tebako_fs_abi_version() < TEBAKO_FS_ABI_VERSION) { /* too old */ }
```

## Minimal C adapter

```c
#include <stdio.h>
#include <fcntl.h>
#include <tebako/fs/c_api.h>

int main(int argc, char** argv)
{
  if (argc < 3) { fprintf(stderr, "usage: %s IMAGE PATH\n", argv[0]); return 2; }

  if (tebako_fs_abi_version() < TEBAKO_FS_ABI_VERSION) {
    fprintf(stderr, "libtfs ABI %d too old (need %d)\n",
            tebako_fs_abi_version(), TEBAKO_FS_ABI_VERSION);
    return 3;
  }

  /* Mount the image (ZIP/DwarFS/SquashFS, auto-detected) */
  if (tebako_fs_init_from_file(argv[1], "/__app__") != 0) {
    fprintf(stderr, "mount: %s\n", tebako_strerror(tebako_get_errno()));
    return 4;
  }

  char path[512];
  snprintf(path, sizeof path, "/__app__/%s", argv[2]);
  int fd = tebako_fs_open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "open: %s\n", tebako_strerror(tebako_get_errno()));
    return 5;
  }

  char buf[4096];
  ssize_t n;
  while ((n = tebako_fs_read(fd, buf, sizeof buf)) > 0)
    fwrite(buf, 1, (size_t)n, stdout);

  tebako_fs_close(fd);
  tebako_fs_unmount();
  return n < 0 ? 6 : 0;
}
```

## Minimal Python adapter (cffi)

```python
# pip install cffi; needs libtfs.so/.dylib on the loader path
from cffi import FFI

ffi = FFI()
ffi.cdef("""
    int tebako_fs_abi_version(void);
    int tebako_fs_init_from_file(const char*, const char*);
    int tebako_fs_open(const char*, int);
    ssize_t tebako_fs_read(int, void*, size_t);
    int tebako_fs_close(int);
    void tebako_fs_unmount(void);
    int tebako_get_errno(void);
    const char* tebako_strerror(int);
""")
tfs = ffi.dlopen("libtfs")          # or an absolute path

O_RDONLY = 0

class TfsError(OSError):
    def __init__(self, what):
        err = tfs.tebako_get_errno()
        super().__init__(f"{what}: {ffi.string(tfs.tebako_strerror(err)).decode()}", err)

def read_file(image: str, inner: str) -> bytes:
    """Mount `image` at /__app__, read `inner` from it, unmount."""
    if tfs.tebako_fs_init_from_file(image.encode(), b"/__app__") != 0:
        raise TfsError("mount")
    try:
        fd = tfs.tebako_fs_open(f"/__app__/{inner}".encode(), O_RDONLY)
        if fd < 0:
            raise TfsError("open")
        try:
            out = bytearray()
            buf = ffi.new("char[]", 65536)
            while (n := tfs.tebako_fs_read(fd, buf, len(buf))) > 0:
                out += ffi.buffer(buf, n)
            if n < 0:
                raise TfsError("read")
            return bytes(out)
        finally:
            tfs.tebako_fs_close(fd)
    finally:
        tfs.tebako_fs_unmount()

if __name__ == "__main__":
    import sys
    sys.stdout.buffer.write(read_file(sys.argv[1], sys.argv[2]))
```

The same shape — map functions, turn the errno channel into exceptions,
wrap handles in close-guards — is the template for adapters in any language
(Ruby fiddle/FFI, Rust `extern "C"`, Go cgo, …). Adapters live in their own
language ecosystems and pin `TEBAKO_FS_ABI_VERSION`.

## Licensing (read this before shipping)

libtfs itself is BSD-2-Clause. **What you ship additionally depends on the
backends you link, and backend selection changes your obligations:**

| Backend | License | Obligation when linked into your product |
|---|---|---|
| DwarFS (`libdwarfs_c` + reader) | **GPL-3.0** | Statically linking the DwarFS backend creates GPL-3.0 obligations for your product (combined work): you must be prepared to offer your product under GPL-compatible terms, including source offer. There is no linking exception. If that is not acceptable, do not link the DwarFS backend. |
| ZIP (libzip) | BSD-3-Clause | Notice retention only; fine for proprietary use. |
| SquashFS (squashfs-tools-ng) | **LGPL-3.0** | Dynamic linking is the simple compliant path. Static linking triggers LGPL-3.0 §4(d)(0)/(1): you must allow the user to relink against a modified squashfs-tools-ng (e.g. ship your object files or use shared libraries), plus license notices. |

The ZIP backend alone carries no copyleft obligations. Dropping DwarFS
removes the GPL-3.0 obligation; dropping SquashFS removes the LGPL-3.0
one. Backend selection is a build-time decision (`WITH_SQUASHFS`, and
whether you link `dwarfs::dwarfs_c`); the obligations above follow the code
you actually ship, not the code the project offers. Third-party
compression/filter dependencies (zstd, lz4, xz/lzma, brotli, zlib, bzip2,
xxhash) are permissively licensed (BSD/MIT/zlib-style).
