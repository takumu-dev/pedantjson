# pedantjson's tests, which use the public header only.

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(pedantjson_tests
    array_tests.cpp
    document_tests.cpp
    encoding_tests.cpp
    literal_tests.cpp
    nesting_tests.cpp
    number_tests.cpp
    object_tests.cpp
    string_value_tests.cpp
    syntax_error_tests.cpp
)
target_link_libraries(pedantjson_tests PRIVATE pedantjson::pedantjson Catch2::Catch2WithMain)
set_target_properties(pedantjson_tests PROPERTIES CXX_EXTENSIONS OFF CXX_SCAN_FOR_MODULES OFF)
pedantjson_target_warnings(pedantjson_tests)
catch_discover_tests(pedantjson_tests)

add_subdirectory(consumers)
