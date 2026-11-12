#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

using pedantjson::Draft;
using pedantjson::Pointer;
using pedantjson::write;
using pedantjson::WriteErrorCode;

namespace {

/// The text a draft is written as, failing the test if it is refused.
std::string written(const Draft& draft, pedantjson::WriteOptions options = {}) {
    auto text = pedantjson::write(draft, options);
    if (!text) {
        FAIL("refused: " << text.error().message() << " at " << text.error().pointer.text());
    }
    return std::move(*text);
}

/// The error a draft is refused with, failing the test if it is written.
pedantjson::WriteError refusal(const Draft& draft, pedantjson::WriteOptions options = {}) {
    auto text = pedantjson::write(draft, options);
    if (text) {
        FAIL("written: " << *text);
    }
    return std::move(text).error();
}

/// Every standard integer type, whatever its width on the platform.
using IntegerTypes = std::tuple<signed char, short, int, long, long long, unsigned char,
                                unsigned short, unsigned int, unsigned long, unsigned long long>;

/// Every floating-point type a draft is made from.
using FloatingTypes = std::tuple<float, double>;

/// The least and the greatest values of an integer type, as text.
struct Limits {
    /// Its least value.
    std::string least;

    /// Its greatest value.
    std::string greatest;
};

/// The least and the greatest values of the integer type `T`, whatever its width on the platform.
template <typename T> Limits limits() {
    constexpr bool is_signed = std::is_signed_v<T>;
    if constexpr (sizeof(T) == 1) {
        return is_signed ? Limits{.least = "-128", .greatest = "127"}
                         : Limits{.least = "0", .greatest = "255"};
    } else if constexpr (sizeof(T) == 2) {
        return is_signed ? Limits{.least = "-32768", .greatest = "32767"}
                         : Limits{.least = "0", .greatest = "65535"};
    } else if constexpr (sizeof(T) == 4) {
        return is_signed ? Limits{.least = "-2147483648", .greatest = "2147483647"}
                         : Limits{.least = "0", .greatest = "4294967295"};
    } else {
        static_assert(sizeof(T) == 8);
        return is_signed
                   ? Limits{.least = "-9223372036854775808", .greatest = "9223372036854775807"}
                   : Limits{.least = "0", .greatest = "18446744073709551615"};
    }
}

/// The bits of a floating-point value, which tell negative zero from zero.
template <typename T> auto bits_of(T value) {
    using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
    return std::bit_cast<Bits>(value);
}

/// A draft of a number written and read back into the type it was made from.
template <typename T> T read_back(T value) {
    const auto document = support::parsed(written(Draft{value}));
    const auto number = support::required(document.root().as_number()).to<T>();
    if (!number) {
        FAIL("not read back: " << number.error().message());
    }
    return *number;
}

} // namespace

TEST_CASE("a draft of null, a boolean or a string is written as that value", "[drafts]") {
    CHECK(written(Draft{}) == "null");
    CHECK(written(Draft{nullptr}) == "null");
    CHECK(written(Draft{true}) == "true");
    CHECK(written(Draft{false}) == "false");
    CHECK(written(Draft{std::string{"a\nb"}}) == R"("a\nb")");
    CHECK(written(Draft{std::string_view{"c"}}) == R"("c")");
    CHECK(written(Draft{""}) == R"("")");
}

TEST_CASE("a string literal makes a string, never a boolean", "[drafts]") {
    CHECK(written(Draft{"text"}) == R"("text")");
    CHECK(written(Draft("text")) == R"("text")");
    const char* pointer = "pointer";
    CHECK(written(Draft{pointer}) == R"("pointer")");
    const Draft assigned = "assigned";
    CHECK(written(assigned) == R"("assigned")");
    std::string characters = "mutable";
    CHECK(written(Draft{characters.data()}) == R"("mutable")");
}

TEST_CASE("no pointer but one to char makes a draft, rather than the boolean it would convert to",
          "[drafts]") {
    STATIC_CHECK(std::is_constructible_v<Draft, char*>);
    STATIC_CHECK(std::is_constructible_v<Draft, const char*>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, const char8_t*>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, const wchar_t*>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, const char16_t*>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, int*>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, const Draft*>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, void (*)()>);
}

TEST_CASE("a draft array is written with its elements in the order they were appended",
          "[drafts]") {
    Draft array = Draft::array();
    CHECK(written(array) == "[]");
    array.append(1).append("two");
    Draft inner = Draft::array();
    inner.append(nullptr);
    array.append(std::move(inner)).append(Draft::object());
    CHECK(written(array) == R"([1,"two",[null],{}])");
    CHECK(written(array, {.indent = 2}) == "[\n  1,\n  \"two\",\n  [\n    null\n  ],\n  {}\n]");
}

