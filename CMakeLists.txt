# Copyright (c) 2021-2025 [Ribose Inc](https://www.ribose.com).
# All rights reserved.
# This file is a part of the Tebako project.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required (VERSION 3.24)

# For project( ... VERSION ... )
cmake_policy(SET CMP0048 NEW)
# For CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)
# DOWNLOAD_EXTRACT_TIMESTAMP option default = true
cmake_policy(SET CMP0135 NEW)
# FindBoost moudule
# Use Boost’s CMake package
if (POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

set(TOOLS ${CMAKE_CURRENT_SOURCE_DIR}/tools CACHE STRING "Tools' folder'")

include(${TOOLS}/cmake-scripts/version.cmake)
determine_version("${CMAKE_CURRENT_SOURCE_DIR}" LIBDWARFS_WR)

# Various locations for external projects
set(DEPS ${CMAKE_CURRENT_SOURCE_DIR}/deps CACHE STRING "Dependencies' folder'")
set(DEPS_INCLUDE_DIR ${DEPS}/include)
set(DEPS_LIB_DIR ${DEPS}/lib)
set(DEPS_BIN_DIR ${DEPS}/bin)
set(DEPS_SBIN_DIR ${DEPS}/sbin)

if(MSVC)
  if(NOT VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR "${DEPS}/vcpkg-installed")
  endif(NOT VCPKG_INSTALLED_DIR)
  if(NOT VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static")
  endif(NOT VCPKG_TARGET_TRIPLET)
endif(MSVC)

project(libtfs VERSION ${LIBDWARFS_WR_VERSION})

option(PREFER_SYSTEM_GTEST "Use system Google test" OFF)
option(WITH_TESTS "Find Google test, install INCBIN, build test applications" ON)
option(WITH_LINK_TESTS "Include tests for hard and symbolic links" ON)

include(ExternalProject)
include(GNUInstallDirs)
include(CheckSymbolExists)
include(CheckCXXSourceCompiles)

# This is the default that will be overwritten for MacOS
set(GNU_BASH "bash")

include(${TOOLS}/cmake-scripts/macos-environment.cmake)
include(${TOOLS}/cmake-scripts/def-external-project.cmake)

if(MSVC)
  include_directories(${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)
  link_directories(${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib)
endif(MSVC)

# log level
if (NOT TESTS_LOG_LEVEL)
  set(TESTS_LOG_LEVEL "warn")
  message(STATUS "Logging level not specified. Defaulted to warn")
endif(NOT TESTS_LOG_LEVEL)

# coverage
if (WITH_COVERAGE)
  if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Coverage is only supported with the GNU compiler")
  endif()
  add_compile_options(--coverage -O0)
  add_link_options(--coverage)
endif(WITH_COVERAGE)

# sanitizers
if (WITH_ASAN)
  if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Sanitizers are only supported with the clang compiler")
  endif()
  add_compile_options(-DWITH_ASAN -fsanitize=address,undefined -fno-omit-frame-pointer -fno-common -O1)
  link_libraries(-fsanitize=address,undefined)
endif(WITH_ASAN)

if (WITH_COVERAGE OR WITH_ASAN)
  message(STATUS "Forcing WITH_TEST=ON for code coverage or sanitizers")
  set(WITH_TESTS ON CACHE BOOL "Forced for code coverage or sanitizers" FORCE)
  message(STATUS "Forcing build type to Debug for code coverage or sanitizers")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Forced for code coverage or sanitizers" FORCE)
endif(WITH_COVERAGE OR WITH_ASAN)

if(MKDWARFS)
  set(TEBAKO_BUILD_SCOPE LIB)
  message(STATUS "Using upstream mkdwarfs at ${MKDWARFS}")
else(MKDWARFS)
  set(TEBAKO_BUILD_SCOPE MKD)
  set(MKDWARFS ${DEPS_BIN_DIR}/mkdwarfs)
  message(STATUS "Building mkdwarfs at ${MKDWARFS}")
endif(MKDWARFS)

if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "Build type was not specified. Defaulted to release")
  set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

# Reasonable defaults
set(USE_JEMALLOC ON)
set(WITH_JEMALLOC_BUILD OFF)
set(JEMALLOC_LG_VADDR "")
set(JEMALLOC_LG_VADDR_MSG "")
set(JEMALLOC_LG_PAGE "")
set(JEMALLOC_LG_PAGE_MSG "")
set(WITH_GLOG_BUILD OFF)
set(WITH_GFLAGS_BUILD OFF)
set(WITH_DC_BUILD OFF)
set(WITH_LZ4_BUILD OFF)
set(WITH_FMT_BUILD OFF)
set(WITH_ZSTD_BUILD OFF)

set(IS_MUSL OFF)
set(IS_WINDOWS OFF)
set(IS_MSYS OFF)
set(IS_DARWIN OFF)

if(MSVC)
  set(IS_WINDOWS ON)
  set(OSTYPE_TXT "windows")
  set(OSTYPE_RES  0)
  set(USE_JEMALLOC OFF)
  set(VCPKG_PARAMS -GNinja -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET} -DVCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR})
  set(WITH_LINK_TESTS OFF)
  set(LIBEMPTY_NAME "empty.dll")
  set(MKDWARFS "${MKDWARFS}.exe")
  add_compile_options(/Zc:__cplusplus /utf-8 /wd4267 /wd4244 /wd5219)
  # Apply /MT or /MTd  (multithread, static version of the run-time library)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MINGW)
  set(IS_MSYS ON)
  set(OSTYPE_TXT "msys")
  set(OSTYPE_RES  0)
  set(USE_JEMALLOC OFF)
  set(PREFER_SYSTEM_GTEST ON)
  set(WITH_LINK_TESTS OFF)
  set(LIBEMPTY_NAME "libempty.dll")
  set(MKDWARFS "${MKDWARFS}.exe")
  set(WITH_FMT_BUILD ON)
else()
  set(LIBEMPTY_NAME "libempty.so")
  set(WITH_ZSTD_BUILD ON)
  execute_process(
    COMMAND "${GNU_BASH}"
            "-c"
            "echo \$OSTYPE"
    RESULT_VARIABLE OSTYPE_RES
    OUTPUT_VARIABLE OSTYPE_TXT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  if("${OSTYPE_TXT}" MATCHES "^linux-musl.*")
    set(IS_MUSL ON)
    # Needed for eaccess ( ~> File.readable?)
    set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Alpine 3.17++ lz4 & brotli libraries use lto and I did not find tool's configuration that works
    # (binutils-gold triggers issues with libstdc++)
      set(WITH_LZ4_BUILD ON)
      set(WITH_BROTLI_BUILD ON)
    endif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if ("${OPENSSL_VERSION}" EQUAL "1.1")
      include(${TOOLS}/cmake-scripts/setup-openssl-1.1.1.cmake)
      if (DEFINED OPENSSL_ROOT_DIR)
        set(OPENSSL_OPTION "-DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}")
        set(INSTALL_OPENSSL true)
      endif (DEFINED OPENSSL_ROOT_DIR)
    endif ("${OPENSSL_VERSION}" EQUAL "1.1")
  elseif("${OSTYPE_TXT}" MATCHES "^linux-gnu.*")
    # Needed for eaccess ( ~> File.readable?)
    set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  elseif("${OSTYPE_TXT}" MATCHES "^darwin.*")
    set(IS_DARWIN ON)
    message(STATUS "Building jemalloc library locally")
    set(WITH_JEMALLOC_BUILD ON)
    if(DEFINED ENV{LG_VADDR})
      set(JEMALLOC_LG_VADDR  --with-lg-vaddr=$ENV{LG_VADDR})
      set(JEMALLOC_LG_VADDR_MSG ", lg_vaddr = $ENV{LG_VADDR}")
      message(STATUS "Using LG_VADDR environment variable: ${JEMALLOC_LG_VADDR}")
    endif(DEFINED ENV{LG_VADDR})
    if(DEFINED ENV{LG_PAGE})
      set(JEMALLOC_LG_PAGE  --with-lg-page=$ENV{LG_PAGE})
      set(JEMALLOC_LG_PAGE_MSG ", lg_page = $ENV{LG_PAGE}")
      message(STATUS "Using LG_PAGE environment variable: ${JEMALLOC_LG_PAGE}")
    endif(DEFINED ENV{LG_PAGE})
    if ("${OPENSSL_VERSION}" EQUAL "1.1")
      set(OPENSSL_OPTION "-DOPENSSL_VERSION=${OPENSSL_VERSION}")
    endif("${OPENSSL_VERSION}" EQUAL "1.1")
  endif()
endif(MSVC)

# RB_W32
# -----------------------------------------------------------------------------
#  - windows (enforced)         will have IS_WINDOWS flag set
#    actually it is defined by compiler only (i.e.: MSVC) and my need revision
#    RB_W32 setting calls for Ruby style simulation of IO
#   (enable it by default when running on Windows)
#
#    RB_W32 means use of Ruby Linux-like Dir IO functions
#    Possible other methods may be MinGW Dir IO (== posix), folly portability (== posix)
#    These two most likely would work but are not tested
if(NOT IS_WINDOWS AND NOT IS_MSYS)
  set(RB_W32 OFF)
else(NOT IS_WINDOWS AND NOT IS_MSYS)
  set(RB_W32 ON)
endif(NOT IS_WINDOWS AND NOT IS_MSYS)
# -----------------------------------------------------------------------------
#  - "darwin.*"                 will have IS_DARWIN flag set
# -----------------------------------------------------------------------------
#  - "linux-musl.*"             will have IS_MUSL flag set
# -----------------------------------------------------------------------------
#  - ... everything else ...    will be be treated as linux-gnu but in reality
#                               only Ubuntu is supported/tested

if(OSTYPE_RES EQUAL 0)
  message(STATUS "OSTYPE: '${OSTYPE_TXT}'")
else(OSTYPE_RES EQUAL 0)
  message(FATAL_ERROR "Failed to detect OSTYPE: ${OSTYPE_TXT}")
endif(OSTYPE_RES EQUAL 0)

message("Configuraion summary:")
message(STATUS "Tebako build scope: ${TEBAKO_BUILD_SCOPE}")
if (IS_WINDOWS)
  message(STATUS "Building for Ruby Win32: ${RB_W32}")
endif(IS_WINDOWS)
message(STATUS "Use jemalloc: ${USE_JEMALLOC}${JEMALLOC_LG_VADDR_MSG}${JEMALLOC_LG_PAGE_MSG}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  with ASAN: ${WITH_ASAN}")
message(STATUS "  with coverage: ${WITH_COVERAGE}")
message(STATUS "  with tests: ${WITH_TESTS}")
if(WITH_TESTS)
  message(STATUS "    using mkdwarfs at: ${MKDWARFS}")
  message(STATUS "    test applications logging: ${TESTS_LOG_LEVEL}")
  message(STATUS "    with link tests: ${WITH_LINK_TESTS}")
endif(WITH_TESTS)
if(VCPKG_PARAMS)
  message(STATUS "Using vcpkg with: ${VCPKG_PARAMS}")
endif(VCPKG_PARAMS)

set(Boost_USE_STATIC_LIBS   ON)
set(Boost_LINK_STATIC       ON)

# If Boost was built using the boost-cmake project or from Boost 1.70.0 on it provides
# a package configuration file for use with find_package's config mode.
# This configuration  file enforces -lz -lbz2 -llzma and it is an issue for us on Alpine/clang
# See https://cmake.org/cmake/help/latest/module/FindBoost.html#boost-cmake for details
# set(Boost_NO_BOOST_CMAKE    ON)

set(USE_STATIC_DEPS_ON_UNIX ON)
set(GFLAGS_SHARED          OFF)
set(CMAKE_CXX_STANDARD      20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS   OFF)

# Find DwarFS via CMake package config (provided by vcpkg)
find_package(dwarfs CONFIG REQUIRED)

add_compile_definitions(GLOG_NO_ABBREVIATED_SEVERITIES NOMINMAX NOGDI)
add_compile_definitions(GLOG_USE_GLOG_EXPORT)

set(CXX_STD "gnu++20" CACHE STRING "The C++ standard argument to pass to the compiler.")
set(MSVC_LANGUAGE_VERSION "c++20" CACHE STRING "The C++ standard argument to pass to the compiler.")


if(CMAKE_BUILD_TYPE STREQUAL Debug)
# Mask folly symbolizer issue where it includes <dwarf.h> instead of <libdwarf/dwarf.h>
  file(WRITE ${DEPS_INCLUDE_DIR}/dwarf.h "#include <libdwarf/dwarf.h>")
endif(CMAKE_BUILD_TYPE STREQUAL Debug)



# ~~~~~~~~~~~~~~~~~~~~~~~~~ Library ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

set(__LIBDL "")
set(__LIBJEMALLOC "")
set(__LIBFMT "")
set(__LIBSSL "")
set(__LIBCRYPTO "")
set(__LIBBITSTRING "")
set(__LIBICUUC "")
set(__LIBICUI18N "")
set(__LIBICUDATA "")
set(__LIBZLIB "")
set(__LIBOPENSSL_ROOT "")
set(__LIBSYSTEM_ZLIB "")
set(__LIBANDROID_NDK "")
set(__LIBadaliba "")
set(_LIBEH "")
set(_LIBNAME "_LIB")

# System/dependency libraries for DwarFS
set(__LIBGLOG "${DEPS_LIB_DIR}/libglog.a")
set(__LIBGFLAGS "${DEPS_LIB_DIR}/libgflags.a")
set(__LIBBROTLICOMMON "${DEPS_LIB_DIR}/libbrotlicommon.a")
set(__LIBBROTLIDEC "${DEPS_LIB_DIR}/libbrotlidec.a")
set(__LIBBROTLIENC "${DEPS_LIB_DIR}/libbrotlienc.a")

# System libraries from Homebrew (macOS)
if(IS_DARWIN)
  set(__LIBSSL "/opt/homebrew/opt/openssl@3/lib/libssl.a")
  set(__LIBCRYPTO "/opt/homebrew/opt/openssl@3/lib/libcrypto.a")
  set(__LIBFLAC "/opt/homebrew/opt/flac/lib/libFLAC.a")
  set(__LIBFLACPP "/opt/homebrew/opt/flac/lib/libFLAC++.a")
  set(__LIBOGG "/opt/homebrew/opt/libogg/lib/libogg.a")
  set(__LIBLZ4 "/opt/homebrew/opt/lz4/lib/liblz4.a")
  set(__LIBLZMA "/opt/homebrew/opt/xz/lib/liblzma.a")
  set(__LIBFMT "/opt/homebrew/opt/fmt/lib/libfmt.a")
  # Boost libraries (filesystem and chrono for DwarFS)
  set(__LIBBOOST_FS "/opt/homebrew/opt/boost/lib/libboost_filesystem.a")
  set(__LIBBOOST_CHRONO "/opt/homebrew/opt/boost/lib/libboost_chrono.a")
endif(IS_DARWIN)

if(IS_WINDOWS)
  include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/include/incbin)
  list(PREPEND _LIBRARIES ${_LIBWINPTHREAD})
endif(IS_WINDOWS)

if(IS_TERMINAL)
  include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif(IS_TERMINAL)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  if(MSVC)
    add_compile_options(/MTd)
    add_link_options(/IGNORE:4221)
  endif(MSVC)
  if(IS_MSYS)
    add_link_options(/DEFAULTLIB:libz/gcc)
    add_link_options(/DEFAULTLIB:libgcc)
  endif(IS_MSYS)
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

if(INSTALL_OPENSSL AND WITH_CXX11_ABI)
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLE_PREFIX}SSL_ROOT )
endif(INSTALL_OPENSSL AND WITH_CXX11_ABI)

