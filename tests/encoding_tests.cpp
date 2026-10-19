#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <string>

using pedantjson::Kind;
using pedantjson::SyntaxErrorCode;
using support::byte_order_mark;
using support::parsed;
using support::required;
using support::syntax_error;

TEST_CASE("a byte-order mark at the start of the text is ignored", "[encoding]") {
    const auto document = parsed(byte_order_mark() + R"( {"a": 1})");
    CHECK(document.root().kind() == Kind::object);
    // Offsets still count from the start of the text, the mark included.
    CHECK(document.root().offset() == 4);
    CHECK(required(required(document.root().as_object()).find("a")).key.offset() == 5);
}

TEST_CASE("a text of a byte-order mark alone is empty input, at its end", "[encoding]") {
    const auto text = GENERATE(as<std::string>{}, byte_order_mark(), byte_order_mark() + " \n");
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::empty_input);
    CHECK(error.offset == text.size());
}

TEST_CASE("a byte-order mark anywhere but the start is a character JSON doesn't allow there",
          "[encoding]") {
    auto [text, code, offset] = GENERATE(table<std::string, SyntaxErrorCode, std::size_t>({
        {" " + byte_order_mark() + "1", SyntaxErrorCode::unexpected_character, 1},
        {byte_order_mark() + byte_order_mark() + "1", SyntaxErrorCode::unexpected_character, 3},
        {"[" + byte_order_mark() + "1]", SyntaxErrorCode::unexpected_character, 1},
        {"1" + byte_order_mark(), SyntaxErrorCode::trailing_content, 1},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == code);
    CHECK(error.offset == offset);
}

TEST_CASE("a byte-order mark in a string is a character of the string", "[encoding]") {
    CHECK(parsed("\"" + byte_order_mark() + "\"").root().as_string() == byte_order_mark());
}

TEST_CASE("well-formed UTF-8 in a string stands for itself, at the edges of every range",
          "[encoding]") {
    const auto text = GENERATE(as<std::string>{},
                               "\xc2\x80",         // U+0080
                               "\xdf\xbf",         // U+07FF
                               "\xe0\xa0\x80",     // U+0800
                               "\xec\xbf\xbf",     // U+CFFF
                               "\xed\x80\x80",     // U+D000
                               "\xed\x9f\xbf",     // U+D7FF, below the surrogates
                               "\xee\x80\x80",     // U+E000, above them
                               "\xef\xbf\xbf",     // U+FFFF
                               "\xf0\x90\x80\x80", // U+10000
                               "\xf3\xbf\xbf\xbf", // U+FFFFF
                               "\xf4\x80\x80\x80", // U+100000
                               "\xf4\x8f\xbf\xbf"  // U+10FFFF
    );
    CAPTURE(text);
    CHECK(parsed("\"" + text + "\"").root().as_string() == text);
}

TEST_CASE("bytes in a string that aren't UTF-8 are invalid at the first byte of their sequence",
          "[encoding]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        // A continuation byte with nothing to continue.
        {"\"\x80\"", 1},
        {"\"\xbf\"", 1},
        {"\"\xc3\xa9\xa9\"", 3},
        // Bytes that begin no sequence.
        {"\"\xc0\xaf\"", 1},
        {"\"\xc1\xbf\"", 1},
        {"\"\xf5\x80\x80\x80\"", 1},
        {"\"\xf8\x88\x80\x80\x80\"", 1},
        {"\"\xfc\x84\x80\x80\x80\x80\"", 1},
        {"\"\xfe\"", 1},
        {"\"\xff\"", 1},
        // A code point in more bytes than it needs.
        {"\"\xe0\x80\xaf\"", 1},
        {"\"\xe0\x9f\xbf\"", 1},
        {"\"\xf0\x80\x80\xaf\"", 1},
        {"\"\xf0\x8f\xbf\xbf\"", 1},
        // A surrogate, which UTF-8 never encodes.
        {"\"\xed\xa0\x80\"", 1},
        {"\"\xed\xbf\xbf\"", 1},
        // Beyond U+10FFFF.
        {"\"\xf4\x90\x80\x80\"", 1},
        // A sequence cut short by the next character.
        {"\"\xc3\"", 1},
        {"\"\xe2\x82\"", 1},
        {"\"\xf0\x9f\x98\"", 1},
        {"\"\xe9t\xe9\"", 1},
        {"\"a\xe9"
         "b\"",
         2},
        // Or by the end of the text.
        {"\"\xe2\x82", 1},
        // After an escape, a high surrogate's included, and in a key.
        {"\"\\n\xff\"", 3},
        {"\"\\ud800\xff\"", 7},
        {"{\"\x80\": 1}", 2},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::invalid_utf8);
    CHECK(error.offset == offset);
}

TEST_CASE("bytes that aren't UTF-8 are invalid wherever the text stops being JSON at them",
          "[encoding]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"\xff", 0},
        {"\xe5", 0},
        {"\xef\xbb{}", 0},
        {"[\xff]", 1},
        {"tru\xe5", 3},
        {"[0\xe5]", 2},
        {"[1e\xe5]", 3},
        {"[1e1\xe5]", 4},
        {"[\"\\\xe5\"]", 3},
        {"[\"\\u\xe5\"]", 4},
        {"{\"a\"\xff: 1}", 4},
        {"1\xff", 1},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::invalid_utf8);
    CHECK(error.offset == offset);
}

TEST_CASE("well-formed UTF-8 outside a string is a character JSON doesn't allow there",
          "[encoding]") {
    auto [text, code, offset] = GENERATE(table<std::string, SyntaxErrorCode, std::size_t>({
        {"\xc3\xa5", SyntaxErrorCode::unexpected_character, 0},
        {"[\xe2\x81\xa0]", SyntaxErrorCode::unexpected_character, 1},
        {"[1e\xef\xbc\x91]", SyntaxErrorCode::invalid_number, 3},
        {"1\xf0\x9f\x98\x80", SyntaxErrorCode::trailing_content, 1},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == code);
    CHECK(error.offset == offset);
}
