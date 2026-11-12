#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>

using pedantjson::write;

TEST_CASE("a document's value is written compactly, its numbers in the text they were read with",
          "[writing]") {
    auto [text, written] = GENERATE(table<std::string, std::string>({
        {"null", "null"},
        {"true", "true"},
        {" false ", "false"},
        {"-1.50E+3", "-1.50E+3"},
        {"1e400", "1e400"},
        {R"("text")", R"("text")"},
        {"[ ]", "[]"},
        {"{ }", "{}"},
        {"[ 1 , [ 2 , { } ] , null ]", "[1,[2,{}],null]"},
        {R"({ "b" : 1 , "a" : [ true ] , "b" : 2.0 })", R"({"b":1,"a":[true],"b":2.0})"},
    }));
    CAPTURE(text);
    CHECK(write(support::parsed(text).root()) == written);
}

TEST_CASE("an indent writes one element or member to a line, closing each bracket at its parent's "
          "indent",
          "[writing]") {
    const auto document =
        support::parsed(R"({"a": [1, [], {}, [true]], "b": {"c": null, "c": "d"}, "e": {}})");
    CHECK(write(document.root(), {.indent = 2}) == R"({
  "a": [
    1,
    [],
    {},
    [
      true
    ]
  ],
  "b": {
    "c": null,
    "c": "d"
  },
  "e": {}
})");
    CHECK(write(document.root(), {.indent = 1}) == R"({
 "a": [
  1,
  [],
  {},
  [
   true
  ]
 ],
 "b": {
  "c": null,
  "c": "d"
 },
 "e": {}
})");
    CHECK(write(document.root(), {.indent = 4}) == R"({
    "a": [
        1,
        [],
        {},
        [
            true
        ]
    ],
    "b": {
        "c": null,
        "c": "d"
    },
    "e": {}
})");
}

TEST_CASE("an indent leaves a scalar and an empty array or object on one line", "[writing]") {
    const auto text = GENERATE(as<std::string>{}, "1", R"("a b")", "[]", "{}", "null");
    CAPTURE(text);
    CHECK(write(support::parsed(text).root(), {.indent = 2}) == text);
}

TEST_CASE("a string is written from its characters, escaped again in one form whatever the text "
          "wrote",
          "[writing]") {
    auto [text, written] = GENERATE(table<std::string, std::string>({
        {R"("a\"b")", R"("a\"b")"},
        {R"("a\u0022b")", R"("a\"b")"},
        {R"("a\\b")", R"("a\\b")"},
        {R"("a\u005Cb")", R"("a\\b")"},
        {R"("a\/b")", R"("a/b")"},
        {R"("a\u002fb")", R"("a/b")"},
        {R"("\u0008\u000C\u000a\u000D\u0009")", R"("\b\f\n\r\t")"},
        {R"("\b\f\n\r\t")", R"("\b\f\n\r\t")"},
        {R"("\u001F")", R"("\u001f")"},
        {R"("\u007f")", "\"\x7f\""},
        {R"("\u00e9")", "\"\xc3\xa9\""},
        {"\"\xc3\xa9\"", "\"\xc3\xa9\""},
        {R"("\u2028\u2029")", "\"\xe2\x80\xa8\xe2\x80\xa9\""},
        {R"("\uD83D\uDE00")", "\"\xf0\x9f\x98\x80\""},
        {R"("\u0000")", R"("\u0000")"},
    }));
    CAPTURE(text);
    CHECK(write(support::parsed(text).root()) == written);
    CHECK(write(support::parsed(text).root(), {.indent = 2}) == written);
}

TEST_CASE("each character below U+0020 is escaped, in a short form where JSON has one",
          "[writing]") {
    constexpr std::string_view hex = "0123456789abcdef";
    const std::map<int, std::string> short_forms{
        {0x08, R"(\b)"}, {0x09, R"(\t)"}, {0x0a, R"(\n)"}, {0x0c, R"(\f)"}, {0x0d, R"(\r)"},
    };
    for (int character = 0; character < 0x20; ++character) {
        CAPTURE(character);
        std::string escape = R"(\u00)";
        escape += hex.at(static_cast<std::size_t>(character / 16));
        escape += hex.at(static_cast<std::size_t>(character % 16));
        const auto found = short_forms.find(character);
        const std::string expected = found == short_forms.end() ? escape : found->second;
        CHECK(write(support::parsed("\"" + escape + "\"").root()) == "\"" + expected + "\"");
        CHECK(write(support::parsed("{\"" + escape + "\":0}").root()) ==
              "{\"" + expected + "\":0}");
    }
}

TEST_CASE("a key is escaped as a string is", "[writing]") {
    CHECK(write(support::parsed(R"({"a\/\u00e9\"\n\u0001": 1})").root()) ==
          "{\"a/\xc3\xa9\\\"\\n\\u0001\":1}");
}

TEST_CASE("any value of a document is written as text that reads back as the same tree",
          "[writing]") {
    const auto document = support::parsed(R"(
        {"name": "a\tb", "sizes": [1, 2.50, -0, 1e400, {"x": {}, "x": [[]]}], "": null,
         "flags": {"on": true, "off": false, "on": false}})");
    const pedantjson::Value root = document.root();
    const auto object = support::required(root.as_object());
    const auto sizes = support::required(support::required(object.find("sizes")).value.as_array());
    const auto indent = GENERATE(std::optional<unsigned>{}, 1U, 2U, 4U);
    CAPTURE(indent);
    for (const pedantjson::Value value : {
             root,
             support::required(object.find("flags")).value,
             support::element(sizes, 4),
             support::element(sizes, 3),
             support::required(object.find("name")).value,
         }) {
        CAPTURE(value.offset());
        support::check_same_tree(value, support::parsed(write(value, {.indent = indent})).root());
    }
}

TEST_CASE("the text ends at the value's last character, and never begins with a byte-order mark",
          "[writing]") {
    const auto text = GENERATE(as<std::string>{}, "[1]", "{\"a\": 1}", "\"a\"", "7");
    const auto indent = GENERATE(std::optional<unsigned>{}, 2U);
    CAPTURE(text, indent);
    const auto document = support::parsed(support::byte_order_mark() + text + "\n");
    const std::string written = write(document.root(), {.indent = indent});
    REQUIRE_FALSE(written.empty());
    CHECK(written.front() == text.front());
    CHECK(written.back() == text.back());
}
