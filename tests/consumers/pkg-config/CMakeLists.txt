# A project that finds the installed library through its pkg-config file alone, as Meson and
# Makefiles do.
cmake_minimum_required(VERSION 3.28)

project(pedantjson_pkg_config_consumer LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(pedantjson REQUIRED IMPORTED_TARGET pedantjson)

add_executable(consumer ../consumer.cpp)
target_compile_features(consumer PRIVATE cxx_std_23)
target_link_libraries(consumer PRIVATE PkgConfig::pedantjson)
