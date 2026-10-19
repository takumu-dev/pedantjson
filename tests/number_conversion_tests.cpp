#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

using pedantjson::NumberError;
using pedantjson::NumberErrorCode;
using support::number_as;
using support::number_error;
using support::parsed;

namespace {

/// The integers just beyond an integer type's range.
struct Beyond {
    /// One less than its least value.
    std::string below;

    /// One more than its greatest value.
    std::string above;
};

/// The integers just beyond the range of the integer type `T`, whatever its width on the platform.
template <typename T> Beyond beyond_range() {
    constexpr bool is_signed = std::is_signed_v<T>;
    if constexpr (sizeof(T) == 1) {
        return is_signed ? Beyond{.below = "-129", .above = "128"}
                         : Beyond{.below = "-1", .above = "256"};
    } else if constexpr (sizeof(T) == 2) {
        return is_signed ? Beyond{.below = "-32769", .above = "32768"}
                         : Beyond{.below = "-1", .above = "65536"};
    } else if constexpr (sizeof(T) == 4) {
        return is_signed ? Beyond{.below = "-2147483649", .above = "2147483648"}
                         : Beyond{.below = "-1", .above = "4294967296"};
    } else {
        static_assert(sizeof(T) == 8);
        return is_signed ? Beyond{.below = "-9223372036854775809", .above = "9223372036854775808"}
                         : Beyond{.below = "-1", .above = "18446744073709551616"};
    }
}

/// Every standard integer type, whatever its width on the platform.
using IntegerTypes = std::tuple<signed char, short, int, long, long long, unsigned char,
                                unsigned short, unsigned int, unsigned long, unsigned long long>;

/// Every floating-point type a number reads into.
using FloatingTypes = std::tuple<float, double>;

/// Whether a number can be read into `T`.
template <typename T>
concept Readable = requires(const pedantjson::Number& number) { number.to<T>(); };

} // namespace

TEST_CASE("an integer reads into an integer type", "[conversions]") {
    const auto document = parsed("42");
    const auto number = document.root().as_number();
    REQUIRE(number.has_value());
    CHECK(number->to<int>() == 42);
}

TEMPLATE_LIST_TEST_CASE("an integer type reads every integer from its least to its greatest",
                        "[conversions]", IntegerTypes) {
    using Limits = std::numeric_limits<TestType>;
    CHECK(number_as<TestType>(std::to_string(Limits::min())) == Limits::min());
    CHECK(number_as<TestType>(std::to_string(Limits::max())) == Limits::max());
    CHECK(number_as<TestType>("0") == TestType{0});
    CHECK(number_as<TestType>("1") == TestType{1});
}

TEMPLATE_LIST_TEST_CASE("an integer beyond an integer type's limits is out of its range",
                        "[conversions]", IntegerTypes) {
    const auto [below, above] = beyond_range<TestType>();
    CHECK(number_error<TestType>(below) == NumberErrorCode::out_of_range);
    CHECK(number_error<TestType>(above) == NumberErrorCode::out_of_range);
    CHECK(number_error<TestType>("123456789012345678901234567890") ==
          NumberErrorCode::out_of_range);
    CHECK(number_error<TestType>("-123456789012345678901234567890") ==
          NumberErrorCode::out_of_range);
}

TEMPLATE_LIST_TEST_CASE("zero reads into an integer type as zero, whatever its sign or exponent",
                        "[conversions]", IntegerTypes) {
    auto text = GENERATE(as<std::string>{}, "0.0", "0e5", "0e-5", "0.000e-400", "-0", "-0.0",
                         "-0e5", "-0.00E-5");
    CAPTURE(text);
    CHECK(number_as<TestType>(text) == TestType{0});
}

TEST_CASE("a number written with a fraction or an exponent reads as an integer if its value is one",
          "[conversions]") {
    auto [text, integer] = GENERATE(table<std::string, int>({
        {"1.0", 1},
        {"1.000", 1},
        {"-1.0", -1},
        {"1e0", 1},
        {"1e2", 100},
        {"1E2", 100},
        {"1e+2", 100},
        {"-1e2", -100},
        {"1.5e1", 15},
        {"1.50e1", 15},
        {"12.30e1", 123},
        {"150e-1", 15},
        {"100e-2", 1},
        {"1020e-1", 102},
        {"0.1e1", 1},
        {"0.000123e6", 123},
        {"0.00000000000000000000000000001e29", 1},
    }));
    CAPTURE(text);
    CHECK(number_as<int>(text) == integer);
}

TEMPLATE_LIST_TEST_CASE("a number whose value has a fraction isn't an integer", "[conversions]",
                        IntegerTypes) {
    auto text = GENERATE(as<std::string>{}, "0.5", "1.5", "-1.5", "0.1", "1e-1", "150e-2",
                         "1.0000000000000001", "9007199254740991.5", "1e-400", "5e-324",
                         "123456789012345678901234567890.5", "-0.000001");
    CAPTURE(text);
    CHECK(number_error<TestType>(text) == NumberErrorCode::not_an_integer);
}

