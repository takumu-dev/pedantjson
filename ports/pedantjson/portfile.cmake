vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO takumu-dev/pedantjson
    REF "${VERSION}"
    SHA512 1aa74e7d5f1c8d909ede684ef135de13b65cd3db7d88a91e7da8ee3de3c8fd56fc197b36b7be7e5cb047bfcd8db59b4c6dfb1ac6c39fbcfa41e6db2c5ac1bb8d
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
