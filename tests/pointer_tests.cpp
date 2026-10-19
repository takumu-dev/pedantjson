#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <string>
#include <string_view>

using pedantjson::Pointer;
using pedantjson::PointerError;
using pedantjson::PointerErrorCode;

TEST_CASE("a default pointer is the empty string, which names the whole document", "[pointers]") {
    CHECK(Pointer{}.text().empty());
}

TEST_CASE("a name or an index after a pointer adds a slash and a reference token", "[pointers]") {
    CHECK((Pointer{} / "windows").text() == "/windows");
    CHECK((Pointer{} / "windows" / 0 / "size").text() == "/windows/0/size");
    CHECK((Pointer{} / 12 / 345).text() == "/12/345");
    CHECK((Pointer{} / std::size_t{4294967295}).text() == "/4294967295");
    CHECK((Pointer{} / "").text() == "/");
    CHECK((Pointer{} / "" / "").text() == "//");
}

TEST_CASE("a name's tildes and slashes are escaped, and nothing else is", "[pointers]") {
    auto [name, token] = GENERATE(table<std::string, std::string>({
        {"a/b", "a~1b"},
        {"m~n", "m~0n"},
        {"~1", "~01"},
        {"~0", "~00"},
        {"/~", "~1~0"},
        {"~/", "~0~1"},
        {"//", "~1~1"},
        {"c%d", "c%d"},
        {" ", " "},
        {"\\", "\\"},
        {"\"", "\""},
        {"#", "#"},
        {"0", "0"},
        {"-", "-"},
        {"\xc3\xa9", "\xc3\xa9"},
        {std::string{"\0", 1}, std::string{"\0", 1}},
    }));
    CAPTURE(name);
    CHECK((Pointer{} / name).text() == "/" + token);
}

TEST_CASE("adding to a pointer leaves the pointer as it was", "[pointers]") {
    const Pointer windows = Pointer{} / "windows";
    const Pointer first = windows / 0;
    const Pointer second = windows / 1;
    CHECK(windows.text() == "/windows");
    CHECK(first.text() == "/windows/0");
    CHECK(second.text() == "/windows/1");
}

TEST_CASE("two pointers are equal when their texts are", "[pointers]") {
    CHECK(Pointer{} == Pointer{});
    CHECK(Pointer{} / "0" == Pointer{} / 0);
    CHECK(Pointer{} / "a" / "b" != Pointer{} / "a/b");
    CHECK(Pointer{} / "" != Pointer{});
}

TEST_CASE("reading a pointer's text gives the pointer that writes it", "[pointers]") {
    auto [text, pointer] = GENERATE(table<std::string, Pointer>({
        {"", Pointer{}},
        {"/", Pointer{} / ""},
        {"//", Pointer{} / "" / ""},
        {"/windows/0/size", Pointer{} / "windows" / 0 / "size"},
        {"/a~1b", Pointer{} / "a/b"},
        {"/m~0n", Pointer{} / "m~n"},
        {"/~01", Pointer{} / "~1"},
        {"/~10", Pointer{} / "/0"},
        {"/ ", Pointer{} / " "},
        {"/c%d/e^f/g|h/i\\j/k\"l", Pointer{} / "c%d" / "e^f" / "g|h" / "i\\j" / "k\"l"},
        {"/\xc3\xa9", Pointer{} / "\xc3\xa9"},
    }));
    CAPTURE(text);
    const auto read = Pointer::parse(text);
    REQUIRE(read.has_value());
    CHECK(*read == pointer);
    CHECK(read->text() == text);
}

TEST_CASE("a text that isn't empty and doesn't begin with a slash isn't a pointer", "[pointers]") {
    auto text = GENERATE(as<std::string>{}, "a", "0", "a/b", " /a", "#/a", "#", "~0");
    CAPTURE(text);
    const auto read = Pointer::parse(text);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().code == PointerErrorCode::missing_slash);
    CHECK(read.error().offset == 0);
}

TEST_CASE("a tilde that isn't followed by 0 or 1 is an invalid escape, at the tilde",
          "[pointers]") {
    auto [text, offset] = GENERATE(table<std::string, std::size_t>({
        {"/~", 1},
        {"/~2", 1},
        {"/~a", 1},
        {"/~/", 1},
        {"/~~0", 1},
        {"/a~", 2},
        {"/~0~", 3},
        {"/a/b~x", 4},
    }));
    CAPTURE(text);
    const auto read = Pointer::parse(text);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().code == PointerErrorCode::invalid_escape);
    CHECK(read.error().offset == offset);
}

TEST_CASE("every pointer error code has a message of its own", "[pointers]") {
    const std::string_view missing_slash =
        PointerError{.code = PointerErrorCode::missing_slash, .offset = 0}.message();
    const std::string_view invalid_escape =
        PointerError{.code = PointerErrorCode::invalid_escape, .offset = 0}.message();
    CHECK_FALSE(missing_slash.empty());
    CHECK_FALSE(invalid_escape.empty());
    CHECK(missing_slash != invalid_escape);
    CHECK(Pointer::parse("a").error().message() == "pointer doesn't begin with a slash");
}
