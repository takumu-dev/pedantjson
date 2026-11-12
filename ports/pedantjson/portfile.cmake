vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO takumu-dev/pedantjson
    REF "${VERSION}"
    SHA512 7212490a2eeea5f1691fe2d9a5a149ed205df0efffa610a8b0c679a3502f314ba2997cd657d6ab2f2d0bccdcf69308c555ebc39fd59b2c59ff7a21e4208e1626
    HEAD_REF develop
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPEDANTJSON_BUILD_FUZZER=OFF
        -DPEDANTJSON_BUILD_TESTS=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/pedantjson)
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
