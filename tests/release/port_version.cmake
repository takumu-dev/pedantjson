# Fails unless the vcpkg port's manifest MANIFEST gives VERSION, the project's, or the release
# before it in the changelog CHANGELOG. The port downloads the release it names, so it can follow a
# release only once the release is tagged, and must not fall further behind than that.
cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/changelog.cmake")

pedantjson_changelog_releases(releases "${CHANGELOG}")
file(READ "${MANIFEST}" manifest)
string(JSON port_version GET "${manifest}" version)

set(expected "${VERSION}")
list(FIND releases "${VERSION}" index)
if(index GREATER_EQUAL 0)
    math(EXPR index "${index} + 1")
    list(LENGTH releases count)
    if(index LESS count)
        list(GET releases ${index} before)
        list(APPEND expected "${before}")
    endif()
endif()
if(NOT port_version IN_LIST expected)
    list(JOIN expected " or " versions)
    message(FATAL_ERROR "${MANIFEST} gives version ${port_version}, not ${versions}")
endif()
