#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace pedantjson {

namespace {

/// The text of a number, taken apart.
struct Parts {
    /// Whether it begins with a minus sign.
    bool negative = false;

    /// The digits before the point.
    std::string_view integer;

    /// The digits after the point, if there is one.
    std::string_view fraction;

    /// The exponent, or 0 if there is none. One too large for a `std::ptrdiff_t` is held as the
    /// largest one with its sign: no text is that long, so it still outweighs every digit.
    std::ptrdiff_t exponent = 0;
};

/// Takes apart the text of a number, which follows JSON's grammar.
///
/// \param text The text.
/// \return Its parts.
[[nodiscard]] Parts parts_of(std::string_view text) {
    Parts parts;
    if (text.starts_with('-')) {
        parts.negative = true;
        text.remove_prefix(1);
    }
    const auto digits = [&text] {
        const std::string_view run = text.substr(0, text.find_first_not_of("0123456789"));
        text.remove_prefix(run.size());
        return run;
    };
    parts.integer = digits();
    if (text.starts_with('.')) {
        text.remove_prefix(1);
        parts.fraction = digits();
    }
    if (text.empty()) {
        return parts;
    }
    // What is left is the exponent: its letter, perhaps a sign, and digits.
    text.remove_prefix(1);
    const bool negative = text.starts_with('-');
    if (negative || text.starts_with('+')) {
        text.remove_prefix(1);
    }
    constexpr std::ptrdiff_t largest = std::numeric_limits<std::ptrdiff_t>::max();
    std::ptrdiff_t magnitude = 0;
    for (const char c : text) {
        const std::ptrdiff_t digit = c - '0';
        if (magnitude > (largest - digit) / 10) {
            magnitude = largest;
            break;
        }
        magnitude = (magnitude * 10) + digit;
    }
    parts.exponent = negative ? -magnitude : magnitude;
    return parts;
}

/// The decimal digits of an integer, with no zero in front: the digits from the number's integer
/// part, then those from its fraction, then `zeros` zeros. Zero has no digits at all.
struct IntegerDigits {
    /// The digits from the integer part.
    std::string_view integer_part;

    /// The digits from the fraction.
    std::string_view fraction_part;

