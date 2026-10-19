#pragma once

#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>

#include <expected>
#include <string>
#include <utility>

CATCH_REGISTER_ENUM(pedantjson::Kind, pedantjson::Kind::null, pedantjson::Kind::boolean,
                    pedantjson::Kind::number, pedantjson::Kind::string, pedantjson::Kind::array,
                    pedantjson::Kind::object)

CATCH_REGISTER_ENUM(
    pedantjson::SyntaxErrorCode, pedantjson::SyntaxErrorCode::empty_input,
    pedantjson::SyntaxErrorCode::unexpected_character, pedantjson::SyntaxErrorCode::unexpected_end,
    pedantjson::SyntaxErrorCode::invalid_number, pedantjson::SyntaxErrorCode::unterminated_string,
    pedantjson::SyntaxErrorCode::invalid_escape, pedantjson::SyntaxErrorCode::unpaired_surrogate,
    pedantjson::SyntaxErrorCode::control_character_in_string,
    pedantjson::SyntaxErrorCode::invalid_utf8, pedantjson::SyntaxErrorCode::too_deep,
    pedantjson::SyntaxErrorCode::trailing_content)

CATCH_REGISTER_ENUM(pedantjson::NumberErrorCode, pedantjson::NumberErrorCode::out_of_range,
                    pedantjson::NumberErrorCode::not_an_integer)

/// Prints a number read into a type as the value read, or as the code of the error.
template <typename T> struct Catch::StringMaker<std::expected<T, pedantjson::NumberError>> {
    static std::string convert(const std::expected<T, pedantjson::NumberError>& number) {
        return number ? Catch::Detail::stringify(*number)
                      : Catch::Detail::stringify(number.error().code);
    }
};

namespace support {

/// Reads a text that must be JSON, failing the test if it isn't.
inline pedantjson::Document parsed(std::string text, pedantjson::ParseOptions options = {}) {
    auto document = pedantjson::parse(std::move(text), options);
    if (!document) {
        FAIL("not JSON: " << document.error().message() << " at offset "
                          << document.error().offset);
    }
    return std::move(*document);
}

/// Reads a text that must not be JSON, failing the test if it is.
inline pedantjson::SyntaxError syntax_error(std::string text,
                                            pedantjson::ParseOptions options = {}) {
    auto document = pedantjson::parse(std::move(text), options);
    REQUIRE_FALSE(document.has_value());
    return document.error();
}

/// Reads a text that must be a number into the type `T`.
template <typename T> std::expected<T, pedantjson::NumberError> number_as(std::string text) {
    const auto document = parsed(std::move(text));
    const auto number = document.root().as_number();
    REQUIRE(number.has_value());
    return number->to<T>();
}

/// Reads a text that must be a number into the type `T`, which must fail, failing the test if it
/// doesn't.
template <typename T> pedantjson::NumberErrorCode number_error(std::string text) {
    const auto number = number_as<T>(std::move(text));
    REQUIRE_FALSE(number.has_value());
    return number.error().code;
}

} // namespace support
