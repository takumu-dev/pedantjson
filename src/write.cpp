#include "utf8.hpp"
#include <pedantjson/pedantjson.hpp>

#include <cassert>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pedantjson {

namespace {

/// The lower-case hexadecimal digit of a value below 16.
[[nodiscard]] char hex_digit(unsigned value) noexcept {
    return static_cast<char>(value < 10 ? '0' + value : 'a' + (value - 10));
}

/// Writes a string or a name as a JSON string, escaping only what JSON won't take as it is: a
/// quotation mark, a backslash, and the characters below U+0020, in their short forms where JSON
/// has one. Every other character is written as itself.
///
/// \param text The text to write onto.
/// \param string The string's characters.
/// \return Whether they are UTF-8. If they aren't, what has been written is incomplete.
[[nodiscard]] bool write_string(std::string& text, std::string_view string) {
    text += '"';
    std::size_t at = 0;
    while (at < string.size()) {
        const std::size_t length = detail::utf8_sequence_length(string, at);
        if (length == 0) {
            return false;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): at < size
        const char character = string[at];
        const auto byte = static_cast<unsigned char>(character);
        if (length > 1 || (byte >= 0x20 && character != '"' && character != '\\')) {
            text += string.substr(at, length);
        } else {
            text += '\\';
            switch (character) {
            case '"':
            case '\\':
                text += character;
                break;
            case '\b':
                text += 'b';
                break;
            case '\f':
                text += 'f';
                break;
            case '\n':
                text += 'n';
                break;
            case '\r':
                text += 'r';
                break;
            case '\t':
                text += 't';
                break;
            default:
                // A byte below 0x20 is two hexadecimal digits, the first 0 or 1.
                text += "u00";
                text += static_cast<char>('0' + (byte / 16));
                text += hex_digit(byte % 16U);
            }
        }
        at += length;
    }
    text += '"';
    return true;
}

/// The writer's view of a document's values.
struct ValueTree {
    /// A handle to a value to write.
    using Handle = Value;

    /// The value's kind.
    static Kind kind(const Value& value) noexcept { return value.kind(); }

    // The writer asks only an array or an object for its children, each by an index below its
    // size.

    /// How many elements or members an array or an object has.
    static std::size_t size(const Value& value) noexcept {
        if (const auto array = value.as_array()) {
            return array->size();
        }
        return value.as_object().value_or(Object{}).size();
    }

    /// The element of an array, or the value of an object's member, with index `index`.
    static Value child(const Value& value, std::size_t index) {
        const auto array = value.as_array();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return array ? (*array)[index] : value.as_object().value_or(Object{})[index].value;
    }

    /// The name of an object's member with index `index`.
    static std::string_view name(const Value& value, std::size_t index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return value.as_object().value_or(Object{})[index].key.name();
    }

