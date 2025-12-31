vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO AgentD/squashfs-tools-ng
    REF "v${VERSION}"
    SHA512 ef5f71f1f0487e0835e5d2c97c6e8e1f9c5c4e8f9e8d5e8f9e8d5e8f9e8d5e8f9e8d5e8f9e8d5e8f9e8d5e8f9e8d5e8f9e8d5e8f9e8d5e8f9e8d5e8f9e8d5e8f
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_SHARED_LIBS=OFF
        -DWITH_LZO=OFF
        -DWITH_LZ4=ON
        -DWITH_XZ=ON
        -DWITH_ZSTD=ON
        -DWITH_ZLIB=ON
        -DWITH_PTHREAD=ON
        -DBUILD_TOOLS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/squashfs-tools-ng)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE.md"
     DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
     RENAME copyright)