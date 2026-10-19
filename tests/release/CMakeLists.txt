# Checks that keep a release's version and its changelog in step.

add_test(NAME "release: the changelog's newest release is the project's version"
    COMMAND "${CMAKE_COMMAND}"
        "-DCHANGELOG=${PROJECT_SOURCE_DIR}/CHANGELOG.md"
        "-DVERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/newest_release.cmake"
)
