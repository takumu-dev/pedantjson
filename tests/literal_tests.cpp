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

TEST_CASE("true and false are booleans with the values they name", "[literals]") {
    CHECK(parsed("true").root().kind() == Kind::boolean);
    CHECK(parsed("true").root().as_bool() == true);
    CHECK(parsed("false").root().kind() == Kind::boolean);
    CHECK(parsed("false").root().as_bool() == false);
}

TEST_CASE("null is null", "[literals]") {
    CHECK(parsed("null").root().kind() == Kind::null);
    CHECK(parsed("null").root().is_null());
}

TEST_CASE("a literal begins at its first letter", "[literals]") {
    CHECK(parsed("null").root().offset() == 0);
    CHECK(parsed(" \t\n\rtrue \t\n\r").root().offset() == 4);
}

TEST_CASE("a misspelt literal is an unexpected character at its first wrong letter", "[literals]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"nul1", 3},
        {"tRue", 1},
        {"fa1se", 2},
        {"nil", 1},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_character);
    CHECK(error.offset == offset);
}

TEST_CASE("a literal cut short by the end of the text is an unexpected end", "[literals]") {
    auto text = GENERATE(as<std::string>{}, "t", "tru", "fals", "nul");
    CAPTURE(text);
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::unexpected_end);
    CHECK(error.offset == text.size());
}
