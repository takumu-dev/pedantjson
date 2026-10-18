/// \file
/// pedantjson, a strict JSON reader.
///
/// `parse` reads a JSON text into a `Document`, an immutable tree that owns the text it was read
/// from. Every key and every value carries the byte offset where it begins in that text, an
/// object's members stay in the order the text gives them, duplicates included, and a number keeps
/// the text it was written as. A text that isn't JSON gives a `SyntaxError` instead, which says
/// what is wrong and where.

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pedantjson {

class Array;
class Document;
class Object;
struct Member;

namespace detail {

/// A document's text and its nodes, shared by every handle into the document.
struct Storage;

/// Walks the elements of an array or the members of an object, in the order of the text.
///
/// It keeps a copy of its container, which is itself a handle, so it stays valid as long as the
/// document does.
///
/// \tparam Container `Array` or `Object`.
/// \tparam Element What the container holds: `Value` or `Member`.
template <typename Container, typename Element> class Iterator {
  public:
    // The standard's iterator concepts look for these three names.

    /// What the iterator gives.
    using value_type = Element; // NOLINT(readability-identifier-naming): see above

    /// The type of the distance between two iterators.
    using difference_type = std::ptrdiff_t; // NOLINT(readability-identifier-naming): see above

    /// What the iterator can do: walk forwards, any number of times.
    using iterator_concept = // NOLINT(readability-identifier-naming): see above
        std::forward_iterator_tag;

    /// Creates an iterator that belongs to no container.
    Iterator() = default;

    /// Creates an iterator at the element of `container` with index `index`.
    ///
    /// \param container The container to walk.
    /// \param index The index of the element, or the container's size for its end.
    Iterator(Container container, std::size_t index) noexcept
        : container_(container), index_(index) {}

    /// The element the iterator is at, which must not be its container's end.
    ///
    /// \return The element.
    [[nodiscard]] Element operator*() const {
        // The container's operator[] asserts that the index is that of an element.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return container_[index_];
    }

    /// Moves to the next element.
    ///
    /// \return This iterator.
    Iterator& operator++() noexcept {
        ++index_;
        return *this;
    }

    /// Moves to the next element.
    ///
    /// \return A copy of the iterator from before the move.
    Iterator operator++(int) noexcept {
        Iterator before = *this;
        ++index_;
        return before;
    }

    /// Whether two iterators of the same container are at the same element.
    ///
    /// \param other The other iterator.
    /// \return Whether they are.
    [[nodiscard]] bool operator==(const Iterator& other) const noexcept {
        return index_ == other.index_;
    }

  private:
    /// The container walked.
    Container container_;

    /// The index of the element the iterator is at.
    std::size_t index_ = 0;
};

} // namespace detail

/// What kind of value a JSON value is.
enum class Kind : std::uint8_t {
    null,    ///< `null`.
    boolean, ///< `true` or `false`.
    number,  ///< A number, such as `-1.5e3`.
    string,  ///< A string, such as `"name"`.
    array,   ///< An array, such as `[1, 2]`.
    object,  ///< An object, such as `{"x": 1}`.
};

/// A number, as the text writes it.
class Number {
  public:
    /// The number exactly as the text writes it, such as `-1.50e3`.
    ///
    /// \return A view into the document's text, valid as long as the document is.
    [[nodiscard]] std::string_view text() const noexcept { return text_; }

  private:
    friend class Value;

    /// Creates a number written as `text`.
    ///
    /// \param text A view into the document's text.
    explicit Number(std::string_view text) noexcept : text_(text) {}

    /// The number as the text writes it.
    std::string_view text_;
};

/// A handle to one value of a document.
///
/// It is cheap to copy, and stays valid as long as its document does, through any move of the
/// document.
class Value {
  public:
    /// What kind of value it is.
    ///
    /// \return Its kind.
    [[nodiscard]] Kind kind() const noexcept;

    /// Whether the value is `null`.
    ///
    /// \return Whether it is.
    [[nodiscard]] bool is_null() const noexcept;

    /// Where the value begins in the text: its first byte, such as its opening bracket or quote.
    ///
    /// \return Its offset in bytes from the start of the text.
    [[nodiscard]] std::size_t offset() const noexcept;

    /// The value as a boolean.
    ///
    /// \return Its value, or nothing if it isn't `true` or `false`.
    [[nodiscard]] std::optional<bool> as_bool() const noexcept;

    /// The value as a number.
    ///
    /// \return The number, or nothing if it isn't one.
    [[nodiscard]] std::optional<Number> as_number() const noexcept;

    /// The value as a string, with its escapes decoded.
    ///
    /// \return A view of the string, valid as long as the document is, or nothing if the value
    ///         isn't a string.
    [[nodiscard]] std::optional<std::string_view> as_string() const noexcept;

    /// The value as an array.
    ///
    /// \return The array, or nothing if the value isn't one.
    [[nodiscard]] std::optional<Array> as_array() const noexcept;

    /// The value as an object.
    ///
    /// \return The object, or nothing if the value isn't one.
    [[nodiscard]] std::optional<Object> as_object() const noexcept;

  private:
    friend class Array;
    friend class Document;
    friend class Object;

    /// Creates a handle to a node of a document.
    ///
    /// \param storage The document's storage.
    /// \param node The index of the value's node.
    Value(const detail::Storage* storage, std::size_t node) noexcept
        : storage_(storage), node_(node) {}

    /// The document's storage, never null. A pointer rather than a reference, so that a value can
    /// be assigned.
    const detail::Storage* storage_ = nullptr;

    /// The index of the value's node.
    std::size_t node_ = 0;
};

/// The elements of an array, in the order of the text.
///
/// It is a handle like `Value`, valid as long as its document is. A default array is empty and
/// belongs to no document.
class Array {
  public:
    /// Walks the elements in order.
    using Iterator = detail::Iterator<Array, Value>;

    /// Creates an empty array that belongs to no document.
    Array() = default;

    /// How many elements the array has.
    ///
    /// \return The number of elements.
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /// Whether the array has no elements.
    ///
    /// \return Whether it is empty.
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /// The element with index `index`, which must be less than `size()`.
    ///
    /// \param index The element's index, counting from 0.
    /// \return The element.
    [[nodiscard]] Value operator[](std::size_t index) const;

    /// The first element.
    ///
    /// \return An iterator at the first element, or at the end if there is none.
    [[nodiscard]] Iterator begin() const noexcept { return {*this, 0}; }

    /// The end of the elements.
    ///
    /// \return An iterator past the last element.
    [[nodiscard]] Iterator end() const noexcept { return {*this, size_}; }

  private:
    friend class Value;

    /// Creates a handle to an array's elements.
    ///
    /// \param storage The document's storage.
    /// \param first Where the elements begin among the document's children.
    /// \param size How many elements there are.
    Array(const detail::Storage* storage, std::size_t first, std::size_t size) noexcept
        : storage_(storage), first_(first), size_(size) {}

    /// The document's storage, or null for a default array.
    const detail::Storage* storage_ = nullptr;

    /// Where the elements begin among the document's children.
    std::size_t first_ = 0;

    /// How many elements there are.
    std::size_t size_ = 0;
};

/// The name of an object's member, as a key in the text.
class Key {
  public:
    /// The member's name, with its escapes decoded.
    ///
    /// \return A view of the name, valid as long as the document is.
    [[nodiscard]] std::string_view name() const noexcept { return name_; }

    /// Where the key begins in the text: its opening quote.
    ///
    /// \return Its offset in bytes from the start of the text.
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

  private:
    friend class Object;

    /// Creates a key.
    ///
    /// \param name The decoded name, a view into the document.
    /// \param offset Where the key begins in the text.
    Key(std::string_view name, std::size_t offset) noexcept : name_(name), offset_(offset) {}

    /// The decoded name.
    std::string_view name_;

    /// Where the key begins in the text.
    std::size_t offset_ = 0;
};

/// One member of an object: a key and its value.
struct Member {
    /// The member's key.
    Key key;

    /// The member's value.
    Value value;
};

/// The members of an object, in the order of the text, every duplicate name included.
///
/// It is a handle like `Value`, valid as long as its document is. A default object is empty and
/// belongs to no document.
class Object {
  public:
    /// Walks the members in order.
    using Iterator = detail::Iterator<Object, Member>;

    /// Creates an empty object that belongs to no document.
    Object() = default;

    /// How many members the object has, every duplicate counted.
    ///
    /// \return The number of members.
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /// Whether the object has no members.
    ///
    /// \return Whether it is empty.
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /// The member with index `index` in the order of the text, which must be less than `size()`.
    ///
    /// \param index The member's index, counting from 0.
    /// \return The member.
    [[nodiscard]] Member operator[](std::size_t index) const;

    /// The first member named `name`.
    ///
    /// A name the object holds more than once answers its first member. A caller that must treat
    /// duplicates walks the members instead.
    ///
    /// \param name The name to look for, compared with each key's decoded name.
    /// \return The member, or nothing if no member has that name.
    [[nodiscard]] std::optional<Member> find(std::string_view name) const noexcept;

    /// The first member.
    ///
    /// \return An iterator at the first member, or at the end if there is none.
    [[nodiscard]] Iterator begin() const noexcept { return {*this, 0}; }

    /// The end of the members.
    ///
    /// \return An iterator past the last member.
    [[nodiscard]] Iterator end() const noexcept { return {*this, size_}; }

  private:
    friend class Value;

    /// Creates a handle to an object's members.
    ///
    /// \param storage The document's storage.
    /// \param first Where the members' keys and values begin among the document's children.
    /// \param size How many members there are.
    Object(const detail::Storage* storage, std::size_t first, std::size_t size) noexcept
        : storage_(storage), first_(first), size_(size) {}

    /// The document's storage, or null for a default object.
    const detail::Storage* storage_ = nullptr;

    /// Where the members' keys and values begin among the document's children.
    std::size_t first_ = 0;

    /// How many members there are.
    std::size_t size_ = 0;
};

/// What kind of mistake makes a text something other than JSON.
enum class SyntaxErrorCode : std::uint8_t {
    empty_input,                 ///< The text holds no value, only whitespace or nothing at all.
    unexpected_character,        ///< A character that nothing in JSON allows where it stands.
    unexpected_end,              ///< The text ends where more of a value has to follow.
    invalid_number,              ///< A number against the grammar, such as `01`, `1.e5` or `-x`.
    unterminated_string,         ///< A string whose closing quote never comes.
    invalid_escape,              ///< A backslash in a string that doesn't begin an escape JSON has.
    unpaired_surrogate,          ///< A `\u` escape of a surrogate that isn't half of a pair.
    control_character_in_string, ///< A character below U+0020 written into a string unescaped.
    invalid_utf8,                ///< Bytes that aren't UTF-8, such as 0xFF or an overlong form.
    too_deep,                    ///< Arrays and objects nested deeper than `max_depth` allows.
    trailing_content,            ///< Something other than whitespace after the value.
};

/// Why a text isn't JSON, and where.
///
/// A text can hold more than one mistake, but nothing after the first can be read with any
/// certainty, so the error describes the first.
struct SyntaxError {
    /// What kind of mistake it is.
    SyntaxErrorCode code;

    /// Where the text stops being JSON: the offset in bytes of the first byte that can't be read
    /// as JSON, or the size of the text when it ends too soon.
    std::size_t offset;

    /// What kind of mistake it is, in words, such as `unexpected end of the text`.
    ///
    /// \return A fixed message for the code, in lower case with no full stop.
    [[nodiscard]] std::string_view message() const noexcept;
};

/// How `parse` reads a text.
struct ParseOptions {
    /// How deeply arrays and objects may nest: an array or an object inside `max_depth` others is
    /// too deep. The parser doesn't recurse, so no nesting can overflow its stack; the limit keeps
    /// a program that walks the document by recursion from overflowing its own.
    std::size_t max_depth = 256;
};

/// A JSON text read into an immutable tree.
///
/// The document owns the text and every node read from it. The values, arrays, objects, keys and
/// numbers it hands out are handles into it, valid while it lives, through any move of it. A
/// document can be moved but not copied, and once moved from it can only be assigned to or
/// destroyed.
class Document {
  public:
    /// Moves a document, and every handle into it with it.
    ///
    /// \param other The document to move from.
    Document(Document&& other) noexcept;

    /// Replaces this document with another, ending every handle into this one.
    ///
    /// \param other The document to move from.
    /// \return This document.
    Document& operator=(Document&& other) noexcept;

    /// A document can't be copied.
    Document(const Document&) = delete;

    /// A document can't be copied.
    ///
    /// \return Nothing, since it can't be called.
    Document& operator=(const Document&) = delete;

    /// Destroys the document, and with it every handle into it.
    ~Document();

    /// The value the text holds. The document must not have been moved from.
    ///
    /// \return The outermost value.
    [[nodiscard]] Value root() const;

  private:
    friend std::expected<Document, SyntaxError> parse(std::string text, ParseOptions options);

    /// Creates a document that owns the storage a text was read into.
    ///
    /// \param storage The storage.
    explicit Document(std::unique_ptr<detail::Storage> storage) noexcept;

    /// The text and its nodes, on the heap so that handles survive a move of the document. Null
    /// once the document is moved from.
    std::unique_ptr<detail::Storage> storage_;
};

/// Reads a JSON text, as RFC 8259 defines one.
///
/// \param text The text, which the document takes over.
/// \param options How to read it.
/// \return The document, or the first mistake that makes the text something other than JSON.
[[nodiscard]] std::expected<Document, SyntaxError> parse(std::string text,
                                                         ParseOptions options = {});

} // namespace pedantjson
