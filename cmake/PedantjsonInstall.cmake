# Installs the header, the static library, a CMake package for `find_package(pedantjson)`, which
# gives the target pedantjson::pedantjson, and a pkg-config file for builds that don't use CMake.

include_guard(GLOBAL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(PEDANTJSON_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/pedantjson")

install(TARGETS pedantjson
    EXPORT pedantjson-targets
    ARCHIVE
    FILE_SET HEADERS
)
install(EXPORT pedantjson-targets
    NAMESPACE pedantjson::
    DESTINATION "${PEDANTJSON_INSTALL_CMAKEDIR}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/pedantjson-config.cmake.in"
    "${PROJECT_BINARY_DIR}/pedantjson-config.cmake"
    INSTALL_DESTINATION "${PEDANTJSON_INSTALL_CMAKEDIR}"
)
# Before 1.0, a minor release may break the interface.
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/pedantjson-config-version.cmake"
    COMPATIBILITY SameMinorVersion
)
install(FILES
    "${PROJECT_BINARY_DIR}/pedantjson-config.cmake"
    "${PROJECT_BINARY_DIR}/pedantjson-config-version.cmake"
    DESTINATION "${PEDANTJSON_INSTALL_CMAKEDIR}"
)

# The pkg-config file finds the prefix from its own place, so that it stays right when the prefix
# is chosen at install time, as `cmake --install --prefix` does, or the installed files move.
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(PEDANTJSON_PC_PREFIX "${CMAKE_INSTALL_PREFIX}")
else()
    file(RELATIVE_PATH up "/${CMAKE_INSTALL_LIBDIR}/pkgconfig" "/")
    string(REGEX REPLACE "/$" "" up "${up}")
    set(PEDANTJSON_PC_PREFIX "\${pcfiledir}/${up}")
endif()
foreach(dir IN ITEMS LIBDIR INCLUDEDIR)
    if(IS_ABSOLUTE "${CMAKE_INSTALL_${dir}}")
        set(PEDANTJSON_PC_${dir} "${CMAKE_INSTALL_${dir}}")
    else()
        set(PEDANTJSON_PC_${dir} "\${prefix}/${CMAKE_INSTALL_${dir}}")
    endif()
endforeach()
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/pedantjson.pc.in"
    "${PROJECT_BINARY_DIR}/pedantjson.pc"
    @ONLY
)
install(FILES "${PROJECT_BINARY_DIR}/pedantjson.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