# CRITICAL: Build pure C library in subdirectory BEFORE global include_directories()
# This completely isolates it from C++ headers in the parent scope
add_subdirectory(src/c_helpers)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako
                    ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs
                    ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs/internal
                    ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs/util
                    ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs/ruby
                    ${DEPS}/include
                    ${DEPS}/include/libxml2-2.9.1
)

link_directories(BEFORE ${DEPS_LIB_DIR})

set(TFS_SOURCE_FILES
  src/backend_factory.cpp
  src/c_api.cpp
  src/tebako-cmdline.cpp
  src/tebako-dirent.cpp
  src/tebako-fd.cpp
  src/tebako-io-helpers.cpp
  src/tebako-io-root.cpp
  src/tebako-kfd.cpp
  src/tebako-memfs-table.cpp
  src/tebako-memfs.cpp
  src/tebako-mount-table.cpp
  src/tebako-package-descriptor.cpp
)

add_library(tfs STATIC
    "src/dl-ctl.cpp"
    "src/tebako-cmdline.cpp"
    "src/tebako-io-helpers.cpp"
    "src/tebako-io-root.cpp"
    "src/tebako-kfd.cpp"
    "src/tebako-mount-table.cpp"
    "src/tebako-memfs.cpp"
    "src/tebako-memfs-table.cpp"
    "src/tebako-fd.cpp"
    "src/tebako-dirent.cpp"
    "src/tebako-package-descriptor.cpp"
    "src/backend_factory.cpp"
    "src/backends/zip_backend.cpp"
    "src/backends/dwarfs_backend.cpp"
    "src/internal/memory_file_view.cpp"
    "src/internal/memory_file_segment.cpp"
    "src/core/path.cpp"
    "src/core/config.cpp"
    # "src/backends/squashfs_backend.cpp"  # TODO: Re-enable once squashfs-tools-ng is available
    # Modern C API implementation (replaces old file-ctl/dir-ctl/file-io/dir-io)
    "src/c_api.cpp"
    "src/c_api/fs_context.cpp"
    "src/backend_registry.cpp"
    "include/tebako-cmdline.h"
    "include/tebako-common.h"
    "include/tebako-config.h"
    "include/tebako-defines.h"
    "include/tebako-dirent.h"
    "include/tebako-fd.h"
    "include/tebako-io.h"
    "include/tebako-io-inner.h"
    "include/tebako-io-root.h"
    "include/tebako-kfd.h"
    "include/tebako-memfs.h"
    "include/tebako-memfs-table.h"
    "include/tebako-mount-table.h"
    "include/tebako-package-descriptor.h"
    "include/tebako-pch.h"
    "include/tebako-pch-pp.h"
    "include/version.h"
    "include/tebako/fs/backend_factory.h"
    "include/tebako/fs/backends/zip_backend.h"
    "include/tebako/fs/backends/dwarfs_backend.h"
    "include/tebako/fs/internal/memory_file_view.h"
    "include/tebako/fs/internal/memory_file_segment.h"
    # "include/tebako/fs/backends/squashfs_backend.h"  # TODO: Re-enable
    "include/tebako/fs/c_api.h"
    "include/tebako/fs/backend_registry.h"
  )