TEST_CASE("a draft object is written with its members in the order they were appended, "
          "duplicates included",
          "[drafts]") {
    Draft object = Draft::object();
    CHECK(written(object) == "{}");
    object.append("b", 1).append("a", true).append("b", "again");
    Draft inner = Draft::object();
    inner.append("", Draft::array());
    object.append("inner", std::move(inner));
    CHECK(written(object) == R"({"b":1,"a":true,"b":"again","inner":{"":[]}})");
    CHECK(written(object, {.indent = 1}) ==
          "{\n \"b\": 1,\n \"a\": true,\n \"b\": \"again\",\n \"inner\": {\n  \"\": []\n }\n}");
}

TEST_CASE("a draft copies and moves as a value", "[drafts]") {
    Draft original = Draft::array();
    original.append(1);
    Draft copy = original;
    copy.append(2);
    CHECK(written(original) == "[1]");
    CHECK(written(copy) == "[1,2]");

    Draft moved = std::move(copy);
    CHECK(written(moved) == "[1,2]");

    Draft assigned;
    assigned = original;
    CHECK(written(assigned) == "[1]");
    assigned = std::move(moved);
    CHECK(written(assigned) == "[1,2]");
}

TEMPLATE_LIST_TEST_CASE("an integer is written exactly, from its least to its greatest value",
                        "[drafts]", IntegerTypes) {
    constexpr TestType least = std::numeric_limits<TestType>::min();
    constexpr TestType greatest = std::numeric_limits<TestType>::max();
    CHECK(written(Draft{least}) == limits<TestType>().least);
    CHECK(written(Draft{greatest}) == limits<TestType>().greatest);
    CHECK(written(Draft{TestType{0}}) == "0");
    CHECK(written(Draft{TestType{42}}) == "42");
    CHECK(read_back(least) == least);
    CHECK(read_back(greatest) == greatest);
}

TEMPLATE_LIST_TEST_CASE("a floating-point number reads back as the same bits at its type's edges",
                        "[drafts]", FloatingTypes) {
    using Limits = std::numeric_limits<TestType>;
    const auto value = GENERATE(TestType{0}, -TestType{0}, Limits::denorm_min(),
                                -Limits::denorm_min(), Limits::min(), Limits::max(),
                                Limits::lowest(), Limits::epsilon(), TestType{1} / TestType{3},
                                static_cast<TestType>(0.1), static_cast<TestType>(-2.5e-7),
                                static_cast<TestType>(123456789), static_cast<TestType>(1e30));
    CAPTURE(value);
    CHECK(bits_of(read_back(value)) == bits_of(value));
}

TEST_CASE("a floating-point number is written in the shortest form of its own type", "[drafts]") {
    CHECK(written(Draft{0.1F}) == "0.1");
    CHECK(written(Draft{0.1}) == "0.1");
    CHECK(written(Draft{-0.0F}) == "-0");
    CHECK(written(Draft{1.5}) == "1.5");
    CHECK(written(Draft{std::numeric_limits<float>::denorm_min()}) == "1e-45");
    CHECK(written(Draft{std::numeric_limits<double>::denorm_min()}) == "5e-324");
    CHECK(written(Draft{std::numeric_limits<float>::max()}) == "3.4028235e+38");
}

TEMPLATE_LIST_TEST_CASE("a floating-point number that isn't finite is refused as not_finite",
                        "[drafts]", FloatingTypes) {
    using Limits = std::numeric_limits<TestType>;
    const auto value = GENERATE(Limits::quiet_NaN(), -Limits::quiet_NaN(), Limits::infinity(),
                                -Limits::infinity());
    const auto indent = GENERATE(std::optional<unsigned>{}, 2U);
    CAPTURE(value, indent);
    CHECK(refusal(Draft{value}, {.indent = indent}).code == WriteErrorCode::not_finite);
    CHECK(refusal(Draft{value}).pointer == Pointer{});

    Draft array = Draft::array();
    array.append(1).append(value);
    const auto error = refusal(array, {.indent = indent});
    CHECK(error.code == WriteErrorCode::not_finite);
    CHECK(error.pointer == Pointer{} / 1);
}

TEST_CASE("a string that isn't UTF-8 is refused as invalid_utf8", "[drafts]") {
    const auto string = GENERATE(as<std::string>{}, "\xff", "a\x80", "\xc0\x80", "\xc3", "\xe2\x82",
                                 "\xed\xa0\x80", "\xf4\x90\x80\x80", "ok\xf8");
    CAPTURE(string);
    const auto error = refusal(Draft{string});
    CHECK(error.code == WriteErrorCode::invalid_utf8);
    CHECK(error.pointer == Pointer{});
}

