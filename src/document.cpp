#include "storage.hpp"
#include <pedantjson/pedantjson.hpp>

#include <cassert>
#include <cstddef>
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

} // namespace pedantjson
