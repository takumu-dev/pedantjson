# Builds pedantjson from the source tree this port sits in, so that CI builds each change through
# vcpkg as a project using the port would. vcpkg's binary caching knows the port's files but not
# the source tree, so a build through this port runs with caching off.
cmake_path(GET CURRENT_PORT_DIR PARENT_PATH ports_dir)
cmake_path(GET ports_dir PARENT_PATH SOURCE_PATH)

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
