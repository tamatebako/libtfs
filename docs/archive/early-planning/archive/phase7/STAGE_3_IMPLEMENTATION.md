# Stage 3 Implementation: Multi-Language Bindings

**Duration**: 3 weeks
**Status**: Ready to execute
**Goal**: Language adapters for Julia, Python, Node.js
**Prerequisites**: Stage 1 & 2 completed

---

## Overview

Expand libtfs to support multiple programming languages beyond Ruby, providing idiomatic bindings for Julia, Python, and Node.js. Each language gets a natural interface while maintaining the same core functionality.

**Architecture**: Thin language-specific wrappers around stable C API.

---

## Motivation

### Why Multi-Language?

1. **Ecosystem Expansion** - Reach beyond Ruby community
2. **Use Case Diversity** - Scientific computing (Julia), data science (Python), web services (Node.js)
3. **Architecture Validation** - Proves C API design quality
4. **Community Growth** - More contributors, more use cases

### Target Languages

| Language | Rationale | Primary Use Case |
|----------|-----------|------------------|
| **Julia** | Scientific computing, performance | HPC, data processing |
| **Python** | Ubiquitous, huge ecosystem | General purpose, ML/AI |
| **Node.js** | Web ecosystem, async I/O | Web services, tooling |
| **Ruby** | ✅ Already supported | Tebako packaging |

---

## Architecture

### C API Layer

**Purpose**: Stable, language-neutral interface

```c
// include/tebako/fs/c_api.h
#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types
typedef struct tfs_filesystem tfs_filesystem_t;
typedef struct tfs_file tfs_file_t;
typedef struct tfs_dir tfs_dir_t;

// Lifecycle
tfs_filesystem_t* tfs_mount(const char* path, const char* mountpoint, const char* backend);
int tfs_unmount(tfs_filesystem_t* fs);

// File operations
tfs_file_t* tfs_open(tfs_filesystem_t* fs, const char* path, int flags);
ssize_t tfs_read(tfs_file_t* file, void* buf, size_t count);
int tfs_close(tfs_file_t* file);

// Directory operations
tfs_dir_t* tfs_opendir(tfs_filesystem_t* fs, const char* path);
const char* tfs_readdir(tfs_dir_t* dir);
int tfs_closedir(tfs_dir_t* dir);

// Metadata
int tfs_stat(tfs_filesystem_t* fs, const char* path, struct stat* st);

// Error handling
const char* tfs_last_error(void);
void tfs_clear_error(void);

#ifdef __cplusplus
}
#endif
```

### Language Binding Layers

```
┌─────────────────────────────────────────┐
│          User Applications              │
├──────────┬──────────┬──────────┬────────┤
│  Julia   │  Python  │  Node.js │  Ruby  │
├──────────┴──────────┴──────────┴────────┤
│         Language Bindings               │
│  (Idiomatic wrappers, error handling)   │
├──────────────────────────────────────────┤
│              C API Layer                │
│      (Stable, version-controlled)       │
├──────────────────────────────────────────┤
│            C++ Core (libtfs)            │
│   (Backends, I/O, mount management)     │
└──────────────────────────────────────────┘
```

---

## Three-Week Plan

### Week 1: C API & Julia Bindings

#### Day 1-2: C API Implementation

**File**: `include/tebako/fs/c_api.h` (shown above)

**File**: `src/c_api.cpp`

```cpp
#include <tebako/fs/c_api.h>
#include <tebako/fs/io.h>
#include <tebako/fs/memfs.h>
#include <cstring>
#include <string>

// Error handling
thread_local std::string last_error;

extern "C" {

const char* tfs_last_error(void) {
    return last_error.c_str();
}

void tfs_clear_error(void) {
    last_error.clear();
}

tfs_filesystem_t* tfs_mount(const char* path, const char* mountpoint,
                            const char* backend) {
    try {
        auto fs = new tebako::fs::Filesystem();
        if (!fs->mount(path, mountpoint, backend)) {
            last_error = "Failed to mount filesystem";
            delete fs;
            return nullptr;
        }
        return reinterpret_cast<tfs_filesystem_t*>(fs);
    } catch (const std::exception& e) {
        last_error = e.what();
        return nullptr;
    }
}

int tfs_unmount(tfs_filesystem_t* fs) {
    try {
        auto* filesystem = reinterpret_cast<tebako::fs::Filesystem*>(fs);
        bool result = filesystem->unmount();
        delete filesystem;
        return result ? 0 : -1;
    } catch (const std::exception& e) {
        last_error = e.what();
        return -1;
    }
}

// ... rest of API implementation ...

} // extern "C"
```

