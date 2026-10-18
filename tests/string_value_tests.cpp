#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <string>

using pedantjson::Kind;
using pedantjson::SyntaxErrorCode;
using support::parsed;
using support::syntax_error;

namespace {

/// The string a text holds, which must be one.
std::string string_of(std::string text) {
    const auto document = parsed(std::move(text));
    const auto value = document.root();
    CHECK(value.kind() == Kind::string);
    REQUIRE(value.as_string().has_value());
    return std::string{*value.as_string()};
}

} // namespace

TEST_CASE("a string without escapes holds the characters between its quotes", "[strings]") {
    CHECK(string_of(R"("")").empty());
    CHECK(string_of(R"("abc")") == "abc");
    CHECK(string_of(R"(" a b ")") == " a b ");
    CHECK(string_of(R"("[1, {}]: true")") == "[1, {}]: true");
    // Bytes at and above U+007F stand for themselves.
    CHECK(string_of("\"\x7f\"") == "\x7f");
    CHECK(string_of("\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\"") ==
          "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80");
}

TEST_CASE("a string begins at its opening quote", "[strings]") {
    CHECK(parsed(R"("a")").root().offset() == 0);
    CHECK(parsed(R"(  "a")").root().offset() == 2);
}

TEST_CASE("each short escape stands for its character", "[strings]") {
    auto [text, expected] = GENERATE(table<std::string, std::string>({
        {R"("\"")", "\""},
        {R"("\\")", "\\"},
        {R"("\/")", "/"},
        {R"("\b")", "\b"},
        {R"("\f")", "\f"},
        {R"("\n")", "\n"},
        {R"("\r")", "\r"},
        {R"("\t")", "\t"},
    }));
    CAPTURE(text);
    CHECK(string_of(text) == expected);
}

TEST_CASE("a \\u escape stands for its code point, in UTF-8", "[strings]") {
    auto [text, expected] = GENERATE(table<std::string, std::string>({
        {R"("\u0041")", "A"},
        {R"("\u007f")", "\x7f"},
        {R"("\u0080")", "\xc2\x80"},
        {R"("\u00e9")", "\xc3\xa9"},
        {R"("\u00E9")", "\xc3\xa9"},
        {R"("\u07FF")", "\xdf\xbf"},
        {R"("\u0800")", "\xe0\xa0\x80"},
        {R"("\u20ac")", "\xe2\x82\xac"},
        {R"("\uFFFF")", "\xef\xbf\xbf"},
        {R"("\u0000")", std::string{"\0", 1}},
    }));
    CAPTURE(text);
    CHECK(string_of(text) == expected);
}

TEST_CASE("a surrogate pair of \\u escapes stands for one code point", "[strings]") {
    auto [text, expected] = GENERATE(table<std::string, std::string>({
        {R"("\ud800\udc00")", "\xf0\x90\x80\x80"},
        {R"("\uD83D\uDE00")", "\xf0\x9f\x98\x80"},
        {R"("\udbff\udfff")", "\xf4\x8f\xbf\xbf"},
    }));
    CAPTURE(text);
    CHECK(string_of(text) == expected);
}

TEST_CASE("escapes and plain characters mix in any order", "[strings]") {
    CHECK(string_of(R"("a\nb\u0041c")") == "a\nbAc");
    CHECK(string_of(R"("\\n")") == "\\n");
    CHECK(string_of(R"("\"quoted\"")") == "\"quoted\"");
}

TEST_CASE("a string still open at the end of the text is unterminated at the end", "[strings]") {
    auto text = GENERATE(as<std::string>{}, R"(")", R"("abc)", R"("\")", R"("\)", R"("\u)",
                         R"("\u12)", R"("\ud800\u)");
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unterminated_string);
    CHECK(error.offset == text.size());
}

TEST_CASE("an escape JSON doesn't have is invalid at the character after the backslash",
          "[strings]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {R"("\x")", 2},
        {R"("\a")", 2},
        {R"("\0")", 2},
        {R"("\'")", 2},
        {R"("\ ")", 2},
        {R"("\U0041")", 2},
        {R"("ab\N")", 4},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::invalid_escape);
    CHECK(error.offset == offset);
}

TEST_CASE("a \\u escape without four hexadecimal digits is invalid at the first that isn't one",
          "[strings]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {R"("\u")", 3},
        {R"("\u12")", 5},
        {R"("\u12G4")", 5},
        {R"("\ux041")", 3},
        {R"("\u004")", 6},
        {R"("\ud800\u12x4")", 11},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::invalid_escape);
    CHECK(error.offset == offset);
}

TEST_CASE("a control character written into a string is an error at that character", "[strings]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"\"a\tb\"", 2},
        {"\"\n\"", 1},
        {"\"\r\"", 1},
        {"\"\x01\"", 1},
        {"\"\x1f\"", 1},
        {std::string{"\"a\0b\"", 5}, 2},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::control_character_in_string);
    CHECK(error.offset == offset);
}
