# pedantjson's fuzzer, which feeds the parser texts that libFuzzer makes up and checks what the
# public header promises of each one.
#
#   pedantjson_fuzzer  the fuzzer
#   fuzz               runs it for PEDANTJSON_FUZZ_SECONDS, from the texts of JSONTestSuite and of
#                      the corpus in this directory
#
# libFuzzer comes with Clang, where its runtime is installed, and links only beside the standard
# library that the runtime was built with: apt.llvm.org's links with libstdc++ but not with libc++.
# Where the compiler can't link a program with it, neither target exists, so a preset that builds
# `fuzz` fails at once, naming the target.

include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS -fsanitize=fuzzer)
    set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=fuzzer)
    check_cxx_source_compiles([[
        #include <cstddef>
        #include <cstdint>
        extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t) { return 0; }
    ]] PEDANTJSON_HAVE_LIBFUZZER)
    cmake_pop_check_state()
endif()
if(NOT PEDANTJSON_HAVE_LIBFUZZER)
    message(STATUS "Not building pedantjson's fuzzer: the compiler can't link a libFuzzer program")
    return()
endif()

# libFuzzer follows the paths each text takes through the library, so the library has to be built
# with coverage instrumentation. That instrumentation needs libFuzzer's runtime in the program, so
# the fuzzer builds a copy of the library of its own, leaving the tests and the install alone.
get_target_property(library_dir pedantjson SOURCE_DIR)
get_target_property(library_sources pedantjson SOURCES)
list(TRANSFORM library_sources PREPEND "${library_dir}/")
add_library(pedantjson_fuzzed STATIC ${library_sources})
target_include_directories(pedantjson_fuzzed PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_compile_features(pedantjson_fuzzed PUBLIC cxx_std_23)
target_compile_options(pedantjson_fuzzed PRIVATE -fsanitize=fuzzer-no-link)
set_target_properties(pedantjson_fuzzed PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_SCAN_FOR_MODULES OFF
)
pedantjson_target_conventions(pedantjson_fuzzed)

add_executable(pedantjson_fuzzer fuzzer.cpp)
target_link_libraries(pedantjson_fuzzer PRIVATE pedantjson_fuzzed)
target_compile_options(pedantjson_fuzzer PRIVATE -fsanitize=fuzzer)
target_link_options(pedantjson_fuzzer PRIVATE -fsanitize=fuzzer)
set_target_properties(pedantjson_fuzzer PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_SCAN_FOR_MODULES OFF
)
pedantjson_target_warnings(pedantjson_fuzzer)

set(PEDANTJSON_FUZZ_SECONDS 60 CACHE STRING "How long the fuzz target runs the fuzzer, in seconds")

# libFuzzer reads every directory it is given, and keeps the texts that take new paths in the first,
# which is in the build tree, so the texts committed here change only by hand. `corpus` holds each
# text that has crashed the fuzzer, and libFuzzer stops if it is missing, so git keeps it while it
# is empty. A text that crashes the fuzzer is written into `crashes`, and so is one that takes over
# ten seconds, which can only be a hang. Texts are cut at 4 KiB, which is deep and wide enough for
# every rule, and short enough that each run is quick.
set(found "${CMAKE_CURRENT_BINARY_DIR}/found")
set(crashes "${CMAKE_CURRENT_BINARY_DIR}/crashes")
add_custom_target(fuzz
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${found}" "${crashes}"
    COMMAND pedantjson_fuzzer
        -max_total_time=${PEDANTJSON_FUZZ_SECONDS}
        -max_len=4096
        -timeout=10
        -print_final_stats=1
        "-artifact_prefix=${crashes}/"
        "${found}"
        "${CMAKE_CURRENT_SOURCE_DIR}/corpus"
        "${PROJECT_SOURCE_DIR}/tests/JSONTestSuite/test_parsing"
    COMMENT "Fuzzing the parser for ${PEDANTJSON_FUZZ_SECONDS} seconds"
    USES_TERMINAL
    VERBATIM
)
