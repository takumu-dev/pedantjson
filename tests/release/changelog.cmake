# Reads the changelog CHANGELOG.md, for the release checks.

include_guard(GLOBAL)

# Sets `out` to the versions of the releases in `changelog`, newest first. Fails unless the
# changelog takes Keep a Changelog's form: its sections are an Unreleased one, then one for each
# release, newest first, headed with its version and its date.
function(pedantjson_changelog_releases out changelog)
    file(STRINGS "${changelog}" headings REGEX "^## ")
    list(POP_FRONT headings first)
    if(NOT first STREQUAL "## [Unreleased]")
        message(FATAL_ERROR "${changelog} begins with \"${first}\", not \"## [Unreleased]\"")
    endif()
    set(releases "")
    set(newer "")
    foreach(heading IN LISTS headings)
        if(NOT heading MATCHES
            "^## \\[([0-9]+\\.[0-9]+\\.[0-9]+)\\] - [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$"
        )
            message(FATAL_ERROR
                "${changelog} has a section headed \"${heading}\", which isn't a version and a date"
            )
        endif()
        set(version "${CMAKE_MATCH_1}")
        if(newer AND NOT newer VERSION_GREATER version)
            message(FATAL_ERROR "${changelog} lists ${version} after ${newer}, not before it")
        endif()
        list(APPEND releases "${version}")
        set(newer "${version}")
    endforeach()
    set("${out}" "${releases}" PARENT_SCOPE)
endfunction()
