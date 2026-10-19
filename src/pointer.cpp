#include <pedantjson/pedantjson.hpp>

#include <charconv>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace pedantjson {

namespace {

/// The name a reference token of a pointer stands for, with its escapes decoded.
///
/// \param token The token, from a pointer, so every `~` in it begins `~0` or `~1`.
/// \return The name.
std::string name_of(std::string_view token) {
    std::string name;
    while (true) {
        const std::size_t tilde = token.find('~');
        name.append(token.substr(0, tilde));
        if (tilde == std::string_view::npos) {
            return name;
        }
        name.push_back(token.substr(tilde + 1).starts_with('0') ? '~' : '/');
        token.remove_prefix(tilde + 2);
    }
}

/// The index a reference token of a pointer stands for, if it stands for one: RFC 6901 writes an
/// index in digits with no zero in front, unless it is 0.
///
/// \param token The token.
/// \return The index, or nothing if the token isn't one, or names one too large to be held.
std::optional<std::size_t> index_of(std::string_view token) {
    if (token.empty() || (token.size() > 1 && token.starts_with('0'))) {
        return std::nullopt;
    }
    std::size_t index = 0;
    const char* const end = std::to_address(token.end());
    const auto read = std::from_chars(std::to_address(token.begin()), end, index);
    if (read.ec != std::errc{} || read.ptr != end) {
        return std::nullopt;
    }
    return index;
}

/// The member or the element of a value that a reference token of a pointer names.
///
/// \param parent The value.
/// \param token The token.
/// \return The member's value or the element, or nothing if `parent` has none of that name or
///         index.
std::optional<Value> child_of(Value parent, std::string_view token) {
    if (const auto object = parent.as_object()) {
        if (const auto member = object->find(name_of(token))) {
            return member->value;
        }
        return std::nullopt;
    }
    const auto array = parent.as_array();
    if (!array) {
        return std::nullopt;
    }
    const auto index = index_of(token);
    if (!index || *index >= array->size()) {
        return std::nullopt;
    }
    // The index is that of an element, as just checked.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
    return (*array)[*index];
}

/// How far a pointer leads into a document.
struct Followed {
    /// The deepest value reached.
    Value deepest;

    /// Whether it is the value the pointer names.
    bool found = false;
};

/// Follows a pointer from the root for as long as the document has what it names.
///
/// \param root The document's root.
/// \param pointer The pointer.
/// \return How far it leads.
Followed follow(Value root, const Pointer& pointer) {
    Value value = root;
    // The text is empty, or a slash before each reference token.
    std::string_view rest = pointer.text();
    while (!rest.empty()) {
        rest.remove_prefix(1);
        const std::string_view token = rest.substr(0, rest.find('/'));
        rest.remove_prefix(token.size());
        const auto child = child_of(value, token);
        if (!child) {
            return {.deepest = value, .found = false};
        }
        value = *child;
    }
    return {.deepest = value, .found = true};
}

} // namespace

std::string_view PointerError::message() const noexcept {
    switch (code) {
    case PointerErrorCode::missing_slash:
        return "pointer doesn't begin with a slash";
    case PointerErrorCode::invalid_escape:
        return "invalid escape in a pointer";
    }
    // Only a code cast from outside the enumeration gets here.
    return "not a pointer";
}

std::expected<Pointer, PointerError> Pointer::parse(std::string_view text) {
    if (!text.empty() && !text.starts_with('/')) {
        return std::unexpected(PointerError{.code = PointerErrorCode::missing_slash, .offset = 0});
    }
    for (std::size_t tilde = text.find('~'); tilde != std::string_view::npos;
         tilde = text.find('~', tilde + 1)) {
        const std::string_view escape = text.substr(tilde, 2);
        if (escape != "~0" && escape != "~1") {
            return std::unexpected(
                PointerError{.code = PointerErrorCode::invalid_escape, .offset = tilde});
        }
    }
    Pointer pointer;
    pointer.text_ = text;
    return pointer;
}

void Pointer::append_name(std::string_view name) {
    text_.push_back('/');
    for (const char c : name) {
        if (c == '~') {
            text_.append("~0");
        } else if (c == '/') {
            text_.append("~1");
        } else {
            text_.push_back(c);
        }
    }
}

void Pointer::append_index(std::size_t index) {
    text_.push_back('/');
    text_.append(std::to_string(index));
}

std::optional<Value> Document::find(const Pointer& pointer) const {
    const Followed followed = follow(root(), pointer);
    if (!followed.found) {
        return std::nullopt;
    }
    return followed.deepest;
}

Value Document::nearest(const Pointer& pointer) const { return follow(root(), pointer).deepest; }

} // namespace pedantjson
