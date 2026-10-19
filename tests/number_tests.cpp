#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <string>

using pedantjson::Kind;
using pedantjson::SyntaxErrorCode;
using support::parsed;
using support::required;
using support::syntax_error;

TEST_CASE("a number keeps the text it was written as", "[numbers]") {
    auto text = GENERATE(as<std::string>{}, "0", "-0", "7", "123", "-9876543210", "0.5", "1.25",
                         "-0.0", "10.000", "1e5", "1E5", "1e+5", "1e-5", "1e05", "0e0", "1.5e10",
                         "-1.5E-10", "123456789012345678901234567890");
    CAPTURE(text);
    const auto document = parsed(text);
    const auto value = document.root();
    CHECK(value.kind() == Kind::number);
    CHECK(required(value.as_number()).text() == text);
}

TEST_CASE("a number begins at its sign or its first digit", "[numbers]") {
    CHECK(parsed("-1").root().offset() == 0);
    CHECK(parsed("  -1").root().offset() == 2);
    CHECK(parsed("\n42 ").root().offset() == 1);
}

TEST_CASE("a number against the grammar is invalid at its first wrong character", "[numbers]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"01", 1},
        {"00", 1},
        {"-01", 2},
        {"-a", 1},
        {"- 1", 1},
        {"-Infinity", 1},
        {"1.e5", 2},
        {"1. ", 2},
        {"1.5.3", 3},
        {"1e+-5", 3},
        {"1ee5", 2},
        {"1e5e5", 3},
        {"1e5.0", 3},
        {"1-2", 1},
        {"1+2", 1},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::invalid_number);
    CHECK(error.offset == offset);
}

TEST_CASE("a number cut short by the end of the text is an unexpected end", "[numbers]") {
    const auto text = GENERATE(as<std::string>{}, "-", "1.", "1e", "1E+", "-1.5e-");
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_end);
    CHECK(error.offset == text.size());
}

TEST_CASE("a number can't begin with a plus sign or a point, or be a word", "[numbers]") {
    const auto text = GENERATE(as<std::string>{}, "+1", ".5", "Infinity", "NaN");
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_character);
    CHECK(error.offset == 0);
}
