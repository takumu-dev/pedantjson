# Fails unless the newest release in the changelog CHANGELOG is VERSION, the project's, so that a
# release can't raise the version without dating its changes, nor date them without raising it.
# Before the first release, the changelog lists none and the version is 0.0.0.
cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/changelog.cmake")

pedantjson_changelog_releases(releases "${CHANGELOG}")
if(NOT releases)
    if(NOT VERSION STREQUAL "0.0.0")
        message(FATAL_ERROR "${CHANGELOG} lists no release, and project() gives ${VERSION}")
    endif()
else()
    list(GET releases 0 newest)
    if(NOT newest STREQUAL VERSION)
        message(FATAL_ERROR
            "${CHANGELOG}'s newest release is ${newest}, and project() gives ${VERSION}"
        )
    endif()
endif()
