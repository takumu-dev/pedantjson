# A project that adds pedantjson's source tree as a subproject.
cmake_minimum_required(VERSION 3.28)

project(pedantjson_subproject_consumer LANGUAGES CXX)

# Like most projects, this one uses no C++ modules, so it isn't scanned for them, which with Clang
# would take clang-scan-deps.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(PEDANTJSON_SOURCE_DIR "" CACHE PATH "pedantjson's source tree")
add_subdirectory("${PEDANTJSON_SOURCE_DIR}" pedantjson)

# A subproject builds the library alone.
if(PEDANTJSON_BUILD_TESTS OR TARGET pedantjson_tests)
    message(FATAL_ERROR "pedantjson builds its tests as a subproject.")
endif()
if(PEDANTJSON_BUILD_FUZZER OR TARGET pedantjson_fuzzer)
    message(FATAL_ERROR "pedantjson builds its fuzzer as a subproject.")
endif()

add_executable(consumer ../consumer.cpp)
target_link_libraries(consumer PRIVATE pedantjson::pedantjson)
