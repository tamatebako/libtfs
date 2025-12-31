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

# Examples CMakeLists.txt

message(STATUS "Configuring examples")

# Helper function to add DwarFS libraries (same as in main CMakeLists.txt)
function(ADD_EXAMPLE_DWARFS_LIBRARY NAME PRJ APP_NM)
  add_library(${NAME}_example STATIC IMPORTED)
  set_target_properties(${NAME}_example PROPERTIES IMPORTED_LOCATION ${_${NAME}})
  add_dependencies(${NAME}_example ${PRJ})
  target_link_libraries(${APP_NM} ${NAME}_example)
endfunction()

function(ADD_EXAMPLE_DWARFS_LIBRARY_W NAME PRJ APP_NM)
  add_library(${NAME}_example STATIC IMPORTED)
  set_target_properties(${NAME}_example PROPERTIES IMPORTED_LOCATION ${_${NAME}})
  add_dependencies(${NAME}_example ${PRJ})
  target_link_libraries(${APP_NM} "$<LINK_LIBRARY:WHOLE_ARCHIVE,${NAME}_example>")
endfunction()

# Basic usage example
add_executable(basic_usage
  basic_usage.cpp
)

# Comprehensive API example
add_executable(api_example
  api_example.cpp
)

# Link both examples against dwarfs-wr
target_link_libraries(basic_usage dwarfs-wr)
target_link_libraries(api_example dwarfs-wr)

# Add dependencies (same libraries as test applications)
add_example_dwarfs_library(_LIBDWARFS ${DWARFS_PRJ} basic_usage)
add_example_dwarfs_library_w(_LIBDWARFS_COMPRESSION ${DWARFS_PRJ} basic_usage)
add_example_dwarfs_library(_LIBFSST ${DWARFS_PRJ} basic_usage)
add_example_dwarfs_library(_LIBT_METADATA ${DWARFS_PRJ} basic_usage)
add_example_dwarfs_library(_LIBT_LIGHT ${DWARFS_PRJ} basic_usage)
add_example_dwarfs_library(_LIBARCHIVE ${DWARFS_PRJ} basic_usage)
add_example_dwarfs_library(_LIBXXHASH ${DWARFS_PRJ} basic_usage)
add_example_dwarfs_library(_LIBZSTD ${DWARFS_PRJ} basic_usage)
add_example_dwarfs_library(_LIBFMT ${DWARFS_PRJ} basic_usage)

add_example_dwarfs_library(_LIBDWARFS ${DWARFS_PRJ} api_example)
add_example_dwarfs_library_w(_LIBDWARFS_COMPRESSION ${DWARFS_PRJ} api_example)
add_example_dwarfs_library(_LIBFSST ${DWARFS_PRJ} api_example)
add_example_dwarfs_library(_LIBT_METADATA ${DWARFS_PRJ} api_example)
add_example_dwarfs_library(_LIBT_LIGHT ${DWARFS_PRJ} api_example)
add_example_dwarfs_library(_LIBARCHIVE ${DWARFS_PRJ} api_example)
add_example_dwarfs_library(_LIBXXHASH ${DWARFS_PRJ} api_example)
add_example_dwarfs_library(_LIBZSTD ${DWARFS_PRJ} api_example)
add_example_dwarfs_library(_LIBFMT ${DWARFS_PRJ} api_example)

# Link external libraries for both examples
if(WITH_GLOG_BUILD)
  add_example_dwarfs_library(_LIBGLOG ${GLOG_PRJ} basic_usage)
  add_example_dwarfs_library(_LIBGLOG ${GLOG_PRJ} api_example)
else(WITH_GLOG_BUILD)
  target_link_libraries(basic_usage ${_LIBGLOG})
  target_link_libraries(api_example ${_LIBGLOG})
endif(WITH_GLOG_BUILD)

if(WITH_GFLAGS_BUILD)
  add_example_dwarfs_library(_LIBGFLAGS ${GFLAGS_PRJ} basic_usage)
  add_example_dwarfs_library(_LIBGFLAGS ${GFLAGS_PRJ} api_example)
else(WITH_GFLAGS_BUILD)
  target_link_libraries(basic_usage ${_LIBGFLAGS})
  target_link_libraries(api_example ${_LIBGFLAGS})
endif(WITH_GFLAGS_BUILD)

if(WITH_DC_BUILD)
  add_example_dwarfs_library(_LIBDC ${DC_PRJ} basic_usage)
  add_example_dwarfs_library(_LIBDC ${DC_PRJ} api_example)
