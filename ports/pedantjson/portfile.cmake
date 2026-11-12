vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO takumu-dev/pedantjson
    REF "${VERSION}"
    SHA512 33ca4eac56bf8ddaa9942d469556df23e461a122f99525914638e0d73f90d349adaacf29f241c65d1a9a39f080c81a6074be5fc5d942e8b2e48a95e31800d3b4
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
