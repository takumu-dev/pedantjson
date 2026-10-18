#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using pedantjson::Document;
using pedantjson::Kind;
using support::parsed;

TEST_CASE("only the accessor of a value's own kind answers", "[documents]") {
    auto [text, kind] = GENERATE(table<std::string, Kind>({
        {"null", Kind::null},
        {"false", Kind::boolean},
        {"0", Kind::number},
        {R"("")", Kind::string},
        {"[]", Kind::array},
        {"{}", Kind::object},
    }));
    CAPTURE(text);
    const auto document = parsed(text);
    const auto value = document.root();
    CHECK(value.kind() == kind);
    CHECK(value.is_null() == (kind == Kind::null));
    CHECK(value.as_bool().has_value() == (kind == Kind::boolean));
    CHECK(value.as_number().has_value() == (kind == Kind::number));
    CHECK(value.as_string().has_value() == (kind == Kind::string));
    CHECK(value.as_array().has_value() == (kind == Kind::array));
    CHECK(value.as_object().has_value() == (kind == Kind::object));
}

TEST_CASE("a document can be moved but not copied", "[documents]") {
    STATIC_CHECK_FALSE(std::is_copy_constructible_v<Document>);
    STATIC_CHECK_FALSE(std::is_copy_assignable_v<Document>);
    STATIC_CHECK(std::is_nothrow_move_constructible_v<Document>);
    STATIC_CHECK(std::is_nothrow_move_assignable_v<Document>);
}

TEST_CASE("handles into a document stay valid when the document moves", "[documents]") {
    // A text this short sits inside a std::string itself, which a move would copy elsewhere.
    auto document = parsed(R"({"k":["s",1]})");
    const auto root = document.root();
    const auto object = *root.as_object();
    const auto member = object[0];
    const std::string_view name = member.key.name();
    const auto array = *member.value.as_array();
    const std::string_view string = *array[0].as_string();
    const auto number = *array[1].as_number();

    Document moved = std::move(document);
    CHECK(root.kind() == Kind::object);
    CHECK(object.find("k").has_value());
    CHECK(name == "k");
    CHECK(member.key.offset() == 1);
    CHECK(array.size() == 2);
    CHECK(string == "s");
    CHECK(number.text() == "1");
    CHECK(moved.root().offset() == 0);

    Document assigned = parsed("null");
    assigned = std::move(moved);
    CHECK(name == "k");
    CHECK(string == "s");
    CHECK(assigned.root().as_object()->find("k")->value.as_array()->size() == 2);
}

TEST_CASE("the root is the value the text holds", "[documents]") {
    const auto document = parsed(R"(  ["a"])");
    CHECK(document.root().kind() == Kind::array);
    CHECK(document.root().offset() == 2);
}
