// libFuzzer calls LLVMFuzzerTestOneInput with each text it makes up. A crash, a finding of a
// sanitizer, or a promise of the public header that doesn't hold for the text ends the run, and
// libFuzzer keeps the text that did it.

#include <pedantjson/pedantjson.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

using pedantjson::Document;
using pedantjson::Kind;
using pedantjson::Number;
using pedantjson::NumberErrorCode;
using pedantjson::Pointer;
using pedantjson::Position;
using pedantjson::SyntaxErrorCode;
using pedantjson::Value;

namespace {

/// Ends the run, naming the promise that doesn't hold.
[[noreturn]] void broken(std::string_view promise,
                         std::source_location where = std::source_location::current()) {
    std::cerr << where.file_name() << ':' << where.line() << ": broken: " << promise << '\n';
    std::abort();
}

/// Ends the run, naming the promise that doesn't hold, if `holds` is false.
void check(bool holds, std::string_view promise,
           std::source_location where = std::source_location::current()) {
    if (!holds) {
        broken(promise, where);
    }
}

/// Whether the text has `word` at `offset`.
bool has_at(std::string_view text, std::size_t offset, std::string_view word) {
    return text.substr(offset).starts_with(word);
}

/// A string written as a JSON string with the fewest escapes: a quotation mark, a backslash and
/// the characters below U+0020, which JSON won't take unescaped.
std::string quoted(std::string_view string) {
    constexpr std::string_view hex = "0123456789abcdef";
    std::string text = "\"";
    for (const char character : string) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '"' || character == '\\') {
            text += '\\';
            text += character;
        } else if (byte < 0x20) {
            text += "\\u00";
            text += hex.at(byte / 16);
            text += hex.at(byte % 16);
        } else {
            text += character;
        }
    }
    return text + '"';
}

/// Checks that a decoded string reads back as itself, written as a JSON string.
void check_string(std::string_view string) {
    const auto reread = pedantjson::parse(quoted(string));
    if (!reread) {
        broken("a decoded string is UTF-8, which JSON takes back");
    }
    check(reread->root().as_string() == string, "a decoded string reads back as itself");
}

/// Checks that an integer type reads a number as the widest type of its signedness does, or fails
/// for a reason that agrees with it.
template <typename T> void check_integer(const Number& number) {
    const auto read = number.to<T>();
    if (!number.is_integer()) {
        check(!read && read.error().code == NumberErrorCode::not_an_integer,
              "a number with a fraction is not_an_integer for every integer type");
        return;
    }
    using Widest = std::conditional_t<std::is_signed_v<T>, long long, unsigned long long>;
    const auto widest = number.to<Widest>();
    if (read) {
        check(widest && *widest == *read,
              "an integer that a type holds reads alike in a wider one");
    } else {
        check(read.error().code == NumberErrorCode::out_of_range,
              "an integer that a type can't hold is out_of_range");
        check(!widest || !std::in_range<T>(*widest),
              "an integer is out_of_range only beyond the type's range");
    }
}

/// Checks that a floating-point type reads a number as a finite value, or finds it out of range.
template <typename T> void check_floating_point(const Number& number) {
    const auto read = number.to<T>();
    check(read || read.error().code == NumberErrorCode::out_of_range,
          "a floating-point type reads a number or finds it out_of_range");
    check(!read || std::isfinite(*read), "a floating-point type reads a finite value");
}