#### Day 3-5: Julia Bindings

**File**: `bindings/julia/LibTFS.jl/src/LibTFS.jl`

```julia
module LibTFS

using CEnum

# Load library
const libtfs = joinpath(@__DIR__, "..", "deps", "usr", "lib", "libtfs")

# C API declarations
mutable struct tfs_filesystem_t end
mutable struct tfs_file_t end
mutable struct tfs_dir_t end

function tfs_mount(path::String, mountpoint::String, backend::String="auto")
    fs = ccall((:tfs_mount, libtfs), Ptr{tfs_filesystem_t},
               (Cstring, Cstring, Cstring), path, mountpoint, backend)
    if fs == C_NULL
        error("Failed to mount: $(unsafe_string(tfs_last_error()))")
    end
    return fs
end

function tfs_unmount(fs::Ptr{tfs_filesystem_t})
    result = ccall((:tfs_unmount, libtfs), Cint, (Ptr{tfs_filesystem_t},), fs)
    if result != 0
        error("Failed to unmount: $(unsafe_string(tfs_last_error()))")
    end
end

function tfs_last_error()
    ccall((:tfs_last_error, libtfs), Cstring, ())
end

# High-level Julia interface
struct Filesystem
    handle::Ptr{tfs_filesystem_t}

    function Filesystem(path::String, mountpoint::String; backend::String="auto")
        handle = tfs_mount(path, mountpoint, backend)
        fs = new(handle)
        finalizer(fs -> tfs_unmount(fs.handle), fs)
        return fs
    end
end

function open(fs::Filesystem, path::String, mode::String="r")
    flags = mode == "r" ? 0x0000 : error("Unsupported mode: $mode")
    file_ptr = ccall((:tfs_open, libtfs), Ptr{tfs_file_t},
                     (Ptr{tfs_filesystem_t}, Cstring, Cint),
                     fs.handle, path, flags)
    if file_ptr == C_NULL
        error("Failed to open file: $(unsafe_string(tfs_last_error()))")
    end
    return file_ptr
end

function read(file::Ptr{tfs_file_t}, size::Int)
    buffer = Vector{UInt8}(undef, size)
    bytes_read = ccall((:tfs_read, libtfs), Cssize_t,
                      (Ptr{tfs_file_t}, Ptr{UInt8}, Csize_t),
                      file, buffer, size)
    if bytes_read < 0
        error("Failed to read: $(unsafe_string(tfs_last_error()))")
    end
    return buffer[1:bytes_read]
end

function close(file::Ptr{tfs_file_t})
    ccall((:tfs_close, libtfs), Cint, (Ptr{tfs_file_t},), file)
end

# Convenience functions
function readfile(fs::Filesystem, path::String)
    file = open(fs, path)
    try
        # Get file size
        stat_buf = Ref{Stat}()
        # ... stat implementation ...

        # Read entire file
        return read(file, filesize)
    finally
        close(file)
    end
end

export Filesystem, open, read, close, readfile

end # module
```

**File**: `bindings/julia/LibTFS.jl/test/runtests.jl`

```julia
using Test
using LibTFS

@testset "LibTFS.jl" begin
    @testset "Mounting" begin
        fs = Filesystem("test.dwarfs", "/mnt/test")
        @test fs.handle != C_NULL
    end

    @testset "File Operations" begin
        fs = Filesystem("test.dwarfs", "/mnt/test")
        content = readfile(fs, "/file.txt")
        @test length(content) > 0
    end
end
```

### Week 2: Python Bindings

#### Day 6-8: Python Interface (ctypes)

**File**: `bindings/python/libtfs/__init__.py`

