#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

using pedantjson::Kind;
using pedantjson::Object;
using pedantjson::SyntaxErrorCode;
using support::element;
using support::member;
using support::parsed;
using support::required;
using support::syntax_error;

namespace {

/// The names of an object's members, in order.
std::vector<std::string_view> names_of(Object object) {
    std::vector<std::string_view> names;
    for (const auto [key, value] : object) {
        names.push_back(key.name());
    }
    return names;
}

} // namespace

TEST_CASE("an empty object has no members", "[objects]") {
    const auto text = GENERATE(as<std::string>{}, "{}", "{ }", "{\n\t\r }");
    CAPTURE(text);
    const auto document = parsed(text);
    CHECK(document.root().kind() == Kind::object);
    const auto object = required(document.root().as_object());
    CHECK(object.empty());
    CHECK(object.size() == 0);
    CHECK(object.begin() == object.end());
    CHECK_FALSE(object.find("").has_value());
}

TEST_CASE("an object holds its members in the order of the text", "[objects]") {
    const auto document = parsed(R"({"b": 1, "a": true, "c": "x"})");
    const auto object = required(document.root().as_object());
    REQUIRE(object.size() == 3);
    CHECK(names_of(object) == std::vector<std::string_view>{"b", "a", "c"});
    CHECK(required(member(object, 0).value.as_number()).text() == "1");
    CHECK(member(object, 1).value.as_bool() == true);
    CHECK(member(object, 2).value.as_string() == "x");
}

TEST_CASE("an object keeps every member of a repeated name, in the order of the text",
          "[objects]") {
    const auto document = parsed(R"({"a": 1, "b": 2, "a": 3})");
    const auto object = required(document.root().as_object());
    REQUIRE(object.size() == 3);
    CHECK(names_of(object) == std::vector<std::string_view>{"a", "b", "a"});
    CHECK(required(member(object, 0).value.as_number()).text() == "1");
    CHECK(required(member(object, 2).value.as_number()).text() == "3");
    CHECK(member(object, 2).key.offset() == 17);
}

TEST_CASE("finding a name answers its first member", "[objects]") {
    const auto document = parsed(R"({"a": 1, "b": 2, "a": 3})");
    const auto object = required(document.root().as_object());
    const auto a = required(object.find("a"));
    CHECK(a.key.name() == "a");
    CHECK(a.key.offset() == 1);
    CHECK(required(a.value.as_number()).text() == "1");
    CHECK(required(required(object.find("b")).value.as_number()).text() == "2");
    CHECK_FALSE(object.find("c").has_value());
    CHECK_FALSE(object.find("A").has_value());
}

TEST_CASE("a key's name has its escapes decoded", "[objects]") {
    const auto document = parsed(R"({"\u0061\n": 1, "": 2, "\"": 3})");
    const auto object = required(document.root().as_object());
    CHECK(names_of(object) == std::vector<std::string_view>{"a\n", "", "\""});
    CHECK(required(required(object.find("a\n")).value.as_number()).text() == "1");
    CHECK(required(required(object.find("")).value.as_number()).text() == "2");
}

TEST_CASE("a key begins at its opening quote, and its value at its own first character",
          "[objects]") {
    const auto document = parsed(R"( { "a" : 1 ,"bc":[ ] })");
    const auto root = document.root();
    CHECK(root.offset() == 1);
    const auto object = required(root.as_object());
    REQUIRE(object.size() == 2);
    CHECK(member(object, 0).key.offset() == 3);
    CHECK(member(object, 0).value.offset() == 9);
    CHECK(member(object, 1).key.offset() == 12);
    CHECK(member(object, 1).value.offset() == 17);
}

TEST_CASE("objects and arrays hold each other", "[objects]") {
    const auto document = parsed(R"({"a": [{"b": {}}, []], "c": {"d": [1]}})");
    const auto root = required(document.root().as_object());
    REQUIRE(root.size() == 2);
    const auto a = required(required(root.find("a")).value.as_array());
    REQUIRE(a.size() == 2);
    const auto b = required(required(element(a, 0).as_object()).find("b"));
    CHECK(required(b.value.as_object()).empty());
    CHECK(required(element(a, 1).as_array()).empty());
    const auto c = required(required(root.find("c")).value.as_object());
    const auto d = required(required(c.find("d")).value.as_array());
    CHECK(required(element(d, 0).as_number()).text() == "1");
}

TEST_CASE("whitespace may stand around every key, colon, value and brace", "[objects]") {
    const auto document =
        parsed(" \t\n\r{ \t\n\r\"a\" \t\n\r: \t\n\r1 \t\n\r, \t\n\r\"b\":{ \t\n\r} } ");
    const auto object = required(document.root().as_object());
    CHECK(names_of(object) == std::vector<std::string_view>{"a", "b"});
}

TEST_CASE("an object is a sized forward range of members", "[objects]") {
    STATIC_CHECK(std::ranges::forward_range<Object>);
    STATIC_CHECK(std::ranges::sized_range<Object>);
    const auto document = parsed(R"({"x": 1, "y": 2})");
    const auto object = required(document.root().as_object());
    CHECK(std::ranges::distance(object) == 2);
}

TEST_CASE("a default object is empty", "[objects]") {
    const Object object;
    CHECK(object.empty());
    CHECK(object.begin() == object.end());
    CHECK_FALSE(object.find("x").has_value());
}

TEST_CASE("an object cut short by the end of the text is an unexpected end", "[objects]") {
    const auto text =
        GENERATE(as<std::string>{}, "{", "{ ", R"({"a")", R"({"a" )", R"({"a":)", R"({"a": )",
                 R"({"a": 1)", R"({"a": 1,)", R"({"a": 1, )", R"({"a": {})");
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_end);
    CHECK(error.offset == text.size());
}

TEST_CASE("an object that isn't string, colon, value and comma in turn has an unexpected character",
          "[objects]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {R"({1: 2})", 1},
        {R"({a: 1})", 1},
        {R"({'a': 1})", 1},
        {R"({,})", 1},
        {R"({"a" 1})", 5},
        {R"({"a"=1})", 4},
        {R"({"a"})", 4},
        {R"({"a",1})", 4},
        {R"({"a": })", 6},
        {R"({"a": 1 "b": 2})", 8},
        {R"({"a": 1,})", 8},
        {R"({"a": 1,,})", 8},
        {R"({"a": 1])", 7},
        {R"([{"a": 1])", 8},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_character);
    CHECK(error.offset == offset);
}
