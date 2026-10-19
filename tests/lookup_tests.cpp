#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <string_view>

using pedantjson::Document;
using pedantjson::Pointer;
using support::parsed;
using support::required;

namespace {

/// The pointer a text writes, which must be one.
Pointer parsed_pointer(const std::string& text) {
    const auto read = Pointer::parse(text);
    REQUIRE(read.has_value());
    return *read;
}

/// The text of the number that a pointer names in a document, which must have one there.
std::string_view number_at(const Document& document, const Pointer& pointer) {
    return required(required(document.find(pointer)).as_number()).text();
}

} // namespace

TEST_CASE("finding a pointer answers the value it names, as RFC 6901's examples have it",
          "[lookup]") {
    const auto document = parsed(R"({
      "foo": ["bar", "baz"],
      "": 0,
      "a/b": 1,
      "c%d": 2,
      "e^f": 3,
      "g|h": 4,
      "i\\j": 5,
      "k\"l": 6,
      " ": 7,
      "m~n": 8
   })");

    CHECK(required(document.find(Pointer{})).offset() == document.root().offset());
    const auto foo = required(document.find(parsed_pointer("/foo")));
    CHECK(required(foo.as_array()).size() == 2);
    CHECK(required(document.find(parsed_pointer("/foo/0"))).as_string() == "bar");

    auto [text, number] = GENERATE(table<std::string, std::string>({
        {"/", "0"},
        {"/a~1b", "1"},
        {"/c%d", "2"},
        {"/e^f", "3"},
        {"/g|h", "4"},
        {"/i\\j", "5"},
        {"/k\"l", "6"},
        {"/ ", "7"},
        {"/m~0n", "8"},
    }));
    CAPTURE(text);
    CHECK(number_at(document, parsed_pointer(text)) == number);
}

TEST_CASE("finding a pointer the document has no value for answers nothing", "[lookup]") {
    const auto document = parsed(R"({"foo": ["bar", "baz"], "a/b": 1, "n": null})");
    const auto text =
        GENERATE(as<std::string>{}, "/bar", "/Foo", "/foo/2", "/foo/-", "/foo/01", "/foo/00",
                 "/foo/+1", "/foo/-1", "/foo/1e0", "/foo/ 1", "/foo/1 ", "/foo/x", "/foo/",
                 "/foo/0/x", "/foo/0/0", "/foo/18446744073709551616",
                 "/foo/99999999999999999999999999", "/a/b", "/a~1b/0", "/n/0", "/n/x", "//");
    CAPTURE(text);
    CHECK_FALSE(document.find(parsed_pointer(text)).has_value());
}

TEST_CASE("an index names an element of an array, and a name of digits a member of an object",
          "[lookup]") {
    const auto document = parsed(R"({"0": [[1, 2], [3]], "1": true})");
    CHECK(number_at(document, Pointer{} / 0 / 1 / 0) == "3");
    CHECK(number_at(document, Pointer{} / 0 / 0 / 1) == "2");
    CHECK(required(document.find(Pointer{} / "1")).as_bool() == true);
}

TEST_CASE("each escape in a pointer is decoded once, so ~01 names a tilde and a 1", "[lookup]") {
    const auto document = parsed(R"({"/": 1, "~1": 2, "~/": 3, "/0": 4})");
    CHECK(number_at(document, parsed_pointer("/~01")) == "2");
    CHECK(number_at(document, parsed_pointer("/~0~1")) == "3");
    CHECK(number_at(document, parsed_pointer("/~10")) == "4");
}

TEST_CASE("a pointer through a repeated name follows its first member", "[lookup]") {
    const auto document = parsed(R"({"a": {"x": 1}, "a": {"y": 2}})");
    CHECK(number_at(document, Pointer{} / "a" / "x") == "1");
    CHECK_FALSE(document.find(Pointer{} / "a" / "y").has_value());
}

TEST_CASE("a pointer names a member by the name its key decodes to", "[lookup]") {
    const auto document = parsed(R"({"a\/b": 1, "\"": 2, "\t": 3})");
    CHECK(number_at(document, Pointer{} / "a/b") == "1");
    CHECK(number_at(document, Pointer{} / "\"") == "2");
    CHECK(number_at(document, Pointer{} / "\t") == "3");
    CHECK_FALSE(document.find(Pointer{} / "a\\/b").has_value());
}

TEST_CASE("the nearest value of a pointer is the deepest on its way that the document has",
          "[lookup]") {
    const auto document =
        parsed(R"({"windows": [{"size": {"width": 640}}, {"tabs": []}], "name": "x"})");
    auto [text, nearest] = GENERATE(table<std::string, std::string>({
        {"", ""},
        {"/windows/0/size/width", "/windows/0/size/width"},
        {"/windows/0/size/height", "/windows/0/size"},
        {"/windows/0/position/x", "/windows/0"},
        {"/windows/2/size", "/windows"},
        {"/windows/-", "/windows"},
        {"/windows/01", "/windows"},
        {"/windows/1/tabs/0/title", "/windows/1/tabs"},
        {"/name/0", "/name"},
        {"/missing/deeper/still", ""},
    }));
    CAPTURE(text);
    const auto expected = required(document.find(parsed_pointer(nearest)));
    CHECK(document.nearest(parsed_pointer(text)).offset() == expected.offset());
}

TEST_CASE("a pointer through a repeated name is nearest the first member's value", "[lookup]") {
    const auto document = parsed(R"({"a": {"x": 1}, "a": {"y": 2}})");
    CHECK(document.nearest(Pointer{} / "a" / "y").offset() == 6);
}

TEST_CASE("a value the text never wrote can take the position of its nearest value", "[lookup]") {
    const auto document = parsed(R"({
  "windows": [
    {"title": "main", "size": {"width": 640}}
  ]
})");
    const auto x = Pointer{} / "windows" / 0 / "position" / "x";
    CHECK_FALSE(document.find(x).has_value());
    CHECK(document.position(document.nearest(x).offset()) ==
          pedantjson::Position{.line = 3, .column = 5});
}
