#include "storage.hpp"
#include <pedantjson/pedantjson.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pedantjson {

namespace {

/// Reads one text into the storage of a document.
class Parser {
  public:
    explicit Parser(std::string text) : storage_(std::make_unique<detail::Storage>()) {
        storage_->text = std::move(text);
        text_ = storage_->text;
    }

    /// Reads the whole text, which must be one value with nothing but whitespace around it.
    ///
    /// Arrays and objects are read with a stack of their own rather than by recursion, so that no
    /// depth of nesting can overflow the call stack.
    ///
    /// \return The storage the text was read into.
    std::expected<std::unique_ptr<detail::Storage>, SyntaxError> run() {
        skip_whitespace();
        if (at_end()) {
            return fail(SyntaxErrorCode::empty_input);
        }
        while (true) {
            const auto completed = begin_value();
            if (!completed) {
                return std::unexpected(completed.error());
            }
            if (!*completed) {
                continue;
            }
            const auto more = end_value(**completed);
            if (!more) {
                return std::unexpected(more.error());
            }
            if (!*more) {
                return std::move(storage_);
            }
        }
    }

  private:
    [[nodiscard]] bool at_end() const noexcept { return offset_ == text_.size(); }

    /// The character at the current offset, which must not be the end.
    [[nodiscard]] char peek() const {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return text_[offset_];
    }

    /// The error of the given kind at the current offset.
    [[nodiscard]] std::unexpected<SyntaxError> fail(SyntaxErrorCode code) const noexcept {
        return std::unexpected(SyntaxError{.code = code, .offset = offset_});
    }

    void skip_whitespace() noexcept {
        while (!at_end()) {
            const char c = peek();
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                return;
            }
            ++offset_;
        }
    }

    /// Adds a node that begins at the current offset.
    detail::Node& add_node(Kind kind) {
        return storage_->nodes.emplace_back(detail::Node{.kind = kind, .offset = offset_});
    }

    [[nodiscard]] static char closing_bracket(Kind kind) noexcept {
        return kind == Kind::array ? ']' : '}';
    }

    /// Reads the value that begins here, after any whitespace, or opens it if it is an array or
    /// an object with something in it.
    ///
    /// \return The value's node, or nothing if it opened an array or an object whose first element
    ///         or member follows.
    std::expected<std::optional<std::size_t>, SyntaxError> begin_value() {
        skip_whitespace();
        if (at_end()) {
            return fail(SyntaxErrorCode::unexpected_end);
        }
        if (peek() != '[' && peek() != '{') {
            if (auto read = read_scalar(); !read) {
                return std::unexpected(read.error());
            }
            return storage_->nodes.size() - 1;
        }
        const Kind kind = peek() == '[' ? Kind::array : Kind::object;
        open(kind);
        skip_whitespace();
        if (!at_end() && peek() == closing_bracket(kind)) {
            ++offset_;
            return close();
        }
        if (kind == Kind::object) {
            if (auto name = read_name(); !name) {
                return std::unexpected(name.error());
            }
        }
        return std::nullopt;
    }

    /// Adds the value that has just ended to the array or the object around it, and closes each
    /// that ends with it, until one goes on to another element or member.
    ///
    /// \param completed The node of the value that has ended.
    /// \return Whether another value follows, which is false once the outermost value has ended
    ///         and the text with it.
    std::expected<bool, SyntaxError> end_value(std::size_t completed) {
        while (!frames_.empty()) {
            pending_.push_back(completed);
            const Kind kind = frames_.back().kind;
            skip_whitespace();
            if (at_end()) {
                return fail(SyntaxErrorCode::unexpected_end);
            }
            if (peek() == ',') {
                ++offset_;
                if (kind == Kind::object) {
                    if (auto name = read_name(); !name) {
                        return std::unexpected(name.error());
                    }
                }
                return true;
            }
            if (peek() != closing_bracket(kind)) {
                return fail(SyntaxErrorCode::unexpected_character);
            }
            ++offset_;
            completed = close();
        }
        skip_whitespace();
        if (!at_end()) {
            return fail(SyntaxErrorCode::trailing_content);
        }
        return false;
    }

    /// Reads a member's key and the colon after it, adding the key to the pending children.
    std::expected<void, SyntaxError> read_name() {
        skip_whitespace();
        if (at_end()) {
            return fail(SyntaxErrorCode::unexpected_end);
        }
        if (peek() != '"') {
            return fail(SyntaxErrorCode::unexpected_character);
        }
        if (auto key = read_string(); !key) {
            return key;
        }
        pending_.push_back(storage_->nodes.size() - 1);
        skip_whitespace();
        if (at_end()) {
            return fail(SyntaxErrorCode::unexpected_end);
        }
        if (peek() != ':') {
            return fail(SyntaxErrorCode::unexpected_character);
        }
        ++offset_;
        return {};
    }

