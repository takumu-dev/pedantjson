# Runs the portfile of ports/pedantjson on the checkout this file sits in, rather than on the
# release that portfile downloads, so that CI builds each change through the port before it is
# released. Everything but the download is the port's own.
cmake_path(SET checkout_dir NORMALIZE "${CURRENT_PORT_DIR}/../../..")

# vcpkg defines vcpkg_from_github before it runs a portfile, so this definition replaces it.
function(vcpkg_from_github)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "OUT_SOURCE_PATH" "")
    set("${arg_OUT_SOURCE_PATH}" "${checkout_dir}" PARENT_SCOPE)
endfunction()

include("${checkout_dir}/ports/pedantjson/portfile.cmake")
