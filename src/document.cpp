#include "storage.hpp"
#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace pedantjson {

namespace {

/// The node with index `index`, if it is a value of the given kind.
const detail::Node* node_of_kind(const detail::Storage& storage, std::size_t index, Kind kind) {
    const detail::Node& node = storage.node(index);
    return node.kind == kind ? &node : nullptr;
}

/// The length of a character in a text known to be UTF-8, from its first byte alone.
///
/// \param first The character's first byte.
/// \return Its length in bytes.
std::size_t character_length(char first) noexcept {
    const auto byte = static_cast<unsigned char>(first);
    if (byte >= 0xf0) {
        return 4;
    }
    if (byte >= 0xe0) {
        return 3;
    }
    if (byte >= 0xc0) {
        return 2;
    }
    return 1;
}

} // namespace

Kind Value::kind() const noexcept { return storage_->node(node_).kind; }

bool Value::is_null() const noexcept { return kind() == Kind::null; }

std::size_t Value::offset() const noexcept { return storage_->node(node_).offset; }

std::optional<bool> Value::as_bool() const noexcept {
    if (const detail::Node* node = node_of_kind(*storage_, node_, Kind::boolean)) {
        return node->is_true;
    }
    return std::nullopt;
}

std::optional<Number> Value::as_number() const noexcept {
    if (const detail::Node* node = node_of_kind(*storage_, node_, Kind::number)) {
        return Number{storage_->characters(*node)};
    }
    return std::nullopt;
}

std::optional<std::string_view> Value::as_string() const noexcept {
    if (const detail::Node* node = node_of_kind(*storage_, node_, Kind::string)) {
        return storage_->characters(*node);
    }
    return std::nullopt;
}

std::optional<Array> Value::as_array() const noexcept {
    if (const detail::Node* node = node_of_kind(*storage_, node_, Kind::array)) {
        return Array{storage_, node->first, node->size};
    }
    return std::nullopt;
}

std::optional<Object> Value::as_object() const noexcept {
    if (const detail::Node* node = node_of_kind(*storage_, node_, Kind::object)) {
        return Object{storage_, node->first, node->size};
    }
    return std::nullopt;
}

Value Array::operator[](std::size_t index) const {
    // NOLINTNEXTLINE(misc-static-assert): the condition depends on the argument
    assert(index < size_ && "the index is that of an element");
    return {storage_, storage_->child(first_ + index)};
}

Member Object::operator[](std::size_t index) const {
    // NOLINTNEXTLINE(misc-static-assert): the condition depends on the argument
    assert(index < size_ && "the index is that of a member");
    // An object's children are its keys and values in turn.
    const std::size_t key = storage_->child(first_ + (2 * index));
    const std::size_t value = storage_->child(first_ + (2 * index) + 1);
    const detail::Node& key_node = storage_->node(key);
    return {.key = Key{storage_->characters(key_node), key_node.offset},
            .value = Value{storage_, value}};
}

std::optional<Member> Object::find(std::string_view name) const noexcept {
    for (const Member member : *this) {
        if (member.key.name() == name) {
            return member;
        }
    }
    return std::nullopt;
}

std::string_view SyntaxError::message() const noexcept {
    switch (code) {
    case SyntaxErrorCode::empty_input:
        return "no value in the text";
    case SyntaxErrorCode::unexpected_character:
        return "unexpected character";
    case SyntaxErrorCode::unexpected_end:
        return "unexpected end of the text";
    case SyntaxErrorCode::invalid_number:
        return "invalid number";
    case SyntaxErrorCode::unterminated_string:
        return "string not closed before the end of the text";
    case SyntaxErrorCode::invalid_escape:
        return "invalid escape in a string";
    case SyntaxErrorCode::unpaired_surrogate:
        return "unpaired surrogate in a string";
    case SyntaxErrorCode::control_character_in_string:
        return "unescaped control character in a string";
    case SyntaxErrorCode::invalid_utf8:
        return "invalid UTF-8";
    case SyntaxErrorCode::too_deep:
        return "arrays and objects nested too deeply";
    case SyntaxErrorCode::trailing_content:
        return "unexpected content after the value";
    }
    // Only a code cast from outside the enumeration gets here.
    return "syntax error";
}

Document::Document(std::unique_ptr<detail::Storage> storage) noexcept
    : storage_(std::move(storage)) {}

Document::Document(Document&& other) noexcept = default;

Document& Document::operator=(Document&& other) noexcept = default;

Document::~Document() = default;

Value Document::root() const {
    // NOLINTNEXTLINE(misc-static-assert): the condition depends on the document's state
    assert(storage_ != nullptr && "the document hasn't been moved from");
    return {storage_.get(), 0};
}

Position Document::position(std::size_t offset) const {
    // NOLINTNEXTLINE(misc-static-assert): the condition depends on the document's state
    assert(storage_ != nullptr && "the document hasn't been moved from");
    return storage_->position(offset);
}

Position detail::Storage::position(std::size_t offset) const noexcept {
    offset = std::min(offset, text.size());
    // The last line that begins at or before the offset. Only an offset within a byte-order mark
    // comes before the first line, and the mark takes no column.
    const auto after = std::ranges::upper_bound(line_starts, offset);
    if (after == line_starts.begin()) {
        return {.line = 1, .column = 1};
    }
    const auto line = static_cast<std::size_t>(after - line_starts.begin());
    // The column is one more than the number of characters that end before the offset. Each
    // character's first byte gives its length, and the text before the offset is UTF-8, since the
    // parser stops at the first byte that isn't. Every byte read is before the offset, which is
    // within the text.
    std::size_t column = 1;
    std::size_t at = *std::prev(after);
    while (at < offset) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        at += character_length(text[at]);
        // A character that the offset falls within doesn't end before it.
        if (at <= offset) {
            ++column;
        }
    }
    return {.line = line, .column = column};
}

} // namespace pedantjson