    /// How many zeros end the integer.
    std::size_t zeros = 0;
};

/// The digits of a number's value, if its value is an integer.
///
/// \param parts The number.
/// \return The digits of its magnitude, or nothing if the value has a fraction.
[[nodiscard]] std::optional<IntegerDigits> integer_digits(const Parts& parts) {
    std::string_view integer = parts.integer;
    std::string_view fraction = parts.fraction;
    // Zeros that end the fraction add nothing to the value.
    fraction = fraction.substr(0, fraction.find_last_not_of('0') + 1);
    // How many places after the point the last digit that isn't zero stands, or as a negative
    // count, how many places before it.
    std::ptrdiff_t places = 0;
    if (!fraction.empty()) {
        places = static_cast<std::ptrdiff_t>(fraction.size());
    } else {
        const std::size_t last = integer.find_last_not_of('0');
        if (last == std::string_view::npos) {
            return IntegerDigits{};
        }
        places = -static_cast<std::ptrdiff_t>(integer.size() - last - 1);
        integer = integer.substr(0, last + 1);
    }
    // The exponent moves the point. Unless it moves it past that digit, the value has a fraction.
    if (parts.exponent < places) {
        return std::nullopt;
    }
    // An integer part that is a lone zero adds nothing, and nor do the zeros that begin the
    // fraction after it.
    if (integer == "0") {
        integer = {};
        fraction.remove_prefix(fraction.find_first_not_of('0'));
    }
    // The difference is at least 0 and at most the largest std::size_t, so the unsigned
    // arithmetic, which wraps, gives it exactly.
    const std::size_t zeros =
        static_cast<std::size_t>(parts.exponent) - static_cast<std::size_t>(places);
    return IntegerDigits{.integer_part = integer, .fraction_part = fraction, .zeros = zeros};
}

/// A number that can't be read into a type, for the reason given.
///
/// \param code The reason.
/// \return The error.
[[nodiscard]] std::unexpected<NumberError> fail(NumberErrorCode code) noexcept {
    return std::unexpected(NumberError{.code = code});
}

/// Reads a number into an integer type.
///
/// \tparam T The type.
/// \param text The number's text.
/// \return The number, or why `T` can't hold it.
template <typename T>
[[nodiscard]] std::expected<T, NumberError> integer_of(std::string_view text) {
    const Parts parts = parts_of(text);
    const std::optional<IntegerDigits> digits = integer_digits(parts);
    if (!digits) {
        return fail(NumberErrorCode::not_an_integer);
    }
    const std::size_t size = digits->integer_part.size() + digits->fraction_part.size();
    if (size == 0) {
        return T{0};
    }
    if (parts.negative && std::is_unsigned_v<T>) {
        return fail(NumberErrorCode::out_of_range);
    }
    // No value of T has more digits than this, so a number with more is out of its range, however
    // many it has.
    constexpr std::size_t most_digits = std::numeric_limits<T>::digits10 + 1;
    if (size > most_digits || digits->zeros > most_digits - size) {
        return fail(NumberErrorCode::out_of_range);
    }
    // The integer written out, sign and all, for std::from_chars to check against T's range.
    std::array<char, most_digits + 1> written{};
    auto end = written.begin();
    const std::string_view sign = parts.negative ? "-" : "";
    for (const std::string_view run : {sign, digits->integer_part, digits->fraction_part}) {
        end = std::ranges::copy(run, end).out;
    }
    end = std::ranges::fill_n(end, static_cast<std::ptrdiff_t>(digits->zeros), '0');
    T value{};
    if (std::from_chars(written.data(), std::to_address(end), value).ec != std::errc{}) {
        return fail(NumberErrorCode::out_of_range);
    }
    return value;
}

/// Reads a number into a floating-point type.
///
/// \tparam T The type.
/// \param text The number's text.
/// \return The value of `T` nearest the number, or why `T` can't hold it.
template <typename T>
[[nodiscard]] std::expected<T, NumberError> floating_point_of(std::string_view text) {
    T value{};
    const auto read =
        std::from_chars(std::to_address(text.begin()), std::to_address(text.end()), value);
    // A number that rounds beyond T's largest finite value, or rounds to zero without being zero,
    // is out of its range. JSON's grammar for numbers is a part of std::from_chars's, so nothing
    // else can go wrong.
    if (read.ec != std::errc{}) {
        return fail(NumberErrorCode::out_of_range);
    }
    return value;
}

} // namespace

std::string_view NumberError::message() const noexcept {
    switch (code) {
    case NumberErrorCode::out_of_range:
        return "number out of range";
    case NumberErrorCode::not_an_integer:
        return "number not an integer";
    }
    // Only a code cast from outside the enumeration gets here.
    return "number not read";
}

bool Number::is_integer() const noexcept { return integer_digits(parts_of(text_)).has_value(); }

template <NumberType T> std::expected<T, NumberError> Number::to() const noexcept {
    if constexpr (std::is_floating_point_v<T>) {
        return floating_point_of<T>(text_);
    } else {
        return integer_of<T>(text_);
    }
}

template std::expected<signed char, NumberError> Number::to<signed char>() const noexcept;
template std::expected<short, NumberError> Number::to<short>() const noexcept;
template std::expected<int, NumberError> Number::to<int>() const noexcept;
template std::expected<long, NumberError> Number::to<long>() const noexcept;
template std::expected<long long, NumberError> Number::to<long long>() const noexcept;
template std::expected<unsigned char, NumberError> Number::to<unsigned char>() const noexcept;
template std::expected<unsigned short, NumberError> Number::to<unsigned short>() const noexcept;
template std::expected<unsigned int, NumberError> Number::to<unsigned int>() const noexcept;
template std::expected<unsigned long, NumberError> Number::to<unsigned long>() const noexcept;
template std::expected<unsigned long long, NumberError>
Number::to<unsigned long long>() const noexcept;
template std::expected<float, NumberError> Number::to<float>() const noexcept;
template std::expected<double, NumberError> Number::to<double>() const noexcept;

} // namespace pedantjson
