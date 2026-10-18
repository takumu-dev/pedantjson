#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <set>
#include <string>
#include <string_view>

using pedantjson::SyntaxError;
using pedantjson::SyntaxErrorCode;
using support::parsed;
using support::syntax_error;

TEST_CASE("a text with no value is empty input, at its end", "[syntax errors]") {
    auto text = GENERATE(as<std::string>{}, "", " ", "\t\n\r ");
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::empty_input);
    CHECK(error.offset == text.size());
}

TEST_CASE("anything but whitespace after the value is trailing content", "[syntax errors]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"1 2", 2},
        {"{} x", 3},
        {"[]]", 2},
        {"{}}", 2},
        {"null,", 4},
        {R"("a""b")", 3},
        {"true false", 5},
        {"0x10", 1},
        {"[] // comment", 3},
        {std::string{"1\0", 2}, 1},
        {"1\v", 1},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::trailing_content);
    CHECK(error.offset == offset);
}

TEST_CASE("a character that can't begin a value is unexpected where a value should begin",
          "[syntax errors]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"?", 0},
        {"]", 0},
        {"}", 0},
        {":", 0},
        {",", 0},
        {"'a'", 0},
        {"\x01", 0},
        {"[?]", 1},
        {R"({"a": ?})", 6},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_character);
    CHECK(error.offset == offset);
}

TEST_CASE("whitespace is spaces, tabs, line feeds and carriage returns only", "[syntax errors]") {
    CHECK(parsed(" \t\n\r1 \t\n\r").root().offset() == 4);

    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"\v1", 0},
        {"\f1", 0},
        {"\xc2\xa0"
         "1",
         0},
        {"[1,\v2]", 3},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_character);
    CHECK(error.offset == offset);
}

TEST_CASE("every code has a message of its own", "[syntax errors]") {
    const auto codes = {
        SyntaxErrorCode::empty_input,         SyntaxErrorCode::unexpected_character,
        SyntaxErrorCode::unexpected_end,      SyntaxErrorCode::invalid_number,
        SyntaxErrorCode::unterminated_string, SyntaxErrorCode::invalid_escape,
        SyntaxErrorCode::unpaired_surrogate,  SyntaxErrorCode::control_character_in_string,
        SyntaxErrorCode::invalid_utf8,        SyntaxErrorCode::too_deep,
        SyntaxErrorCode::trailing_content,
    };
    std::set<std::string_view> messages;
    for (const auto code : codes) {
        const std::string_view message = SyntaxError{.code = code, .offset = 0}.message();
        CAPTURE(code, message);
        CHECK_FALSE(message.empty());
        messages.insert(message);
    }
    CHECK(messages.size() == codes.size());
    CHECK(syntax_error("[").message() == "unexpected end of the text");
}
