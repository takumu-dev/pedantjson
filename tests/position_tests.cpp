#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <limits>
#include <string>

using pedantjson::Position;
using pedantjson::SyntaxErrorCode;
using support::parsed;

namespace {

/// The byte-order mark, U+FEFF, in UTF-8.
const std::string byte_order_mark = "\xef\xbb\xbf";

} // namespace

TEST_CASE("a position is a line and a column, each counting from 1", "[positions]") {
    const auto document = parsed(R"({
  "a": [1,
    2]
})");
    const auto root = document.root();
    const auto a = root.as_object()->find("a");
    REQUIRE(a.has_value());
    const auto array = *a->value.as_array();
    CHECK(document.position(root.offset()) == Position{.line = 1, .column = 1});
    CHECK(document.position(a->key.offset()) == Position{.line = 2, .column = 3});
    CHECK(document.position(a->value.offset()) == Position{.line = 2, .column = 8});
    CHECK(document.position(array[0].offset()) == Position{.line = 2, .column = 9});
    CHECK(document.position(array[1].offset()) == Position{.line = 3, .column = 5});
}

TEST_CASE("a line ends at a line feed, a carriage return, or a carriage return and a line feed",
          "[positions]") {
    const auto document = parsed("[1,\n2,\r3,\r\n4,\n\r5,\r\r6]");
    const auto array = *document.root().as_array();
    REQUIRE(array.size() == 6);
    CHECK(document.position(array[0].offset()) == Position{.line = 1, .column = 2});
    CHECK(document.position(array[1].offset()) == Position{.line = 2, .column = 1});
    CHECK(document.position(array[2].offset()) == Position{.line = 3, .column = 1});
    CHECK(document.position(array[3].offset()) == Position{.line = 4, .column = 1});
    CHECK(document.position(array[4].offset()) == Position{.line = 6, .column = 1});
    CHECK(document.position(array[5].offset()) == Position{.line = 8, .column = 1});
}

TEST_CASE("nothing in a string ends a line", "[positions]") {
    // An escaped line feed, and U+2028 LINE SEPARATOR, which a string may hold as it is.
    const auto document = parsed("[\"\\n\\r\", \"\xe2\x80\xa8\", 1]");
    const auto array = *document.root().as_array();
    REQUIRE(array.size() == 3);
    CHECK(document.position(array[2].offset()) == Position{.line = 1, .column = 15});
}

TEST_CASE("a column counts code points, not bytes, and a tab as one", "[positions]") {
    // U+00E9, U+20AC and U+1F600 take two, three and four bytes.
    const std::string text = "[\"\xc3\xa9\",\t\"\xe2\x82\xac\xf0\x9f\x98\x80\", 1]";
    const auto document = parsed(text);
    const auto array = *document.root().as_array();
    REQUIRE(array.size() == 3);
    CHECK(array[2].offset() == 18);
    CHECK(document.position(array[1].offset()) == Position{.line = 1, .column = 7});
    CHECK(document.position(array[2].offset()) == Position{.line = 1, .column = 13});
}

TEST_CASE("an offset within a character is in that character's column", "[positions]") {
    const auto document = parsed("[\"\xc3\xa9\xf0\x9f\x98\x80\"]");
    CHECK(document.position(2) == Position{.line = 1, .column = 3});
    CHECK(document.position(3) == Position{.line = 1, .column = 3});
    CHECK(document.position(4) == Position{.line = 1, .column = 4});
    CHECK(document.position(5) == Position{.line = 1, .column = 4});
    CHECK(document.position(7) == Position{.line = 1, .column = 4});
    CHECK(document.position(8) == Position{.line = 1, .column = 5});
}

TEST_CASE("a syntax error carries the position of its offset", "[positions]") {
    using enum SyntaxErrorCode;
    auto [text, code, offset,
          position] = GENERATE(table<std::string, SyntaxErrorCode, std::size_t, Position>({
        {"?", unexpected_character, 0, {.line = 1, .column = 1}},
        {"[\n  1,\n  ?]", unexpected_character, 9, {.line = 3, .column = 3}},
        {"1\n\n x", trailing_content, 4, {.line = 3, .column = 2}},
        {" \n\n", empty_input, 3, {.line = 3, .column = 1}},
        {"[1,\r\n", unexpected_end, 5, {.line = 2, .column = 1}},
        {"[\"\xc3\xa9\xf0\x9f\x98\x80\" ?]", unexpected_character, 10, {.line = 1, .column = 7}},
        // A continuation byte with nothing to continue begins no character before it.
        {"\"\xc3\xa9\x80\"", invalid_utf8, 3, {.line = 1, .column = 3}},
        // An unpaired surrogate is found after its escape, and reported at its backslash.
        {"[\n \"a\\ud800 \"]", unpaired_surrogate, 5, {.line = 2, .column = 4}},
    }));
    CAPTURE(text);
    const auto error = support::syntax_error(text);
    CHECK(error.code == code);
    CHECK(error.offset == offset);
    CHECK(error.position == position);
}

TEST_CASE("a byte-order mark takes no column", "[positions]") {
    const auto document = parsed(byte_order_mark + "[1]");
    CHECK(document.position(document.root().offset()) == Position{.line = 1, .column = 1});
    CHECK(document.position((*document.root().as_array())[0].offset()) ==
          Position{.line = 1, .column = 2});
    // An offset within the mark is before the first character.
    CHECK(document.position(0) == Position{.line = 1, .column = 1});
    CHECK(document.position(2) == Position{.line = 1, .column = 1});

    const auto first = support::syntax_error(byte_order_mark + "?");
    CHECK(first.code == SyntaxErrorCode::unexpected_character);
    CHECK(first.offset == 3);
    CHECK(first.position == Position{.line = 1, .column = 1});
    const auto second = support::syntax_error(byte_order_mark + "\n?");
    CHECK(second.code == SyntaxErrorCode::unexpected_character);
    CHECK(second.offset == 4);
    CHECK(second.position == Position{.line = 2, .column = 1});
}

TEST_CASE("the end of the text is after its last character, and so is any offset beyond it",
          "[positions]") {
    auto [text, end] = GENERATE(table<std::string, Position>({
        {"[1]", {.line = 1, .column = 4}},
        {"[1]\n", {.line = 2, .column = 1}},
        {"[1]\r", {.line = 2, .column = 1}},
        {"[1]\r\n", {.line = 2, .column = 1}},
        {"\"\xf0\x9f\x98\x80\"", {.line = 1, .column = 4}},
    }));
    CAPTURE(text);
    const auto document = parsed(text);
    CHECK(document.position(text.size()) == end);
    CHECK(document.position(text.size() + 1) == end);
    CHECK(document.position(std::numeric_limits<std::size_t>::max()) == end);
}

TEST_CASE("a carriage return and a line feed are the end of one line", "[positions]") {
    const auto document = parsed("[1]\r\n");
    CHECK(document.position(3) == Position{.line = 1, .column = 4});
    CHECK(document.position(4) == Position{.line = 1, .column = 5});
    CHECK(document.position(5) == Position{.line = 2, .column = 1});
}
