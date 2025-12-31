vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/libdwarfs
    REF v${VERSION}
    SHA512 0  # Will be updated
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWITH_TESTS=OFF
        -DPREFER_SYSTEM_GTEST=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME libtfs CONFIG_PATH lib/cmake/libtfs)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)