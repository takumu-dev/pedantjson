#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

using pedantjson::Array;
using pedantjson::Kind;
using pedantjson::SyntaxErrorCode;
using support::element;
using support::parsed;
using support::required;
using support::syntax_error;

TEST_CASE("an empty array has no elements", "[arrays]") {
    const auto text = GENERATE(as<std::string>{}, "[]", "[ ]", "[\n\t\r ]");
    CAPTURE(text);
    const auto document = parsed(text);
    CHECK(document.root().kind() == Kind::array);
    const auto array = required(document.root().as_array());
    CHECK(array.empty());
    CHECK(array.size() == 0);
    CHECK(array.begin() == array.end());
}

TEST_CASE("an array holds its elements in the order of the text", "[arrays]") {
    const auto document = parsed(R"([1, true, "a", null, false])");
    const auto array = required(document.root().as_array());
    REQUIRE(array.size() == 5);
    CHECK(required(element(array, 0).as_number()).text() == "1");
    CHECK(element(array, 1).as_bool() == true);
    CHECK(element(array, 2).as_string() == "a");
    CHECK(element(array, 3).is_null());
    CHECK(element(array, 4).as_bool() == false);
}

TEST_CASE("walking an array visits its elements in the order of the text", "[arrays]") {
    const auto document = parsed(R"([1, true, "a", null, false])");
    const auto array = required(document.root().as_array());
    std::vector<Kind> kinds;
    for (const auto element : array) {
        kinds.push_back(element.kind());
    }
    CHECK(kinds ==
          std::vector{Kind::number, Kind::boolean, Kind::string, Kind::null, Kind::boolean});
}

TEST_CASE("arrays hold arrays", "[arrays]") {
    const auto document = parsed("[[], [[1]], [2, [3, 4]]]");
    const auto root = required(document.root().as_array());
    REQUIRE(root.size() == 3);
    CHECK(required(element(root, 0).as_array()).empty());
    const auto second = required(element(root, 1).as_array());
    const auto inner = required(element(second, 0).as_array());
    CHECK(required(element(inner, 0).as_number()).text() == "1");
    const auto third = required(element(root, 2).as_array());
    REQUIRE(third.size() == 2);
    CHECK(required(element(third, 0).as_number()).text() == "2");
    const auto innermost = required(element(third, 1).as_array());
    REQUIRE(innermost.size() == 2);
    CHECK(required(element(innermost, 0).as_number()).text() == "3");
    CHECK(required(element(innermost, 1).as_number()).text() == "4");
}

TEST_CASE("an array and each of its elements begin at their first character", "[arrays]") {
    const auto document = parsed(R"( [ 1,[2] , "x",null])");
    const auto root = document.root();
    CHECK(root.offset() == 1);
    const auto array = required(root.as_array());
    REQUIRE(array.size() == 4);
    CHECK(element(array, 0).offset() == 3);
    CHECK(element(array, 1).offset() == 5);
    CHECK(element(required(element(array, 1).as_array()), 0).offset() == 6);
    CHECK(element(array, 2).offset() == 11);
    CHECK(element(array, 3).offset() == 15);
}

TEST_CASE("whitespace may stand around every element and bracket", "[arrays]") {
    const auto document = parsed(" \t\n\r[ \t\n\r1 \t\n\r, \t\n\r[ \t\n\r] \t\n\r] \t\n\r");
    const auto array = required(document.root().as_array());
    REQUIRE(array.size() == 2);
    CHECK(required(element(array, 0).as_number()).text() == "1");
    CHECK(required(element(array, 1).as_array()).empty());
}

TEST_CASE("an array is a sized forward range of values", "[arrays]") {
    STATIC_CHECK(std::ranges::forward_range<Array>);
    STATIC_CHECK(std::ranges::sized_range<Array>);
    const auto document = parsed("[3, 1, 2]");
    const auto array = required(document.root().as_array());
    CHECK(std::ranges::distance(array) == 3);
    const auto found = std::ranges::find_if(
        array, [](pedantjson::Value value) { return required(value.as_number()).text() == "2"; });
    REQUIRE(found != array.end());
    CHECK((*found).offset() == 7);
}

TEST_CASE("a default array is empty", "[arrays]") {
    const Array array;
    CHECK(array.empty());
    CHECK(array.begin() == array.end());
}

TEST_CASE("arrays nested a hundred thousand deep are read", "[arrays]") {
    constexpr std::size_t depth = 100'000;
    const auto document =
        parsed(std::string(depth, '[') + std::string(depth, ']'), {.max_depth = depth});
    auto array = required(document.root().as_array());
    std::size_t levels = 1;
    while (!array.empty()) {
        array = required(element(array, 0).as_array());
        ++levels;
    }
    CHECK(levels == depth);
}

TEST_CASE("an array cut short by the end of the text is an unexpected end", "[arrays]") {
    const auto text =
        GENERATE(as<std::string>{}, "[", "[ ", "[1", "[1 ", "[1,", "[1, ", "[[]", "[[1]");
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_end);
    CHECK(error.offset == text.size());
}

TEST_CASE("an array whose elements aren't separated by single commas has an unexpected character",
          "[arrays]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"[1 2]", 3},
        {"[1,]", 3},
        {"[1, ]", 4},
        {"[,1]", 1},
        {"[,]", 1},
        {"[1,,2]", 3},
        {"[1;2]", 2},
        {"[1:2]", 2},
        {"[1}", 2},
        {"[[1]}", 4},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_character);
    CHECK(error.offset == offset);
}