TEMPLATE_LIST_TEST_CASE("an integer written with a fraction is held to an integer type's limits",
                        "[conversions]", IntegerTypes) {
    using Limits = std::numeric_limits<TestType>;
    const std::string least = std::to_string(Limits::min());
    const std::string greatest = std::to_string(Limits::max());
    const auto [below, above] = beyond_range<TestType>();
    CHECK(number_as<TestType>(least + ".0") == Limits::min());
    CHECK(number_as<TestType>(greatest + ".000") == Limits::max());
    CHECK(number_as<TestType>(greatest + "0e-1") == Limits::max());
    CHECK(number_error<TestType>(below + ".0") == NumberErrorCode::out_of_range);
    CHECK(number_error<TestType>(above + ".0") == NumberErrorCode::out_of_range);
}

TEST_CASE("an integer written with an exponent is exact to the last digit", "[conversions]") {
    using Signed = std::int64_t;
    using Unsigned = std::uint64_t;
    CHECK(number_as<Signed>("9.223372036854775807e18") == std::numeric_limits<Signed>::max());
    CHECK(number_as<Signed>("-9.223372036854775808e18") == std::numeric_limits<Signed>::min());
    CHECK(number_as<Unsigned>("1.8446744073709551615e19") == std::numeric_limits<Unsigned>::max());
    CHECK(number_as<Signed>("9007199254740993.0") == Signed{9'007'199'254'740'993});
    CHECK(number_as<Signed>("-9.007199254740993e15") == Signed{-9'007'199'254'740'993});
    CHECK(number_as<Signed>("1e18") == Signed{1'000'000'000'000'000'000});
    CHECK(number_as<Unsigned>("1e19") == Unsigned{10'000'000'000'000'000'000U});
    CHECK(number_as<Signed>("100000000000000000000000000000e-28") == Signed{10});
    CHECK(number_error<Signed>("9.223372036854775808e18") == NumberErrorCode::out_of_range);
    CHECK(number_error<Signed>("-9.223372036854775809e18") == NumberErrorCode::out_of_range);
    CHECK(number_error<Unsigned>("1.8446744073709551616e19") == NumberErrorCode::out_of_range);
    CHECK(number_error<Signed>("1e19") == NumberErrorCode::out_of_range);
    CHECK(number_error<Unsigned>("1e20") == NumberErrorCode::out_of_range);
}

TEMPLATE_LIST_TEST_CASE("an integer too large for any type is out of every integer type's range",
                        "[conversions]", IntegerTypes) {
    auto text = GENERATE(as<std::string>{}, "1e400", "-1e400", "1.5e400", "1e99999999999999999999",
                         "-1e99999999999999999999", "1e+99999999999999999999999999999999999999",
                         "123456789012345678901234567890e10");
    CAPTURE(text);
    CHECK(number_error<TestType>(text) == NumberErrorCode::out_of_range);
}

TEMPLATE_LIST_TEST_CASE("an exponent of any size still decides whether a value has a fraction",
                        "[conversions]", IntegerTypes) {
    CHECK(number_error<TestType>("1e-99999999999999999999999999999999999999") ==
          NumberErrorCode::not_an_integer);
    CHECK(number_error<TestType>("-12.5e-99999999999999999999") == NumberErrorCode::not_an_integer);
    CHECK(number_as<TestType>("0e99999999999999999999999999999999999999") == TestType{0});
    CHECK(number_as<TestType>("-0.0e-99999999999999999999999999999999999999") == TestType{0});
}

TEST_CASE("a number says whether its value is an integer, whatever its size", "[conversions]") {
    auto [text, integer] = GENERATE(table<std::string, bool>({
        {"0", true},
        {"-0", true},
        {"42", true},
        {"123456789012345678901234567890", true},
        {"1.0", true},
        {"-0.0", true},
        {"1e2", true},
        {"150e-1", true},
        {"1e400", true},
        {"1e99999999999999999999", true},
        {"0e-99999999999999999999", true},
        {"0.5", false},
        {"-1.5", false},
        {"1e-1", false},
        {"150e-2", false},
        {"1.0000000000000001", false},
        {"1e-400", false},
        {"1e-99999999999999999999", false},
    }));
    CAPTURE(text);
    const auto document = parsed(text);
    const auto number = document.root().as_number();
    REQUIRE(number.has_value());
    CHECK(number->is_integer() == integer);
}

TEST_CASE("a number reads into a floating-point type as the nearest value the type holds",
          "[conversions]") {
    CHECK(number_as<float>("0") == 0.0F);
    CHECK(number_as<float>("1") == 1.0F);
    CHECK(number_as<float>("-1.5") == -1.5F);
    CHECK(number_as<float>("0.1") == 0.1F);
    CHECK(number_as<float>("1e10") == 1e10F);
    CHECK(number_as<float>("123456789") == 123'456'792.0F);
    CHECK(number_as<double>("0") == 0.0);
    CHECK(number_as<double>("1") == 1.0);
    CHECK(number_as<double>("-1.5") == -1.5);
    CHECK(number_as<double>("0.1") == 0.1);
    CHECK(number_as<double>("6.02214076e23") == 6.02214076e23);
    CHECK(number_as<double>("9007199254740993") == 9'007'199'254'740'992.0);
}

TEST_CASE("a floating-point type reads every number that rounds to a value within its limits",
          "[conversions]") {
    using FloatLimits = std::numeric_limits<float>;
    CHECK(number_as<float>("3.4028234663852886e38") == FloatLimits::max());
    CHECK(number_as<float>("3.4028235e38") == FloatLimits::max());
    CHECK(number_as<float>("-3.4028234663852886e38") == FloatLimits::lowest());
    CHECK(number_as<float>("1.1754943508222875e-38") == FloatLimits::min());
    CHECK(number_as<float>("1.401298464324817e-45") == FloatLimits::denorm_min());
    CHECK(number_as<float>("-1.401298464324817e-45") == -FloatLimits::denorm_min());
    using DoubleLimits = std::numeric_limits<double>;
    CHECK(number_as<double>("1.7976931348623157e308") == DoubleLimits::max());
    CHECK(number_as<double>("1.7976931348623158e308") == DoubleLimits::max());
    CHECK(number_as<double>("-1.7976931348623157e308") == DoubleLimits::lowest());
    CHECK(number_as<double>("2.2250738585072014e-308") == DoubleLimits::min());
    CHECK(number_as<double>("4.9406564584124654e-324") == DoubleLimits::denorm_min());
    CHECK(number_as<double>("-4.9406564584124654e-324") == -DoubleLimits::denorm_min());
}

TEST_CASE(
    "a number that rounds beyond a floating-point type's limits, or rounds to zero, is out of "
    "its range",
    "[conversions]") {
    SECTION("float") {
        auto text = GENERATE(as<std::string>{}, "3.4028236e38", "-3.4028236e38", "1e39", "1e-46",
                             "-1e-46", "1e99999999999999999999", "1e-99999999999999999999");
        CAPTURE(text);
        CHECK(number_error<float>(text) == NumberErrorCode::out_of_range);
    }
    SECTION("double") {
        auto text = GENERATE(as<std::string>{}, "1.7976931348623159e308", "-1.7976931348623159e308",
                             "1e309", "2e-324", "-2e-324", "1e-400", "1e99999999999999999999",
                             "1e-99999999999999999999");
        CAPTURE(text);
        CHECK(number_error<double>(text) == NumberErrorCode::out_of_range);
    }
}

TEMPLATE_LIST_TEST_CASE(
    "zero reads into a floating-point type with its sign, whatever its exponent", "[conversions]",
    FloatingTypes) {
    auto [text, negative] = GENERATE(table<std::string, bool>({
        {"0", false},
        {"0.0", false},
        {"0e99999999999999999999", false},
        {"-0", true},
        {"-0.0", true},
        {"-0e5", true},
        {"-0.000e-99999999999999999999", true},
    }));
    CAPTURE(text);
    const auto zero = number_as<TestType>(text);
    REQUIRE(zero.has_value());
    CHECK(*zero == TestType{0});
    CHECK(std::signbit(*zero) == negative);
}

TEST_CASE("a number reads into the standard integer types, float and double, and nothing else",
          "[conversions]") {
    STATIC_CHECK(Readable<signed char>);
    STATIC_CHECK(Readable<long long>);
    STATIC_CHECK(Readable<unsigned char>);
    STATIC_CHECK(Readable<unsigned long long>);
    STATIC_CHECK(Readable<std::int8_t>);
    STATIC_CHECK(Readable<std::uint64_t>);
    STATIC_CHECK(Readable<std::size_t>);
    STATIC_CHECK(Readable<float>);
    STATIC_CHECK(Readable<double>);
    STATIC_CHECK_FALSE(Readable<bool>);
    STATIC_CHECK_FALSE(Readable<char>);
    STATIC_CHECK_FALSE(Readable<wchar_t>);
    STATIC_CHECK_FALSE(Readable<char8_t>);
    STATIC_CHECK_FALSE(Readable<char16_t>);
    STATIC_CHECK_FALSE(Readable<char32_t>);
    STATIC_CHECK_FALSE(Readable<long double>);
    STATIC_CHECK_FALSE(Readable<const int>);
    STATIC_CHECK_FALSE(Readable<int&>);
    STATIC_CHECK_FALSE(Readable<std::string>);
}

TEST_CASE("every number error code has a message of its own", "[conversions]") {
    const auto codes = {NumberErrorCode::out_of_range, NumberErrorCode::not_an_integer};
    std::set<std::string_view> messages;
    for (const auto code : codes) {
        const std::string_view message = NumberError{.code = code}.message();
        CAPTURE(code, message);
        CHECK_FALSE(message.empty());
        messages.insert(message);
    }
    CHECK(messages.size() == codes.size());
    const auto number = number_as<int>("1.5");
    REQUIRE_FALSE(number.has_value());
    CHECK(number.error().message() == "number not an integer");
}