    /// Opens an array or an object at its bracket.
    void open(Kind kind) {
        frames_.push_back(
            Frame{.node = storage_->nodes.size(), .kind = kind, .first_pending = pending_.size()});
        add_node(kind);
        ++offset_;
    }

    /// Closes the innermost array or object, after its closing bracket, moving its children from
    /// the pending ones to its own.
    ///
    /// \return Its node.
    std::size_t close() {
        const Frame frame = frames_.back();
        frames_.pop_back();
        detail::Node& node = storage_->node(frame.node);
        node.first = storage_->children.size();
        // An object's children are its keys and values in turn.
        const std::size_t children = pending_.size() - frame.first_pending;
        node.size = frame.kind == Kind::object ? children / 2 : children;
        const auto first = pending_.begin() + static_cast<std::ptrdiff_t>(frame.first_pending);
        storage_->children.insert(storage_->children.end(), first, pending_.end());
        pending_.erase(first, pending_.end());
        return frame.node;
    }

    /// Reads a value that isn't an array or an object.
    std::expected<void, SyntaxError> read_scalar() {
        switch (peek()) {
        case 't':
            add_node(Kind::boolean).is_true = true;
            return read_word("true");
        case 'f':
            add_node(Kind::boolean).is_true = false;
            return read_word("false");
        case 'n':
            add_node(Kind::null);
            return read_word("null");
        case '"':
            return read_string();
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return read_number();
        default:
            return fail(SyntaxErrorCode::unexpected_character);
        }
    }

    [[nodiscard]] static bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

    /// Reads a number, keeping its text.
    std::expected<void, SyntaxError> read_number() {
        const std::size_t first = offset_;
        if (peek() == '-') {
            ++offset_;
        }
        // The integer part is a lone zero, or digits that don't begin with one.
        if (!at_end() && peek() == '0') {
            ++offset_;
        } else if (auto digits = read_digits(); !digits) {
            return digits;
        }
        if (!at_end() && peek() == '.') {
            ++offset_;
            if (auto digits = read_digits(); !digits) {
                return digits;
            }
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            ++offset_;
            if (!at_end() && (peek() == '+' || peek() == '-')) {
                ++offset_;
            }
            if (auto digits = read_digits(); !digits) {
                return digits;
            }
        }
        // Anything that could only carry the number on makes it one the grammar doesn't allow,
        // such as `01` or `1.5.3`, rather than a number followed by something else.
        if (!at_end()) {
            const char c = peek();
            if (is_digit(c) || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                return fail(SyntaxErrorCode::invalid_number);
            }
        }
        storage_->nodes.push_back(detail::Node{
            .kind = Kind::number, .offset = first, .first = first, .size = offset_ - first});
        return {};
    }

    /// Reads one digit or more.
    std::expected<void, SyntaxError> read_digits() {
        if (at_end()) {
            return fail(SyntaxErrorCode::unexpected_end);
        }
        if (!is_digit(peek())) {
            return fail(SyntaxErrorCode::invalid_number);
        }
        while (!at_end() && is_digit(peek())) {
            ++offset_;
        }
        return {};
    }

    /// Reads a string, from its opening quote to its closing one.
    ///
    /// A string without escapes stays in the text. One with escapes is decoded into the storage's
    /// decoded strings, the plain runs between its escapes copied as they are.
    std::expected<void, SyntaxError> read_string() {
        const std::size_t offset = offset_;
        ++offset_;
        const std::size_t decoded_first = storage_->decoded.size();
        bool escaped = false;
        // Where the characters not yet copied to the decoded strings begin.
        std::size_t run = offset_;
        while (true) {
            if (at_end()) {
                return fail(SyntaxErrorCode::unterminated_string);
            }
            const char c = peek();
            if (c == '"') {
                break;
            }
            if (c == '\\') {
                escaped = true;
                storage_->decoded.append(text_.substr(run, offset_ - run));
                ++offset_;
                if (auto escape = read_escape(); !escape) {
                    return escape;
                }
                run = offset_;
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return fail(SyntaxErrorCode::control_character_in_string);
            } else {
                ++offset_;
            }
        }

        detail::Node node{.kind = Kind::string, .escaped = escaped, .offset = offset};
        if (escaped) {
            storage_->decoded.append(text_.substr(run, offset_ - run));
            node.first = decoded_first;
            node.size = storage_->decoded.size() - decoded_first;
        } else {
            node.first = offset + 1;
            node.size = offset_ - node.first;
        }
        ++offset_;
        storage_->nodes.push_back(node);
        return {};
    }

