# A project that finds the installed package with find_package.
cmake_minimum_required(VERSION 3.28)

project(pedantjson_installed_consumer LANGUAGES CXX)

find_package(pedantjson CONFIG REQUIRED)

# pedantjson's warnings and warnings as errors are its own, and must not reach a project's build.
get_target_property(options pedantjson::pedantjson INTERFACE_COMPILE_OPTIONS)
if(options)
    message(FATAL_ERROR "pedantjson::pedantjson passes compile options to the project: ${options}")
endif()

add_executable(consumer ../consumer.cpp)
target_link_libraries(consumer PRIVATE pedantjson::pedantjson)