if(RB_W32)
  target_sources(tfs PUBLIC
    ""
    "include/tebako-io.h"
    "include/tebako-io-rb-w32-inner.h"
  )
endif(RB_W32)

# Link the pure C helper library to tfs
target_link_libraries(tfs PUBLIC tebako_dirent_helper_c)

# IMPORTANT: Add vcpkg include directories FIRST (BEFORE any system includes)
# This ensures vcpkg headers take precedence over Homebrew/system headers
# which is critical when both provide the same package (e.g., dwarfs)
# Use PRIVATE since this is only needed for compiling tfs, not for consumers
if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
  target_include_directories(tfs BEFORE PRIVATE
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
  )
endif()

# Find and link libzip (provided by vcpkg)
find_package(libzip CONFIG REQUIRED)
target_link_libraries(tfs PUBLIC libzip::zip)

# Link DwarFS libraries via CMake targets (provided by vcpkg)
target_link_libraries(tfs PUBLIC
  dwarfs::dwarfs_reader
  dwarfs::dwarfs_common
)

# Find and link squashfs-tools-ng (provided by vcpkg)
# TODO: Re-enable once squashfs-tools-ng is available
# find_package(squashfs-tools-ng CONFIG REQUIRED)
# target_link_libraries(tfs PUBLIC squashfs-tools-ng::squashfs)