```python
"""LibTFS Python bindings"""

import ctypes
import os
from pathlib import Path
from typing import Optional, Union

# Load library
_lib_path = Path(__file__).parent / "lib" / "libtfs.so"
_lib = ctypes.CDLL(str(_lib_path))

# Define C types
class _FilesystemHandle(ctypes.Structure):
    pass

class _FileHandle(ctypes.Structure):
    pass

class _DirHandle(ctypes.Structure):
    pass

# C function signatures
_lib.tfs_mount.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.tfs_mount.restype = ctypes.POINTER(_FilesystemHandle)

_lib.tfs_unmount.argtypes = [ctypes.POINTER(_FilesystemHandle)]
_lib.tfs_unmount.restype = ctypes.c_int

_lib.tfs_open.argtypes = [ctypes.POINTER(_FilesystemHandle), ctypes.c_char_p, ctypes.c_int]
_lib.tfs_open.restype = ctypes.POINTER(_FileHandle)

_lib.tfs_read.argtypes = [ctypes.POINTER(_FileHandle), ctypes.c_void_p, ctypes.c_size_t]
_lib.tfs_read.restype = ctypes.c_ssize_t

_lib.tfs_close.argtypes = [ctypes.POINTER(_FileHandle)]
_lib.tfs_close.restype = ctypes.c_int

_lib.tfs_last_error.argtypes = []
_lib.tfs_last_error.restype = ctypes.c_char_p

# Python wrapper classes
class LibTFSError(Exception):
    """Base exception for LibTFS errors"""
    pass

class Filesystem:
    """Virtual filesystem backed by archive"""

    def __init__(self, path: Union[str, Path], mountpoint: str,
                 backend: str = "auto"):
        """
        Mount a filesystem

        Args:
            path: Path to archive file
            mountpoint: Virtual mount point
            backend: Backend type ('dwarfs', 'zip', 'auto')
        """
        path_bytes = str(path).encode('utf-8')
        mount_bytes = mountpoint.encode('utf-8')
        backend_bytes = backend.encode('utf-8')

        self._handle = _lib.tfs_mount(path_bytes, mount_bytes, backend_bytes)
        if not self._handle:
            error = _lib.tfs_last_error().decode('utf-8')
            raise LibTFSError(f"Failed to mount: {error}")

    def __del__(self):
        if hasattr(self, '_handle') and self._handle:
            _lib.tfs_unmount(self._handle)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self._handle:
            _lib.tfs_unmount(self._handle)
            self._handle = None
        return False

    def open(self, path: str, mode: str = 'r') -> 'File':
        """Open file in filesystem"""
        return File(self, path, mode)

    def read_text(self, path: str, encoding: str = 'utf-8') -> str:
        """Read entire text file"""
        with self.open(path, 'r') as f:
            return f.read().decode(encoding)

    def read_bytes(self, path: str) -> bytes:
        """Read entire binary file"""
        with self.open(path, 'rb') as f:
            return f.read()

class File:
    """File handle within filesystem"""

    def __init__(self, filesystem: Filesystem, path: str, mode: str = 'r'):
        flags = 0x0000 if 'r' in mode else 0x0001
        path_bytes = path.encode('utf-8')

        self._handle = _lib.tfs_open(filesystem._handle, path_bytes, flags)
        if not self._handle:
            error = _lib.tfs_last_error().decode('utf-8')
            raise LibTFSError(f"Failed to open: {error}")

    def read(self, size: int = -1) -> bytes:
        """Read from file"""
        if size < 0:
            size = 1024 * 1024  # 1MB default

        buffer = ctypes.create_string_buffer(size)
        bytes_read = _lib.tfs_read(self._handle, buffer, size)

        if bytes_read < 0:
            error = _lib.tfs_last_error().decode('utf-8')
            raise LibTFSError(f"Failed to read: {error}")

        return buffer.raw[:bytes_read]

    def close(self):
        """Close file"""
        if self._handle:
            _lib.tfs_close(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def __del__(self):
        self.close()

__all__ = ['Filesystem', 'File', 'LibTFSError']
```

**File**: `bindings/python/tests/test_libtfs.py`

```python
import unittest
from pathlib import Path
import libtfs

class TestLibTFS(unittest.TestCase):

    def setUp(self):
        self.test_archive = Path(__file__).parent.parent.parent.parent / "tests" / "test.dwarfs"

    def test_mount(self):
        """Test filesystem mounting"""
        with libtfs.Filesystem(self.test_archive, "/mnt/test") as fs:
            self.assertIsNotNone(fs)

    def test_read_text(self):
        """Test reading text file"""
        with libtfs.Filesystem(self.test_archive, "/mnt/test") as fs:
            content = fs.read_text("/file.txt")
            self.assertIsInstance(content, str)
            self.assertGreater(len(content), 0)

    def test_read_bytes(self):
        """Test reading binary file"""
        with libtfs.Filesystem(self.test_archive, "/mnt/test") as fs:
            data = fs.read_bytes("/file.txt")
            self.assertIsInstance(data, bytes)
            self.assertGreater(len(data), 0)

    def test_file_operations(self):
        """Test file open/read/close"""
        with libtfs.Filesystem(self.test_archive, "/mnt/test") as fs:
            with fs.open("/file.txt") as f:
                data = f.read()
                self.assertGreater(len(data), 0)

if __name__ == '__main__':
    unittest.main()
```