/// Checks a number against its text, and its readings into each type against one another.
void check_number(const Number& number, std::string_view text, std::size_t offset) {
    check(has_at(text, offset, number.text()), "a number keeps its text as the text writes it");

    check_integer<signed char>(number);
    check_integer<short>(number);
    check_integer<int>(number);
    check_integer<long>(number);
    check_integer<long long>(number);
    check_integer<unsigned char>(number);
    check_integer<unsigned short>(number);
    check_integer<unsigned int>(number);
    check_integer<unsigned long>(number);
    check_integer<unsigned long long>(number);
    check_floating_point<float>(number);
    check_floating_point<double>(number);

    const auto signed_value = number.to<long long>();
    const auto unsigned_value = number.to<unsigned long long>();
    if (signed_value && *signed_value >= 0) {
        check(unsigned_value && std::cmp_equal(*unsigned_value, *signed_value),
              "an integer at least zero reads alike as signed and unsigned");
    }
    if (unsigned_value && std::in_range<long long>(*unsigned_value)) {
        check(signed_value && std::cmp_equal(*signed_value, *unsigned_value),
              "an unsigned integer that long long holds reads alike as signed");
    }

    const auto as_double = number.to<double>();
    check(as_double || !number.to<float>().has_value(),
          "a number out of a double's range is out of a float's too");
    // A double holds every integer up to 2^53 exactly.
    constexpr long long largest_exact_integer = 9'007'199'254'740'992;
    if (signed_value && *signed_value >= -largest_exact_integer &&
        *signed_value <= largest_exact_integer) {
        check(as_double && *as_double == static_cast<double>(*signed_value),
              "an integer that a double holds exactly reads as exactly that double");
    }
}

/// Walks a document's values in the order of the text, checking each against the text and the
/// document's lookups.
class Walk {
  public:
    /// Prepares to walk a document read from `text`.
    Walk(const Document& document, std::string_view text) : document_(&document), text_(text) {}

    /// Checks a value and everything inside it.
    ///
    /// \param value The value.
    /// \param pointer The pointer to the value.
    /// \param first Whether each name on the way to the value is the first member of its name, so
    ///              that the pointer leads to the value rather than to an earlier namesake.
    // NOLINTNEXTLINE(misc-no-recursion): max_depth bounds the depth, which is what it is for
    void visit(const Value& value, const Pointer& pointer, bool first) {
        check(value.offset() < text_.size(), "a value begins within the text");
        check_order(value.offset());
        check_lookups(value, pointer, first);
        check_kind(value);

        if (const auto array = value.as_array()) {
            std::size_t index = 0;
            for (const Value element : *array) {
                visit(element, pointer / index, first);
                ++index;
            }
            check(index == array->size(), "an array walks through as many elements as its size");
            check(!first || !document_->find(pointer / array->size()).has_value(),
                  "an array has no element at its size");
        } else if (const auto object = value.as_object()) {
            std::size_t count = 0;
            for (const pedantjson::Member member : *object) {
                const pedantjson::Key key = member.key;
                check_order(key.offset());
                check(has_at(text_, key.offset(), "\""), "a key begins at its quotation mark");
                check_string(key.name());
                const auto namesake = object->find(key.name());
                if (!namesake) {
                    broken("an object finds the name of each of its members");
                }
                visit(member.value, pointer / key.name(),
                      first && namesake->key.offset() == key.offset());
                ++count;
            }
            check(count == object->size(), "an object walks through as many members as its size");
        }
    }

  private:
    /// Checks that values and keys come in the order of the text, each at a later offset and
    /// position than the one before.
    void check_order(std::size_t offset) {
        const Position position = document_->position(offset);
        check(position.line >= 1 && position.column >= 1, "a position counts from 1:1");
        if (seen_any_) {
            check(offset > last_offset_, "values come in the order of the text");
            check(std::pair{position.line, position.column} >
                      std::pair{last_position_.line, last_position_.column},
                  "a later value has a later position");
        }
        seen_any_ = true;
        last_offset_ = offset;
        last_position_ = position;
    }

    /// Checks that a pointer to a value finds it, and leads no further.
    void check_lookups(const Value& value, const Pointer& pointer, bool first) const {
        check(Pointer::parse(pointer.text()) == pointer, "a pointer reads back from its text");
        const auto found = document_->find(pointer);
        if (!found) {
            broken("the pointer to a value finds a value");
        }
        check(document_->nearest(pointer).offset() == found->offset(),
              "the nearest value to a pointer that finds a value is that value");
        // An earlier namesake on the way leads the pointer to another value.
        if (!first) {
            return;
        }
        check(found->offset() == value.offset(), "the pointer to a value finds it");
        if (value.kind() != Kind::array && value.kind() != Kind::object) {
            check(!document_->find(pointer / 0).has_value(), "a scalar has no elements");
            check(document_->nearest(pointer / "a" / 0).offset() == value.offset(),
                  "the nearest value to a pointer into a scalar is the scalar");
        }
    }

