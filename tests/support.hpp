#pragma once

#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>

#include <cstddef>
#include <expected>
#include <optional>
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

CATCH_REGISTER_ENUM(pedantjson::PointerErrorCode, pedantjson::PointerErrorCode::missing_slash,
                    pedantjson::PointerErrorCode::invalid_escape)

CATCH_REGISTER_ENUM(pedantjson::NumberErrorCode, pedantjson::NumberErrorCode::out_of_range,
                    pedantjson::NumberErrorCode::not_an_integer)

/// Prints a number read into a type as the value read, or as the code of the error.
template <typename T> struct Catch::StringMaker<std::expected<T, pedantjson::NumberError>> {
    static std::string convert(const std::expected<T, pedantjson::NumberError>& number) {
        return number ? Catch::Detail::stringify(*number)
                      : Catch::Detail::stringify(number.error().code);
    }
};

/// Prints a position as its line and its column, such as `3:7`.
template <> struct Catch::StringMaker<pedantjson::Position> {
    static std::string convert(const pedantjson::Position& position) {
        return std::to_string(position.line) + ":" + std::to_string(position.column);
    }
};

/// Prints a pointer as its text.
template <> struct Catch::StringMaker<pedantjson::Pointer> {
    static std::string convert(const pedantjson::Pointer& pointer) {
        return Catch::Detail::stringify(pointer.text());
    }
};

namespace support {

/// The value of an optional that must hold one, failing the test if it doesn't.
template <typename T> T required(std::optional<T> value) {
    REQUIRE(value.has_value());
    // REQUIRE has ended the test if the optional is empty, which clang-tidy can't see.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access): see above
    return *std::move(value);
}

/// The element of an array at an index the array must have, failing the test if it hasn't.
inline pedantjson::Value element(const pedantjson::Array& array, std::size_t index) {
    REQUIRE(index < array.size());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): checked above
    return array[index];
}

/// The member of an object at an index the object must have, failing the test if it hasn't.
inline pedantjson::Member member(const pedantjson::Object& object, std::size_t index) {
    REQUIRE(index < object.size());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): checked above
    return object[index];
}

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
    REQUIRE(!document.has_value());
    return document.error();
}

/// Reads a text that must be a number into the type `T`.
template <typename T> std::expected<T, pedantjson::NumberError> number_as(std::string text) {
    const auto document = parsed(std::move(text));
    return required(document.root().as_number()).template to<T>();
}

/// Reads a text that must be a number into the type `T`, which must fail, failing the test if it
/// doesn't.
template <typename T> pedantjson::NumberErrorCode number_error(std::string text) {
    const auto number = number_as<T>(std::move(text));
    REQUIRE(!number.has_value());
    return number.error().code;
}

} // namespace support