#### Day 9-10: Python Packaging

**File**: `bindings/python/setup.py`

```python
from setuptools import setup, find_packages
from pathlib import Path

# Read README
readme = Path(__file__).parent / "README.md"
long_description = readme.read_text() if readme.exists() else ""

setup(
    name="libtfs",
    version="2.0.0",
    description="Tebako File System - Virtual filesystem backed by archives",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Ribose Inc.",
    author_email="open.source@ribose.com",
    url="https://github.com/tamatebako/libtfs",
    packages=find_packages(),
    package_data={
        "libtfs": ["lib/*.so", "lib/*.dylib", "lib/*.dll"],
    },
    python_requires=">=3.8",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: BSD License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Topic :: Software Development :: Libraries",
        "Topic :: System :: Filesystems",
    ],
)
```

### Week 3: Node.js Bindings

#### Day 11-13: Node.js Native Module

**File**: `bindings/nodejs/binding.gyp`

```json
{
  "targets": [
    {
      "target_name": "libtfs",
      "sources": [
        "src/binding.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../../../include"
      ],
      "libraries": [
        "-L../../../build",
        "-ltfs"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7"
      }
    }
  ]
}
```

**File**: `bindings/nodejs/src/binding.cpp`

```cpp
#include <napi.h>
#include <tebako/fs/c_api.h>

class Filesystem : public Napi::ObjectWrap<Filesystem> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    Filesystem(const Napi::CallbackInfo& info);
    ~Filesystem();

private:
    static Napi::FunctionReference constructor;
    tfs_filesystem_t* fs_;

    Napi::Value Open(const Napi::CallbackInfo& info);
    Napi::Value ReadText(const Napi::CallbackInfo& info);
    Napi::Value ReadBytes(const Napi::CallbackInfo& info);
    void Close(const Napi::CallbackInfo& info);
};

Napi::FunctionReference Filesystem::constructor;

Napi::Object Filesystem::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Filesystem", {
        InstanceMethod("open", &Filesystem::Open),
        InstanceMethod("readText", &Filesystem::ReadText),
        InstanceMethod("readBytes", &Filesystem::ReadBytes),
        InstanceMethod("close", &Filesystem::Close),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set("Filesystem", func);
    return exports;
}

Filesystem::Filesystem(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Filesystem>(info), fs_(nullptr) {

    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected (path, mountpoint, [backend])")
            .ThrowAsJavaScriptException();
        return;
    }

    std::string path = info[0].As<Napi::String>();
    std::string mountpoint = info[1].As<Napi::String>();
    std::string backend = info.Length() > 2 ?
        info[2].As<Napi::String>().Utf8Value() : "auto";

    fs_ = tfs_mount(path.c_str(), mountpoint.c_str(), backend.c_str());

    if (!fs_) {
        Napi::Error::New(env, tfs_last_error())
            .ThrowAsJavaScriptException();
    }
}

Filesystem::~Filesystem() {
    if (fs_) {
        tfs_unmount(fs_);
    }
}

Napi::Value Filesystem::ReadText(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected path string")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string path = info[0].As<Napi::String>();

    // Open file
    tfs_file_t* file = tfs_open(fs_, path.c_str(), 0);
    if (!file) {
        Napi::Error::New(env, tfs_last_error())
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    // Read content
    std::vector<char> buffer(1024 * 1024);
    ssize_t bytes = tfs_read(file, buffer.data(), buffer.size());
    tfs_close(file);

    if (bytes < 0) {
        Napi::Error::New(env, tfs_last_error())
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    return Napi::String::New(env, buffer.data(), bytes);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return Filesystem::Init(env, exports);
}

NODE_API_MODULE(libtfs, Init)
```

**File**: `bindings/nodejs/index.js`

```javascript
const addon = require('./build/Release/libtfs.node');

class Filesystem {
    constructor(path, mountpoint, backend = 'auto') {
        this._fs = new addon.Filesystem(path, mountpoint, backend);
    }

    readText(path, encoding = 'utf-8') {
        return this._fs.readText(path);
    }

    readBytes(path) {
        const text = this._fs.readText(path);
        return Buffer.from(text, 'binary');
    }

    close() {
        this._fs.close();
    }
}

module.exports = { Filesystem };
```

#### Day 14-15: Testing & Documentation

**File**: `bindings/nodejs/test/test.js`