TEST_CASE("a name that isn't UTF-8 is refused at its member, before the member's value",
          "[drafts]") {
    Draft object = Draft::object();
    object.append("fine", 1).append("\xff", std::numeric_limits<double>::infinity());
    const auto error = refusal(object);
    CHECK(error.code == WriteErrorCode::invalid_utf8);
    CHECK(error.pointer == Pointer{} / "\xff");
}

TEST_CASE("the first refused value in the order of the text is the one reported", "[drafts]") {
    Draft inner = Draft::object();
    inner.append("x", 1).append("b", "\xfe");
    Draft list = Draft::array();
    list.append(true).append(std::move(inner)).append(std::numeric_limits<float>::infinity());
    Draft root = Draft::object();
    root.append("a", std::move(list)).append("c", std::numeric_limits<double>::quiet_NaN());
    const auto error = refusal(root, {.indent = 4});
    CHECK(error.code == WriteErrorCode::invalid_utf8);
    CHECK(error.pointer == Pointer{} / "a" / 1 / "b");
}

TEST_CASE("every write error code has a message of its own", "[drafts]") {
    const auto codes = {
        WriteErrorCode::not_finite,
        WriteErrorCode::invalid_utf8,
    };
    std::set<std::string_view> messages;
    for (const auto code : codes) {
        const std::string_view message =
            pedantjson::WriteError{.code = code, .pointer = {}}.message();
        CAPTURE(code, message);
        CHECK(!message.empty());
        messages.insert(message);
    }
    CHECK(messages.size() == codes.size());
    CHECK(refusal(Draft{std::numeric_limits<double>::infinity()}).message() == "number not finite");
    CHECK(refusal(Draft{"\xff"}).message() == "invalid UTF-8");
}

TEST_CASE("a draft made from a value of a document is written as that value", "[drafts]") {
    const auto document = support::parsed(R"(
        {"b": -1.50E+3, "a": [1e400, "x\/\ty", null, true, {}], "b": {"": [[]], "c\n": false}})");
    const pedantjson::Value root = document.root();
    const auto object = support::required(root.as_object());
    const auto indent = GENERATE(std::optional<unsigned>{}, 2U);
    CAPTURE(indent);
    for (const pedantjson::Value value : {
             root,
             support::member(object, 1).value,
             support::member(object, 2).value,
             support::member(object, 0).value,
         }) {
        CAPTURE(value.offset());
        CHECK(written(Draft{value}, {.indent = indent}) == write(value, {.indent = indent}));
    }
    CHECK(written(Draft{root}) ==
          R"({"b":-1.50E+3,"a":[1e400,"x/\ty",null,true,{}],"b":{"":[[]],"c\n":false}})");
}

TEST_CASE("a draft made from a value can be changed and written back", "[drafts]") {
    const auto document = support::parsed(R"({"name": "old", "sizes": [1, 2]})");
    const auto object = support::required(document.root().as_object());
    Draft changed = Draft::object();
    changed.append("name", "new");
    Draft sizes{support::required(object.find("sizes")).value};
    sizes.append(3);
    changed.append("sizes", std::move(sizes));
    CHECK(written(changed) == R"({"name":"new","sizes":[1,2,3]})");
    CHECK(write(document.root()) == R"({"name":"old","sizes":[1,2]})");
}

TEST_CASE("a draft is an ordinary value, made from a document's value only when asked",
          "[drafts]") {
    STATIC_CHECK(std::is_nothrow_move_constructible_v<Draft>);
    STATIC_CHECK(std::is_nothrow_move_assignable_v<Draft>);
    STATIC_CHECK(std::is_copy_constructible_v<Draft>);
    STATIC_CHECK(std::is_copy_assignable_v<Draft>);
    STATIC_CHECK(std::is_constructible_v<Draft, pedantjson::Value>);
    STATIC_CHECK_FALSE(std::is_convertible_v<pedantjson::Value, Draft>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, char>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, char8_t>);
    STATIC_CHECK_FALSE(std::is_constructible_v<Draft, long double>);
}

TEST_CASE("a draft of any depth is copied, written and destroyed without overflowing the stack",
          "[drafts]") {
    constexpr std::size_t depth = 1'000'000;
    Draft draft = Draft::array();
    for (std::size_t level = 1; level < depth; ++level) {
        Draft outer = Draft::array();
        outer.append(std::move(draft));
        draft = std::move(outer);
    }
    const Draft copy = draft;
    const std::string text = written(copy);
    CHECK(text == std::string(depth, '[') + std::string(depth, ']'));
    const auto document = support::parsed(text, {.max_depth = depth});
    const Draft from_value{document.root()};
    CHECK(written(from_value) == text);
}