    /// Checks that a value is of its kind alone, and that the text writes a value of that kind.
    void check_kind(const Value& value) const {
        const Kind kind = value.kind();
        check(value.is_null() == (kind == Kind::null), "a value is null only of kind null");
        check(value.as_bool().has_value() == (kind == Kind::boolean),
              "a value is a boolean only of kind boolean");
        check(value.as_number().has_value() == (kind == Kind::number),
              "a value is a number only of kind number");
        check(value.as_string().has_value() == (kind == Kind::string),
              "a value is a string only of kind string");
        check(value.as_array().has_value() == (kind == Kind::array),
              "a value is an array only of kind array");
        check(value.as_object().has_value() == (kind == Kind::object),
              "a value is an object only of kind object");

        const std::size_t offset = value.offset();
        if (value.is_null()) {
            check(has_at(text_, offset, "null"), "null is written null");
        }
        if (const auto boolean = value.as_bool()) {
            check(has_at(text_, offset, *boolean ? "true" : "false"),
                  "a boolean is written as its value");
        }
        if (const auto number = value.as_number()) {
            check_number(*number, text_, offset);
        }
        if (const auto string = value.as_string()) {
            check(has_at(text_, offset, "\""), "a string begins at its quotation mark");
            check_string(*string);
        }
        if (value.as_array()) {
            check(has_at(text_, offset, "["), "an array begins at its bracket");
        }
        if (value.as_object()) {
            check(has_at(text_, offset, "{"), "an object begins at its brace");
        }
    }

    /// The document walked, never null.
    const Document* document_;

    /// The text it was read from.
    std::string_view text_;

    /// Whether a value or a key has been seen yet.
    bool seen_any_ = false;

    /// The offset of the last value or key seen.
    std::size_t last_offset_ = 0;

    /// The position of the last value or key seen.
    Position last_position_{.line = 0, .column = 0};
};

/// Checks what `parse` makes of a text.
void check_text(const std::string& text) {
    auto document = pedantjson::parse(text);

    // A lower limit on the depth can only find a mistake sooner, and only too_deep.
    const auto shallow = pedantjson::parse(text, {.max_depth = 4});
    if (!document) {
        const auto& error = document.error();
        check(error.offset <= text.size(), "a syntax error is within the text or at its end");
        check(error.position.line >= 1 && error.position.column >= 1,
              "a syntax error's position counts from 1:1");
        check(!error.message().empty(), "a syntax error has a message");
        if (shallow) {
            broken("a lower limit on the depth rejects every text it would");
        }
        check((shallow.error().code == error.code && shallow.error().offset == error.offset) ||
                  (shallow.error().code == SyntaxErrorCode::too_deep &&
                   shallow.error().offset < error.offset),
              "a lower limit on the depth finds the same mistake, or too_deep before it");
        return;
    }
    check(shallow || shallow.error().code == SyntaxErrorCode::too_deep,
          "a lower limit on the depth rejects a text only as too_deep");

    // Handles stay valid through a move of their document.
    const Value root = document->root();
    const Document moved = std::move(*document);
    Walk{moved, text}.visit(root, Pointer{}, /*first=*/true);
}

/// Checks what `Pointer::parse` makes of a text.
void check_pointer_text(std::string_view text) {
    const auto pointer = Pointer::parse(text);
    if (pointer) {
        check(pointer->text() == text, "a pointer gives back the text it was read from");
    } else {
        check(pointer.error().offset < text.size(), "a pointer's mistake is within its text");
        check(!pointer.error().message().empty(), "a pointer's mistake has a message");
    }
}

} // namespace

// NOLINTNEXTLINE(readability-identifier-naming): the name libFuzzer calls
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::span bytes{data, size};
    const std::string text(bytes.begin(), bytes.end());
    check_text(text);
    check_pointer_text(text);
    return 0;
}