# Find argtable3 for CLI tool
find_package(argtable3 CONFIG REQUIRED)

if(WITH_GLOG_BUILD)
  add_dependencies(tfs ${GLOG_PRJ})
endif(WITH_GLOG_BUILD)
if(WITH_GFLAGS_BUILD)
  add_dependencies(tfs ${GFLAGS_PRJ})
endif(WITH_GFLAGS_BUILD)
if(WITH_FMT_BUILD)
  add_dependencies(tfs ${FMT_PRJ})
endif(WITH_FMT_BUILD)
if(WITH_DC_BUILD)
  add_dependencies(tfs ${DC_PRJ})
endif(WITH_DC_BUILD)
if(WITH_LZ4_BUILD)
  add_dependencies(tfs ${LZ4_PRJ})
endif(WITH_LZ4_BUILD)
if(WITH_JEMALLOC_BUILD)
  add_dependencies(tfs ${JEMALLOC_PRJ})
endif(WITH_JEMALLOC_BUILD)

install(TARGETS
         tfs
         EXPORT libtfsTargets
         LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
         ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
         RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
         INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if (IS_MSYS)
  install(FILES ${__LIBFMT} DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY ${DEPS_INCLUDE_DIR}/fmt DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fmt)
elseif(NOT IS_WINDOWS)
  install(FILES ${__LIBFMT} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif(IS_MSYS)

if(WITH_GLOG_BUILD)
  install(FILES ${__LIBGLOG} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif(WITH_GLOG_BUILD)

if(WITH_GFLAGS_BUILD)
  install(FILES ${__LIBGFLAGS} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif(WITH_GFLAGS_BUILD)

if(WITH_DC_BUILD)
  install(FILES ${__LIBDC} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif(WITH_DC_BUILD)

if(WITH_BROTLI_BUILD)
  install(FILES ${__LIBBROTLICOMMON} ${__LIBBROTLIENC} ${__LIBBROTLIDEC} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif(WITH_BROTLI_BUILD)

if(WITH_LZ4_BUILD)
  install(FILES ${__LIBLZ4} DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif(WITH_LZ4_BUILD)

if(WITH_JEMALLOC_BUILD)
  install(FILES ${__LIBJEMALLOC} DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY ${DEPS_INCLUDE_DIR}/jemalloc DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif(WITH_JEMALLOC_BUILD)

if(INSTALL_OPENSSL)
  install(FILES ${DEPS_BIN_DIR}/openssl DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES ${__LIBSSL} DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ${__LIBCRYPTO} DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY ${DEPS_INCLUDE_DIR}/openssl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif(INSTALL_OPENSSL)


install(FILES
        "include/tebako-config.h"
        "include/tebako-defines.h"
        "include/tebako-io.h"
        "include/tebako-cmdline.h"
        "include/tebako-package-descriptor.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tebako
)

if(RB_W32)
  install(FILES
          "include/tebako-io-rb-w32.h"
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tebako
  )
endif(RB_W32)

if(IS_WINDOWS)
  install(FILES
          ${DEPS_BIN_DIR}/incbin.exe
          DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif(IS_WINDOWS)

install(FILES
        ${MKDWARFS}
        DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install CMake package config files for vcpkg integration
include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libtfsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/libtfsConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libtfs"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/libtfsConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/libtfsConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/libtfsConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libtfs"
)

install(EXPORT libtfsTargets
  FILE libtfsTargets.cmake
  NAMESPACE libtfs::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libtfs"
)

# Install public headers for libtfs API
install(DIRECTORY
  "include/tebako/fs"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tebako
  FILES_MATCHING PATTERN "*.h"
  PATTERN "internal" EXCLUDE
  PATTERN "ruby" EXCLUDE
)

if(IS_WINDOWS OR IS_MSYS)
#  include(InstallRequiredSystemLibraries)

  string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1;\\2;\\3" VERSION_COMPONENTS ${LIBDWARFS_WR_VERSION})

  list(GET VERSION_COMPONENTS 0 VERSION_MAJOR)
  list(GET VERSION_COMPONENTS 1 VERSION_MINOR)
  list(GET VERSION_COMPONENTS 2 VERSION_PATCH)

  set(CPACK_PACKAGE_NAME "libtfs")
  set(CPACK_PACKAGE_VENDOR "Ribose Inc.")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tamatebako libdwarfs wrapper package")
  set(CPACK_PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
  set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
  set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
  set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "libdwarfs ${CPACK_PACKAGE_VERSION}")

  if (IS_MSYS)
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-mingw-ucrt64")
  else (IS_MSYS)
  endif (IS_MSYS)

  set(CPACK_GENERATOR "7Z")
  include(CPack)
endif(IS_WINDOWS OR IS_MSYS)

# ~~~~~~~~~~~~~~~~~~~~~~~~~ Tests ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

if (WITH_TESTS)
  include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  include(CTest)
  include(GoogleTest)

  # Find GTest using modern CMake
  find_package(GTest REQUIRED)

  if(RB_W32)
    enable_testing()
    add_subdirectory(tests)

    add_dependencies(wr-bin ${INCBIN_PRJ} PACKAGEDFileSYSTEM_STEP_3)
  endif(RB_W32)

  # Backend factory tests
  add_executable(test_backend_factory
    "tests/test_backend_factory.cpp"
  )
  target_link_libraries(test_backend_factory PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
  )
  gtest_add_tests(TARGET test_backend_factory)

  # ZIP Backend tests
  add_executable(test_zip_backend
    "tests/test_zip_backend.cpp"
  )
  target_link_libraries(test_zip_backend PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
  )
  gtest_add_tests(TARGET test_zip_backend)

  # ZIP Integration tests
  add_executable(test_zip_integration
    "tests/test_zip_integration.cpp"
  )
  target_link_libraries(test_zip_integration PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
  )
  gtest_add_tests(TARGET test_zip_integration)

  # DwarFS Backend tests
  add_executable(test_dwarfs_backend
    "tests/test_dwarfs_backend.cpp"
  )
  target_link_libraries(test_dwarfs_backend PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
    dwarfs::dwarfs_reader
    dwarfs::dwarfs_common
  )
  gtest_add_tests(TARGET test_dwarfs_backend)

  # DwarFS Integration tests
  add_executable(test_dwarfs_integration
    "tests/test_dwarfs_integration.cpp"
  )
  target_link_libraries(test_dwarfs_integration PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
    dwarfs::dwarfs_reader
    dwarfs::dwarfs_common
  )
  gtest_add_tests(TARGET test_dwarfs_integration)

  # SquashFS Backend tests
  # TODO: Re-enable once squashfs-tools-ng is available
  # add_executable(test_squashfs_backend
  #   "tests/test_squashfs_backend.cpp"
  # )
  # target_compile_options(test_squashfs_backend PUBLIC ${GTEST_CFLAGS})
  # target_link_libraries(test_squashfs_backend tfs ${GTestMain} ${GTEST_LDFLAGS})
  # gtest_add_tests(TARGET test_squashfs_backend)

  # SquashFS Integration tests
  # TODO: Re-enable once squashfs-tools-ng is available
  # add_executable(test_squashfs_integration
  #   "tests/test_squashfs_integration.cpp"
  # )
  # target_compile_options(test_squashfs_integration PUBLIC ${GTEST_CFLAGS})
  # target_link_libraries(test_squashfs_integration tfs ${GTestMain} ${GTEST_LDFLAGS})
  # gtest_add_tests(TARGET test_squashfs_integration)

  # C API tests
  add_executable(test_c_api
    "tests/test_c_api.cpp"
  )
  target_link_libraries(test_c_api PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
    dwarfs::dwarfs_reader
    dwarfs::dwarfs_common
  )
  gtest_add_tests(TARGET test_c_api TEST_LIST c_api_tests)
  # C API tests share global state and must not run in parallel
  set_tests_properties(${c_api_tests} PROPERTIES RESOURCE_LOCK "capi_global_state")

  # Extraction API tests
  add_executable(test_extraction
    "tests/test_extraction.cpp"
  )
  target_link_libraries(test_extraction PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
  )
  gtest_add_tests(TARGET test_extraction TEST_LIST extraction_tests)
  # Extraction tests also use C API global state and must not run in parallel with C API tests
  set_tests_properties(${extraction_tests} PROPERTIES RESOURCE_LOCK "capi_global_state")

  # Result<T, Error> type tests
  add_executable(test_result
    "tests/test_result.cpp"
  )
  target_link_libraries(test_result PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
  )
  gtest_add_tests(TARGET test_result)

  # Path class tests
  add_executable(test_path
    "tests/test_path.cpp"
    "src/core/path.cpp"
  )
  target_include_directories(test_path PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(test_path PRIVATE
    GTest::gtest
    GTest::gtest_main
  )
  gtest_add_tests(TARGET test_path)

  # LRU Cache tests
  add_executable(test_lru_cache
    "tests/test_lru_cache.cpp"
  )
  target_include_directories(test_lru_cache PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(test_lru_cache PRIVATE
    GTest::gtest
    GTest::gtest_main
  )
  gtest_add_tests(TARGET test_lru_cache)

  # Unified interface tests
  add_executable(test_unified_interface
    "tests/test_unified_interface.cpp"
  )
  target_link_libraries(test_unified_interface PRIVATE
    tfs
    tebako_dirent_helper_c
    GTest::gtest
    GTest::gtest_main
  )
  gtest_add_tests(TARGET test_unified_interface)

  # Copy test fixtures to build directory
  file(COPY tests/fixtures/zip DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures)
  file(COPY tests/fixtures/dwarfs DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures)
  # file(COPY tests/fixtures/squashfs DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures)  # TODO: Re-enable

endif(WITH_TESTS)

# ~~~~~~~~~~~~~~~~~~~~~~~~~ CLI Tool ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# tebakofs CLI tool
add_executable(tebakofs
  "src/tebakofs_main.cpp"
  "src/cli/tebakofs.cpp"
  "include/tebako/fs/cli/tebakofs.h"
)

target_include_directories(tebakofs PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/tebako/fs
)

target_link_libraries(tebakofs PRIVATE
  tfs
  argtable3::argtable3
)

install(TARGETS tebakofs DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT tebakofs)
