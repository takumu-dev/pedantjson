#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <string>

using pedantjson::Kind;
using pedantjson::ParseOptions;
using pedantjson::SyntaxErrorCode;
using support::parsed;
using support::required;
using support::syntax_error;

namespace {

/// Arrays nested `depth` deep, the innermost empty.
std::string arrays(std::size_t depth) { return std::string(depth, '[') + std::string(depth, ']'); }

/// Objects nested `depth` deep, each the value of the member `a` of the one around it.
std::string objects(std::size_t depth) {
    std::string text;
    for (std::size_t level = 1; level < depth; ++level) {
        text += R"({"a":)";
    }
    text += "{}";
    return text + std::string(depth - 1, '}');
}

} // namespace

TEST_CASE("arrays and objects may nest 256 deep by default", "[nesting]") {
    CHECK(ParseOptions{}.max_depth == 256);
    CHECK(parsed(arrays(256)).root().kind() == Kind::array);
    CHECK(parsed(objects(256)).root().kind() == Kind::object);
    CHECK(parsed(arrays(255).insert(255, "1")).root().kind() == Kind::array);
}

TEST_CASE("an array or an object deeper than the limit is too deep, at its opening bracket",
          "[nesting]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {arrays(257), 256},
        {objects(257), 256 * 5},
        {arrays(100'000), 256},
        // The text ends long after the limit is passed.
        {std::string(300, '['), 256},
    }));
    CAPTURE(text.size());
    const auto error = syntax_error(text);
    CHECK(error.code == SyntaxErrorCode::too_deep);
    CHECK(error.offset == offset);
}

TEST_CASE("the depth is the number of arrays and objects open, siblings not counted", "[nesting]") {
    const ParseOptions two{.max_depth = 2};
    const auto document = parsed(R"([[1], {"a": 2}, [], {}])", two);
    CHECK(required(document.root().as_array()).size() == 4);

    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"[[[]]]", 2},
        {R"([{"a":[]}])", 6},
        {"[[1],[2],[[3]]]", 10},
        {R"({"a":{"b":{}}})", 10},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text, two);
    CHECK(error.code == SyntaxErrorCode::too_deep);
    CHECK(error.offset == offset);
}

TEST_CASE("a limit of 0 allows a value that isn't an array or an object, and nothing else",
          "[nesting]") {
    const ParseOptions none{.max_depth = 0};
    CHECK(parsed("1", none).root().kind() == Kind::number);
    CHECK(parsed(R"("[]")", none).root().kind() == Kind::string);

    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"[]", 0},
        {" {}", 1},
    }));
    CAPTURE(text);
    const auto error = syntax_error(text, none);
    CHECK(error.code == SyntaxErrorCode::too_deep);
    CHECK(error.offset == offset);
}

TEST_CASE("a limit may be as large as any nesting", "[nesting]") {
    CHECK(parsed(arrays(1000), {.max_depth = 1000}).root().kind() == Kind::array);
    const auto error = syntax_error(arrays(1001), {.max_depth = 1000});
    CHECK(error.code == SyntaxErrorCode::too_deep);
    CHECK(error.offset == 1000);
}
