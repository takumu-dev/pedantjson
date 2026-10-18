# A project that adds pedantjson's source tree as a subproject.
cmake_minimum_required(VERSION 3.28)

project(pedantjson_subproject_consumer LANGUAGES CXX)

set(PEDANTJSON_SOURCE_DIR "" CACHE PATH "pedantjson's source tree")
add_subdirectory("${PEDANTJSON_SOURCE_DIR}" pedantjson)

# A subproject builds the library alone.
if(PEDANTJSON_BUILD_TESTS OR TARGET pedantjson_tests)
    message(FATAL_ERROR "pedantjson builds its tests as a subproject.")
endif()

add_executable(consumer ../consumer.cpp)
target_link_libraries(consumer PRIVATE pedantjson::pedantjson)
