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
#   INSTALLED   the consumer needs the installed library
#   BUILD_NAME  the name of the consumer's build directory, `source_dir` by default
#   OPTIONS     more cache settings for the consumer, which win over this build's
function(pedantjson_add_consumer_test name source_dir)
    cmake_parse_arguments(PARSE_ARGV 2 arg "INSTALLED" "BUILD_NAME" "OPTIONS")
    set(test_name "consumers: ${name}")
    if(NOT DEFINED arg_BUILD_NAME)
        set(arg_BUILD_NAME "${source_dir}")
    endif()
    if(arg_INSTALLED)
        list(APPEND arg_OPTIONS "-DCMAKE_PREFIX_PATH=${prefix}")
    endif()
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}" "${builds}/${arg_BUILD_NAME}"
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

    # A library that one of GCC and Clang builds links into a program that the other builds, on the
    # same standard library, libstdc++, as a package manager's build of it may differ from the
    # project's own compiler. The test runs where the other compiler is found, at its oldest version
    # or newer. A build with the sanitizers is left out, since the two compilers' runtimes for them
    # don't mix.
    if(NOT CMAKE_CXX_FLAGS MATCHES "-fsanitize=|-stdlib=libc\\+\\+")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            find_program(PEDANTJSON_OTHER_CXX_COMPILER NAMES clang++
                DOC "The other of GCC and Clang, which builds a consumer of the installed library"
            )
            set(other_floor 19)
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            find_program(PEDANTJSON_OTHER_CXX_COMPILER NAMES g++
                DOC "The other of GCC and Clang, which builds a consumer of the installed library"
            )
            set(other_floor 12)
        endif()
        if(PEDANTJSON_OTHER_CXX_COMPILER)
            execute_process(COMMAND "${PEDANTJSON_OTHER_CXX_COMPILER}" -dumpversion
                OUTPUT_VARIABLE other_version
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE other_result
            )
            if(other_result EQUAL 0 AND other_version VERSION_GREATER_EQUAL other_floor)
                pedantjson_add_consumer_test(
                    "a project that the other of GCC and Clang builds links the installed library"
                    installed
                    INSTALLED
                    BUILD_NAME installed-by-the-other-compiler
                    OPTIONS "-DCMAKE_CXX_COMPILER=${PEDANTJSON_OTHER_CXX_COMPILER}"
                )
            endif()
        endif()
    endif()
endif()

pedantjson_add_consumer_test("a project builds pedantjson as a subproject" subproject
    OPTIONS "-DPEDANTJSON_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
)