    /// Reads an escape after its backslash, decoding it onto the decoded strings.
    std::expected<void, SyntaxError> read_escape() {
        if (at_end()) {
            return fail(SyntaxErrorCode::unterminated_string);
        }
        char character = 0;
        switch (peek()) {
        case '"':
        case '\\':
        case '/':
            character = peek();
            break;
        case 'b':
            character = '\b';
            break;
        case 'f':
            character = '\f';
            break;
        case 'n':
            character = '\n';
            break;
        case 'r':
            character = '\r';
            break;
        case 't':
            character = '\t';
            break;
        case 'u':
            ++offset_;
            return read_unicode_escape();
        default:
            return fail(SyntaxErrorCode::invalid_escape);
        }
        ++offset_;
        storage_->decoded.push_back(character);
        return {};
    }

    /// Reads the four hexadecimal digits of a `\u` escape, and the escape after it when the two
    /// are a surrogate pair, decoding the code point they stand for onto the decoded strings.
    std::expected<void, SyntaxError> read_unicode_escape() {
        std::uint32_t code_point = 0;
        for (std::size_t digit = 0; digit < 4; ++digit) {
            if (at_end()) {
                return fail(SyntaxErrorCode::unterminated_string);
            }
            const std::optional<std::uint32_t> value = hex_digit(peek());
            if (!value) {
                return fail(SyntaxErrorCode::invalid_escape);
            }
            code_point = (code_point << 4U) | *value;
            ++offset_;
        }
        if (is_high_surrogate(code_point) && text_.substr(offset_, 2) == "\\u") {
            const std::optional<std::uint32_t> low = hex_number(text_.substr(offset_ + 2, 4));
            if (low && is_low_surrogate(*low)) {
                code_point = 0x10000 + ((code_point - 0xD800) << 10U) + (*low - 0xDC00);
                offset_ += 6;
            }
        }
        append_utf8(code_point);
        return {};
    }

    [[nodiscard]] static std::optional<std::uint32_t> hex_digit(char c) noexcept {
        if (c >= '0' && c <= '9') {
            return static_cast<std::uint32_t>(c - '0');
        }
        if (c >= 'a' && c <= 'f') {
            return static_cast<std::uint32_t>(c - 'a' + 10);
        }
        if (c >= 'A' && c <= 'F') {
            return static_cast<std::uint32_t>(c - 'A' + 10);
        }
        return std::nullopt;
    }

    /// The value of exactly four hexadecimal digits.
    [[nodiscard]] static std::optional<std::uint32_t> hex_number(std::string_view digits) noexcept {
        if (digits.size() != 4) {
            return std::nullopt;
        }
        std::uint32_t number = 0;
        for (const char c : digits) {
            const std::optional<std::uint32_t> value = hex_digit(c);
            if (!value) {
                return std::nullopt;
            }
            number = (number << 4U) | *value;
        }
        return number;
    }

    [[nodiscard]] static bool is_high_surrogate(std::uint32_t unit) noexcept {
        return unit >= 0xD800 && unit <= 0xDBFF;
    }

    [[nodiscard]] static bool is_low_surrogate(std::uint32_t unit) noexcept {
        return unit >= 0xDC00 && unit <= 0xDFFF;
    }

    /// Writes a code point onto the decoded strings in UTF-8.
    void append_utf8(std::uint32_t code_point) {
        std::string& out = storage_->decoded;
        if (code_point < 0x80) {
            out.push_back(static_cast<char>(code_point));
        } else if (code_point < 0x800) {
            out.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
            out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        } else if (code_point < 0x10000) {
            out.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
            out.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        } else {
            out.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
            out.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        }
    }

    /// Reads the letters of `true`, `false` or `null`.
    std::expected<void, SyntaxError> read_word(std::string_view word) {
        for (const char letter : word) {
            if (at_end()) {
                return fail(SyntaxErrorCode::unexpected_end);
            }
            if (peek() != letter) {
                return fail(SyntaxErrorCode::unexpected_character);
            }
            ++offset_;
        }
        return {};
    }

    /// An array or an object still open at the current offset.
    struct Frame {
        /// Its node.
        std::size_t node;

        /// Whether it is an array or an object.
        Kind kind;

        /// Where its children begin in `pending_`.
        std::size_t first_pending;
    };

    std::unique_ptr<detail::Storage> storage_;
    std::string_view text_;

    /// The offset of the next byte to read.
    std::size_t offset_ = 0;

    /// The arrays and objects open at the current offset, the innermost last.
    std::vector<Frame> frames_;

    /// The children read so far of every open array and object, those of the innermost last. An
    /// object's are its keys and values in turn.
    std::vector<std::size_t> pending_;
};

} // namespace

std::expected<Document, SyntaxError> parse(std::string text, ParseOptions /*options*/) {
    auto storage = Parser{std::move(text)}.run();
    if (!storage) {
        return std::unexpected(storage.error());
    }
    return Document{std::move(*storage)};
}

} // namespace pedantjson
