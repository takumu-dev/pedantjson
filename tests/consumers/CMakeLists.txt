# The ways in for a project: the installed package, through CMake or pkg-config, and the subproject.
# Each consumer is a project of its own, which CTest configures, builds and runs with this build's
# compiler and flags.

set(prefix "${CMAKE_CURRENT_BINARY_DIR}/prefix")
set(builds "${CMAKE_CURRENT_BINARY_DIR}/builds")

# The flags of this build's type go too, since a preset may replace the defaults, as MSVC's with
# AddressSanitizer keeps assertions on in an optimised build.
set(config_flags "")
if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" config)
    set(config_flags
        "-DCMAKE_CXX_FLAGS_${config}=${CMAKE_CXX_FLAGS_${config}}"
        "-DCMAKE_EXE_LINKER_FLAGS_${config}=${CMAKE_EXE_LINKER_FLAGS_${config}}"
    )
endif()

# Every run starts from nothing, so that no file installed and no cache written by an earlier run
# can stand in for what this one should make.
add_test(NAME "consumers: earlier installs and consumer builds are removed"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${prefix}" "${builds}"
)
set_tests_properties("consumers: earlier installs and consumer builds are removed" PROPERTIES
    FIXTURES_SETUP pedantjson_consumers_cleared
)

# Adds a test that builds the consumer project in `source_dir` and runs the program it builds.
#
#   INSTALLED  the consumer needs the installed library
#   OPTIONS    more cache settings for the consumer
function(pedantjson_add_consumer_test name source_dir)
    cmake_parse_arguments(PARSE_ARGV 2 arg "INSTALLED" "" "OPTIONS")
    set(test_name "consumers: ${name}")
    if(arg_INSTALLED)
        list(APPEND arg_OPTIONS "-DCMAKE_PREFIX_PATH=${prefix}")
    endif()
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}" "${builds}/${source_dir}"
            --build-generator "${CMAKE_GENERATOR}"
            --build-makeprogram "${CMAKE_MAKE_PROGRAM}"
            --build-config $<CONFIG>
            --build-options
                -DCMAKE_BUILD_TYPE=$<CONFIG>
                "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
                "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
                "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
                ${config_flags}
                ${arg_OPTIONS}
            --test-command consumer
    )
    set_tests_properties("${test_name}" PROPERTIES FIXTURES_REQUIRED pedantjson_consumers_cleared)
    if(arg_INSTALLED)
        set_tests_properties("${test_name}" PROPERTIES
            FIXTURES_REQUIRED "pedantjson_consumers_cleared;pedantjson_installed"
        )
    endif()
endfunction()

if(PEDANTJSON_INSTALL)
    add_test(NAME "consumers: the library installs"
        COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}" --prefix "${prefix}"
            --config $<CONFIG>
    )
    set_tests_properties("consumers: the library installs" PROPERTIES
        FIXTURES_REQUIRED pedantjson_consumers_cleared
        FIXTURES_SETUP pedantjson_installed
    )

    pedantjson_add_consumer_test("a project finds the installed package with find_package" installed
        INSTALLED
    )

    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pedantjson_add_consumer_test("a project finds the installed library with pkg-config"
            pkg-config
            INSTALLED
        )
    endif()
endif()

pedantjson_add_consumer_test("a project builds pedantjson as a subproject" subproject
    OPTIONS "-DPEDANTJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
)