else(WITH_DC_BUILD)
  target_link_libraries(basic_usage ${_LIBDC})
  target_link_libraries(api_example ${_LIBDC})
endif(WITH_DC_BUILD)

if(WITH_BROTLI_BUILD)
  add_example_dwarfs_library(_LIBBROTLIENC ${BROTLI_PRJ} basic_usage)
  add_example_dwarfs_library(_LIBBROTLIDEC ${BROTLI_PRJ} basic_usage)
  add_example_dwarfs_library(_LIBBROTLICOMMON ${BROTLI_PRJ} basic_usage)
  add_example_dwarfs_library(_LIBBROTLIENC ${BROTLI_PRJ} api_example)
  add_example_dwarfs_library(_LIBBROTLIDEC ${BROTLI_PRJ} api_example)
  add_example_dwarfs_library(_LIBBROTLICOMMON ${BROTLI_PRJ} api_example)
elseif(NOT IS_MSYS)
  target_link_libraries(basic_usage ${_LIBBROTLIENC} ${_LIBBROTLIDEC} ${_LIBBROTLICOMMON})
  target_link_libraries(api_example ${_LIBBROTLIENC} ${_LIBBROTLIDEC} ${_LIBBROTLICOMMON})
endif(WITH_BROTLI_BUILD)

if(WITH_LZ4_BUILD)
  add_example_dwarfs_library(_LIBLZ4 ${LZ4_PRJ} basic_usage)
  add_example_dwarfs_library(_LIBLZ4 ${LZ4_PRJ} api_example)
else(WITH_LZ4_BUILD)
  target_link_libraries(basic_usage ${_LIBLZ4})
  target_link_libraries(api_example ${_LIBLZ4})
endif(WITH_LZ4_BUILD)

# Link Boost and other system libraries
target_link_libraries(basic_usage ${_LIBSSL} ${_LIBCRYPTO} ${Boost_LIBRARIES} ${_LIBEVENT})
target_link_libraries(api_example ${_LIBSSL} ${_LIBCRYPTO} ${Boost_LIBRARIES} ${_LIBEVENT})

# Platform-specific libraries
if(IS_DARWIN)
  target_link_libraries(basic_usage ${_LIBZ} ${_LIBLZMA} c++abi)
  target_link_libraries(api_example ${_LIBZ} ${_LIBLZMA} c++abi)
else(IS_DARWIN)
  target_link_libraries(basic_usage ${_LIBIBERTY} ${_LIBACL} ${_LIBZ} ${_LIBEH})
  target_link_libraries(api_example ${_LIBIBERTY} ${_LIBACL} ${_LIBZ} ${_LIBEH})
  if(IS_MSYS)
    target_link_libraries(basic_usage ${_LIBLZMA} ${_LIBDL})
    target_link_libraries(api_example ${_LIBLZMA} ${_LIBDL})
    target_link_options(basic_usage PUBLIC -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive)
    target_link_options(api_example PUBLIC -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive)
  elseif(NOT IS_WINDOWS)
    target_link_libraries(basic_usage ${_LIBUNWIND} ${_LIBLZMA} pthread dl)
    target_link_libraries(api_example ${_LIBUNWIND} ${_LIBLZMA} pthread dl)
  else()
    target_link_libraries(basic_usage dl)
    target_link_libraries(api_example dl)
  endif(IS_MSYS)
endif(IS_DARWIN)

# jemalloc if enabled
if(WITH_JEMALLOC_BUILD)
  add_example_dwarfs_library(_LIBJEMALLOC ${JEMALLOC_PRJ} basic_usage)
  add_example_dwarfs_library(_LIBJEMALLOC ${JEMALLOC_PRJ} api_example)
elseif(USE_JEMALLOC)
  target_link_libraries(basic_usage ${_LIBJEMALLOC})
  target_link_libraries(api_example ${_LIBJEMALLOC})
endif(WITH_JEMALLOC_BUILD)

# Windows-specific libraries
if(IS_WINDOWS OR IS_MSYS)
  target_link_libraries(basic_usage crypt32 shlwapi wsock32 ws2_32)
  target_link_libraries(api_example crypt32 shlwapi wsock32 ws2_32)
endif(IS_WINDOWS OR IS_MSYS)

message(STATUS "Examples configured successfully")
message(STATUS "  - basic_usage: Basic mount, read, unmount example")
message(STATUS "  - api_example: Comprehensive API demonstration")