    /// Writes a value that is neither an array nor an object.
    ///
    /// \return Nothing, since a document's strings are UTF-8 and its numbers are text.
    static std::optional<WriteErrorCode> write_scalar(std::string& text, const Value& value) {
        if (value.is_null()) {
            text += "null";
        } else if (const auto boolean = value.as_bool()) {
            text += *boolean ? "true" : "false";
        } else if (const auto number = value.as_number()) {
            text += number->text();
        } else if (const auto string = value.as_string()) {
            if (!write_string(text, *string)) {
                return WriteErrorCode::invalid_utf8;
            }
        }
        return std::nullopt;
    }
};

/// Writes a tree of values as text, keeping a stack of its own rather than recursing, so that no
/// depth of nesting can overflow the call stack.
///
/// \tparam Tree How to walk the tree.
template <typename Tree> class Writer {
  public:
    /// A handle to a value to write.
    using Handle = Tree::Handle;

    /// Prepares to write with the given options.
    explicit Writer(WriteOptions options) : options_(options) {
        // NOLINTNEXTLINE(misc-static-assert): the condition depends on the argument
        assert(options_.indent != 0U && "an indent is at least one space");
    }

    /// Writes a value and everything inside it.
    ///
    /// \param root The value.
    /// \return The text, or the first value in it that can't be written.
    std::expected<std::string, WriteError> run(const Handle& root) {
        Handle value = root;
        while (true) {
            if (const auto refused = begin_value(value)) {
                return refuse(*refused);
            }
            const auto more = next_child(value);
            if (!more) {
                return refuse(more.error());
            }
            if (!*more) {
                return std::move(text_);
            }
        }
    }

  private:
    /// An array or an object whose children are being written.
    struct Frame {
        /// The array or the object.
        Handle value;

        /// Whether it is an object.
        bool is_object;

        /// How many children it has.
        std::size_t size;

        /// How many of its children have begun.
        std::size_t begun;
    };

    /// Writes a scalar, or opens an array or an object.
    ///
    /// \return Why the value can't be written, or nothing if it has been.
    std::optional<WriteErrorCode> begin_value(const Handle& value) {
        const Kind kind = Tree::kind(value);
        if (kind != Kind::array && kind != Kind::object) {
            return Tree::write_scalar(text_, value);
        }
        const bool is_object = kind == Kind::object;
        text_ += is_object ? '{' : '[';
        frames_.push_back(
            {.value = value, .is_object = is_object, .size = Tree::size(value), .begun = 0});
        return std::nullopt;
    }

    /// Closes every array and object whose children have all been written, then begins the next
    /// child, writing its key if it is a member.
    ///
    /// \param value Set to the next child.
    /// \return Whether there is one, or why its key can't be written.
    std::expected<bool, WriteErrorCode> next_child(Handle& value) {
        while (!frames_.empty()) {
            Frame& frame = frames_.back();
            if (frame.begun == frame.size) {
                const bool is_object = frame.is_object;
                const bool empty = frame.size == 0;
                frames_.pop_back();
                if (!empty) {
                    new_line();
                }
                text_ += is_object ? '}' : ']';
                continue;
            }
            if (frame.begun > 0) {
                text_ += ',';
            }
            const std::size_t index = frame.begun++;
            new_line();
            if (frame.is_object) {
                if (!write_string(text_, Tree::name(frame.value, index))) {
                    return std::unexpected(WriteErrorCode::invalid_utf8);
                }
                text_ += options_.indent ? ": " : ":";
            }
            value = Tree::child(frame.value, index);
            return true;
        }
        return false;
    }

    /// The error of the given kind at the value or the key being written, which is the last child
    /// to have begun in each array and object open, or the root if none is.
    [[nodiscard]] std::unexpected<WriteError> refuse(WriteErrorCode code) const {
        Pointer pointer;
        for (const Frame& frame : frames_) {
            const std::size_t index = frame.begun - 1;
            if (frame.is_object) {
                pointer = std::move(pointer) / Tree::name(frame.value, index);
            } else {
                pointer = std::move(pointer) / index;
            }
        }
        return std::unexpected(WriteError{.code = code, .pointer = std::move(pointer)});
    }

    /// Begins a new line, indented to the depth of the arrays and objects open, if the text is
    /// indented at all.
    void new_line() {
        if (options_.indent) {
            text_ += '\n';
            text_.append(frames_.size() * *options_.indent, ' ');
        }
    }

    /// How to lay out the text.
    WriteOptions options_;

    /// The text written so far.
    std::string text_;

    /// The arrays and objects open, the innermost last.
    std::vector<Frame> frames_;
};

} // namespace

/// The writer's view of a draft.
struct detail::DraftTree {
    /// A handle to a draft to write, never null.
    using Handle = const Draft*;

    // Every index comes from the writer, which keeps it below the size of the draft's children.

    /// The draft's kind.
    static Kind kind(const Draft* draft) noexcept { return draft->kind_; }

    /// How many elements or members an array or an object has.
    static std::size_t size(const Draft* draft) noexcept { return draft->children_.size(); }

    /// The element of an array, or the value of an object's member, with index `index`.
    static const Draft* child(const Draft* draft, std::size_t index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return &draft->children_[index];
    }

    /// The name of an object's member with index `index`.
    static std::string_view name(const Draft* draft, std::size_t index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return draft->names_[index];
    }

    /// Writes a draft that is neither an array nor an object.
    ///
    /// \return Why it can't be written, or nothing if it has been.
    static std::optional<WriteErrorCode> write_scalar(std::string& text, const Draft* draft) {
        switch (draft->kind_) {
        case Kind::null:
            text += "null";
            break;
        case Kind::boolean:
            text += draft->is_true_ ? "true" : "false";
            break;
        case Kind::number:
            if (draft->characters_.empty()) {
                return WriteErrorCode::not_finite;
            }
            text += draft->characters_;
            break;
        case Kind::string:
            if (!write_string(text, draft->characters_)) {
                return WriteErrorCode::invalid_utf8;
            }
            break;
        case Kind::array:
        case Kind::object:
            // The writer opens arrays and objects itself.
            break;
        }
        return std::nullopt;
    }
};

std::string_view WriteError::message() const noexcept {
    switch (code) {
    case WriteErrorCode::not_finite:
        return "number not finite";
    case WriteErrorCode::invalid_utf8:
        return "invalid UTF-8";
    }
    // Only a code cast from outside the enumeration gets here.
    return "value not written";
}

std::string write(const Value& value, WriteOptions options) {
    auto text = Writer<ValueTree>{options}.run(value);
    // NOLINTNEXTLINE(misc-static-assert): the condition depends on the value
    assert(text.has_value() && "nothing in a document is refused");
    return std::move(text).value_or(std::string{});
}

std::expected<std::string, WriteError> write(const Draft& draft, WriteOptions options) {
    return Writer<detail::DraftTree>{options}.run(&draft);
}

} // namespace pedantjson
