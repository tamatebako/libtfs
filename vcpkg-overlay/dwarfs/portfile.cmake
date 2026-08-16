# vcpkg portfile for dwarfs
# DwarFS - A fast high-compression read-only file system
# Source: tamatebako/dwarfs-t fork, tag v1.0.0 (commit d9ebfef7; ships the
# stable C ABI reader binding libdwarfs_c / dwarfs_c.h, installed via
# dwarfs-targets).
#
# PIN CONTRACT (owner decision 2026-08-16): pin TAGS of dwarfs-t, never
# bare commit shas. A pre-merge PR-branch sha can be orphaned by a
# rebase-merge and garbage-collected by GitHub — the 1a43690c pin's
# archive tarball started 404ing and took every CI leg down with it
# (~3 weeks of red main). Tags are immutable anchors: a tag never moves,
# a new release is a new tag. The pin-guard workflow enforces this at PR
# time (REF must be an existing dwarfs-t tag, or — exceptionally — a sha
# proven reachable from dwarfs-t main).

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

# Link the C++ runtime statically on Linux so the shipped tools (mkdwarfs,
# dwarfsck, dwarfsextract) run on hosts whose libstdc++/libgcc are older than
# the build container's (tebako consumes these binaries in its CI images and
# on user machines; a dynamic C++ runtime made musl binaries fail with
# "Error relocating ... _M_replace_cold: symbol not found").
set(__DWARFS_EXE_LINKER_OPTIONS "")
if(VCPKG_TARGET_IS_LINUX)
    set(__DWARFS_EXE_LINKER_OPTIONS "-DCMAKE_EXE_LINKER_FLAGS=-static-libstdc++ -static-libgcc")
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/dwarfs-t
    REF v1.0.0
    SHA512 a0c70dd535cdcc3ad0967600fe41b21f52020b2f13a2a2c4b424f13019c22fd8c41a4a6e84a9717380bfe1dd11ea5658f0680cb614ae5ea789c0a5fe08e15051
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    OPTIONS
        -DWITH_TESTS=OFF
        -DWITH_LIBDWARFS=ON
        -DWITH_TOOLS=ON
        -DWITH_FUSE_DRIVER=OFF
        # shipped artifacts are libs + binaries only; man pages need the
        # mistletoe Python module that minimal build containers do not have.
        # WITH_MAN_PAGES covers the standalone .1 files, WITH_MAN_OPTION the
        # text embedded in the tools (--man) — both must be off.
        -DWITH_MAN_PAGES=OFF
        -DWITH_MAN_OPTION=OFF
        # tarball builds have no git metadata; version.cmake's source-build override
        -DNIXPKGS_DWARFS_VERSION_OVERRIDE=v0.14.1
        # need_fuse.cmake is included unconditionally and FATAL_ERRORs without FUSE-T;
        # WITH_FUSE_DRIVER=OFF does not guard it
        -DDWARFS_WITH_FUSE=OFF
        # keep the port hermetic; host Homebrew flac must not leak in
        -DTRY_ENABLE_FLAC=OFF
        # keep consumers hermetic; stops a brew probe leaking into dwarfs-config.cmake
        -DUSE_HOMEBREW_LIBARCHIVE=OFF
        ${__DWARFS_EXE_LINKER_OPTIONS}
)

vcpkg_cmake_install()

# Manually move CMake config files from lib/cmake to share
# (vcpkg_cmake_config_fixup requires both release and debug, but we only have release)
if(EXISTS "${CURRENT_PACKAGES_DIR}/lib/cmake/dwarfs")
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/share")
    file(RENAME
        "${CURRENT_PACKAGES_DIR}/lib/cmake/dwarfs"
        "${CURRENT_PACKAGES_DIR}/share/dwarfs")

    # Fix the config file paths
    file(READ "${CURRENT_PACKAGES_DIR}/share/dwarfs/dwarfs-config.cmake" CONFIG_CONTENT)
    string(REPLACE
        "\${PACKAGE_PREFIX_DIR}/lib/cmake/dwarfs"
        "\${PACKAGE_PREFIX_DIR}/share/dwarfs"
        CONFIG_CONTENT
        "${CONFIG_CONTENT}")
    file(WRITE "${CURRENT_PACKAGES_DIR}/share/dwarfs/dwarfs-config.cmake" "${CONFIG_CONTENT}")
endif()

# Remove debug cmake if it exists
if(EXISTS "${CURRENT_PACKAGES_DIR}/debug/lib/cmake")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/lib/cmake")
endif()

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)
file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/share)

# Install generated flatbuffers headers (created during build)
# These are generated in CMAKE_BINARY_DIR and need special handling
file(GLOB_RECURSE GEN_FB_HEADERS
     "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/include/dwarfs/gen-flatbuffers/*.h")
foreach(HEADER ${GEN_FB_HEADERS})
    file(RELATIVE_PATH REL_HEADER
         "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/include"
         "${HEADER}")
    get_filename_component(HEADER_DIR "${REL_HEADER}" DIRECTORY)
    file(INSTALL "${HEADER}"
         DESTINATION "${CURRENT_PACKAGES_DIR}/include/${HEADER_DIR}")
endforeach()

vcpkg_fixup_pkgconfig()

# Create phmap config for header-only parallel-hashmap
# This is needed because dwarfs targets reference phmap
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/share/phmap")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/phmap/phmap-config.cmake" "
include(CMakeFindDependencyMacro)
find_dependency(Threads REQUIRED)

add_library(phmap INTERFACE IMPORTED)
set_target_properties(phmap PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES \"\${PACKAGE_PREFIX_DIR}/include\"
)
")

# Update dwarfs-config.cmake to find phmap before including targets
file(READ "${CURRENT_PACKAGES_DIR}/share/dwarfs/dwarfs-config.cmake" CONFIG_CONTENT)
# Add phmap find_dependency after Threads
string(REPLACE
    "find_dependency(Threads REQUIRED)"
    "find_dependency(Threads REQUIRED)\nfind_dependency(phmap CONFIG)"
    CONFIG_CONTENT
    "${CONFIG_CONTENT}")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/dwarfs/dwarfs-config.cmake" "${CONFIG_CONTENT}")

# Install license using proper vcpkg function
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.GPL-3.0")