```javascript
const assert = require('assert');
const { Filesystem } = require('..');
const path = require('path');

describe('LibTFS Node.js Bindings', function() {
    const testArchive = path.join(__dirname, '..', '..', '..', 'tests', 'test.dwarfs');

    describe('Filesystem', function() {
        it('should mount filesystem', function() {
            const fs = new Filesystem(testArchive, '/mnt/test');
            assert.ok(fs);
            fs.close();
        });

        it('should read text file', function() {
            const fs = new Filesystem(testArchive, '/mnt/test');
            const content = fs.readText('/file.txt');
            assert.ok(content.length > 0);
            fs.close();
        });

        it('should read binary file', function() {
            const fs = new Filesystem(testArchive, '/mnt/test');
            const data = fs.readBytes('/file.txt');
            assert.ok(Buffer.isBuffer(data));
            assert.ok(data.length > 0);
            fs.close();
        });
    });
});
```

---

## Success Criteria

### Per-Language

#### Julia ✅
- [ ] Package builds successfully
- [ ] Tests pass on all platforms
- [ ] Registered in Julia General registry
- [ ] Documentation complete
- [ ] Examples provided

#### Python ✅
- [ ] pip installable
- [ ] Tests pass (pytest)
- [ ] Published to PyPI
- [ ] Type hints complete
- [ ] Sphinx documentation

#### Node.js ✅
- [ ] npm installable
- [ ] Tests pass (mocha)
- [ ] Published to npm
- [ ] TypeScript definitions
- [ ] API documentation

### Overall
- [ ] All languages have feature parity
- [ ] Cross-language API consistency
- [ ] No C++ API regressions
- [ ] Documentation covers all languages
- [ ] CI/CD for all bindings

---

## Distribution

### Julia
```julia
# Install from registry
using Pkg
Pkg.add("LibTFS")

# Usage
using LibTFS
fs = Filesystem("app.dwarfs", "/app")
content = readfile(fs, "/config.json")
```

### Python
```bash
# Install from PyPI
pip install libtfs

# Usage
from libtfs import Filesystem

with Filesystem("app.dwarfs", "/app") as fs:
    content = fs.read_text("/config.json")
```

### Node.js
```bash
# Install from npm
npm install libtfs

# Usage
const { Filesystem } = require('libtfs');

const fs = new Filesystem('app.dwarfs', '/app');
const content = fs.readText('/config.json');
fs.close();
```

---

## Continuous Integration

**File**: `.github/workflows/bindings.yml`

```yaml
name: Language Bindings

on: [push, pull_request]

jobs:
  julia:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-22.04, macos-13]
        julia-version: ['1.9', '1.10']

    steps:
    - uses: actions/checkout@v4
    - uses: julia-actions/setup-julia@v1
      with:
        version: ${{ matrix.julia-version }}
    - run: julia --project=bindings/julia -e 'using Pkg; Pkg.test()'

  python:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-22.04, macos-13]
        python-version: ['3.8', '3.9', '3.10', '3.11', '3.12']

    steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-python@v4
      with:
        python-version: ${{ matrix.python-version }}
    - run: |
        cd bindings/python
        pip install -e .
        pytest tests/

  nodejs:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-22.04, macos-13]
        node-version: ['18', '20', '21']

    steps:
    - uses: actions/checkout@v4
    - uses: actions/setup-node@v4
      with:
        node-version: ${{ matrix.node-version }}
    - run: |
        cd bindings/nodejs
        npm install
        npm test
```

---

## Documentation

Each language gets:

1. **README.md** - Quick start, installation
2. **API.md** - Complete API reference
3. **EXAMPLES.md** - Code examples
4. **CONTRIBUTING.md** - How to contribute

Plus central cross-language docs in main repository.

---

## Future Enhancements

### Additional Languages
- **Rust** - Systems programming
- **Go** - Cloud services
- **C#** - .NET ecosystem
- **Java** - Enterprise applications

### Enhanced Features
- Async I/O (Python asyncio, Node.js promises)
- Streaming reads
- Memory-mapped access
- Multi-threading support

---

## Rollback Plan

If critical issues:
```bash
# Disable specific binding
cmake -DWITH_JULIA_BINDINGS=OFF ..

# Or all bindings
cmake -DWITH_LANGUAGE_BINDINGS=OFF ..
```

---

## Completion

Upon Stage 3 completion:
- LibTFS supports 4 languages
- Production-ready bindings
- Published packages
- Complete documentation
- Comprehensive CI/CD

**Ready for v2.0.0 stable release! 🎉**

---

**Last Updated**: 2025-01-17