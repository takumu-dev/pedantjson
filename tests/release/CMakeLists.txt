# Checks that keep a release's version, its changelog and the vcpkg port in step.

add_test(NAME "release: the changelog's newest release is the project's version"
    COMMAND "${CMAKE_COMMAND}"
        "-DCHANGELOG=${PROJECT_SOURCE_DIR}/CHANGELOG.md"
        "-DVERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/newest_release.cmake"
)

add_test(NAME "release: the vcpkg port builds the project's version or the release before it"
    COMMAND "${CMAKE_COMMAND}"
        "-DCHANGELOG=${PROJECT_SOURCE_DIR}/CHANGELOG.md"
        "-DMANIFEST=${PROJECT_SOURCE_DIR}/ports/pedantjson/vcpkg.json"
        "-DVERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/port_version.cmake"
)
