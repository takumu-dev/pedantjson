# pedantjson: a strict JSON reader, built as one static library behind one header.
#
# The build only calls find_package, and never downloads anything. Every configuration the project
# builds itself is a preset in CMakePresets.json.
cmake_minimum_required(VERSION 3.28)

project(pedantjson
    VERSION 0.1.0
    DESCRIPTION "A strict JSON reader that puts a position on every key and value"
    HOMEPAGE_URL "https://github.com/takumu-dev/pedantjson"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(PedantjsonConventions)

# A project that adds pedantjson as a subproject gets the library alone.
option(PEDANTJSON_BUILD_TESTS "Build pedantjson's tests." ${PROJECT_IS_TOP_LEVEL})
option(PEDANTJSON_BUILD_FUZZER "Build pedantjson's fuzzer, where the compiler can link libFuzzer."
    ${PROJECT_IS_TOP_LEVEL}
)
option(PEDANTJSON_INSTALL "Install pedantjson's header, library and package files."
    ${PROJECT_IS_TOP_LEVEL}
)
option(PEDANTJSON_EXCEPTIONS
    "Build the library with exceptions enabled. It never throws, so it builds either way."
    ON
)

# Adds the library as the static library `target`. The fuzzer adds a copy of its own, which it
# builds with coverage instrumentation.
function(pedantjson_add_library target)
    set(dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    add_library(${target} STATIC)
    target_sources(${target}
        PRIVATE
            "${dir}/src/document.cpp"
            "${dir}/src/number.cpp"
            "${dir}/src/parse.cpp"
            "${dir}/src/pointer.cpp"
            "${dir}/src/storage.hpp"
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS "${dir}/include"
            FILES "${dir}/include/pedantjson/pedantjson.hpp"
    )
    target_compile_features(${target} PUBLIC cxx_std_23)
    # Standard C++ with no extensions, and no scanning for C++ modules, which pedantjson doesn't use.
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_SCAN_FOR_MODULES OFF
    )
    pedantjson_target_conventions(${target})
endfunction()

pedantjson_add_library(pedantjson)
add_library(pedantjson::pedantjson ALIAS pedantjson)

if(PEDANTJSON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PEDANTJSON_BUILD_FUZZER)
    add_subdirectory(fuzz)
endif()

if(PEDANTJSON_INSTALL)
    include(PedantjsonInstall)
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(PedantjsonLint)
endif()
