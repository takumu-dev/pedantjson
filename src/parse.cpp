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

/// U+FEFF in UTF-8, which a text may begin with.
constexpr std::string_view byte_order_mark = "\xef\xbb\xbf";

/// The length of the well-formed UTF-8 sequence that begins at `offset`, as Table 3-7 of the
/// Unicode Standard defines one.
///
/// \param text The text.
/// \param offset Where the sequence begins, which must be before the end of the text.
/// \return Its length in bytes, or 0 if the bytes there aren't UTF-8.
std::size_t utf8_sequence_length(std::string_view text, std::size_t offset) {
    // The precondition, and the check of the length below, keep every byte read within the text.
    const auto byte = [text](std::size_t at) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return static_cast<unsigned char>(text[at]);
    };
    const unsigned char lead = byte(offset);
    if (lead < 0x80) {
        return 1;
    }
    // The lead byte gives the length, and the range of the byte after it. That range is narrower
    // than a continuation byte's for the leads whose sequences could otherwise encode a code point
    // in more bytes than it needs, a surrogate, or one beyond U+10FFFF.
    std::size_t length = 0;
    unsigned char low = 0x80;
    unsigned char high = 0xbf;
    if (lead >= 0xc2 && lead <= 0xdf) {
        length = 2;
    } else if (lead == 0xe0) {
        length = 3;
        low = 0xa0;
    } else if (lead == 0xed) {
        length = 3;
        high = 0x9f;
    } else if (lead >= 0xe1 && lead <= 0xef) {
        length = 3;
    } else if (lead == 0xf0) {
        length = 4;
        low = 0x90;
    } else if (lead == 0xf4) {
        length = 4;
        high = 0x8f;
    } else if (lead >= 0xf1 && lead <= 0xf3) {
        length = 4;
    } else {
        return 0;
    }
    if (text.size() - offset < length) {
        return 0;
    }
    for (std::size_t next = 1; next < length; ++next) {
        const unsigned char continuation = byte(offset + next);
        if (continuation < low || continuation > high) {
            return 0;
        }
        low = 0x80;
        high = 0xbf;
    }
    return length;
}

/// Reads one text into the storage of a document.
class Parser {
  public:
    Parser(std::string text, ParseOptions options)
        : storage_(std::make_unique<detail::Storage>()), options_(options) {
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
        // A byte-order mark says nothing in UTF-8, the only encoding read, but editors write one.
        if (text_.starts_with(byte_order_mark)) {
            offset_ = byte_order_mark.size();
        }
        storage_->line_starts.push_back(offset_);
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
        return fail_at(code, offset_);
    }

    /// The error of the given kind at `offset`, or `invalid_utf8` if the bytes there aren't UTF-8,
    /// since that is the first thing wrong with them whatever the grammar expected.
    [[nodiscard]] std::unexpected<SyntaxError> fail_at(SyntaxErrorCode code,
                                                       std::size_t offset) const noexcept {
        if (offset < text_.size() && utf8_sequence_length(text_, offset) == 0) {
            code = SyntaxErrorCode::invalid_utf8;
        }
        return std::unexpected(
            SyntaxError{.code = code, .offset = offset, .position = storage_->position(offset)});
    }

    /// Skips whitespace, noting where each line after it begins. Only whitespace can end a line,
    /// since a string can't hold a line feed or a carriage return unescaped.
    void skip_whitespace() {
        while (!at_end()) {
            const char c = peek();
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                return;
            }
            ++offset_;
            // A carriage return and a line feed together end one line.
            if (c == '\n' || (c == '\r' && (at_end() || peek() != '\n'))) {
                storage_->line_starts.push_back(offset_);
            }
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
        if (auto opened = open(kind); !opened) {
            return std::unexpected(opened.error());
        }
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

    /// Opens an array or an object at its bracket, unless it is nested too deeply.
    std::expected<void, SyntaxError> open(Kind kind) {
        if (frames_.size() == options_.max_depth) {
            return fail(SyntaxErrorCode::too_deep);
        }
        frames_.push_back(
            Frame{.node = storage_->nodes.size(), .kind = kind, .first_pending = pending_.size()});
        add_node(kind);
        ++offset_;
        return {};
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
            .kind = Kind::number,
            .offset = first,
            .first = first,
            .size = offset_ - first,
        });
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
        // The high surrogate escape just read, which the next escape has to pair.
        std::optional<Escape> high;
        while (true) {
            if (at_end()) {
                return fail(SyntaxErrorCode::unterminated_string);
            }
            const char c = peek();
            if (c == '\\') {
                escaped = true;
                storage_->decoded.append(text_.substr(run, offset_ - run));
                const std::size_t escape = offset_;
                ++offset_;
                const auto unit = read_escape();
                if (!unit) {
                    return std::unexpected(unit.error());
                }
                if (auto appended = append_escape({.unit = *unit, .offset = escape}, high);
                    !appended) {
                    return appended;
                }
                run = offset_;
                continue;
            }
            // Whatever follows a high surrogate escape is read by its own rules first, as an
            // escape is, and only then found not to be the low surrogate escape it needs.
            std::size_t length = 1;
            if (c != '"') {
                if (static_cast<unsigned char>(c) < 0x20) {
                    return fail(SyntaxErrorCode::control_character_in_string);
                }
                length = utf8_sequence_length(text_, offset_);
                if (length == 0) {
                    return fail(SyntaxErrorCode::invalid_utf8);
                }
            }
            if (high) {
                return fail_at(SyntaxErrorCode::unpaired_surrogate, high->offset);
            }
            if (c == '"') {
                break;
            }
            offset_ += length;
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

    /// An escape in a string, read.
    struct Escape {
        /// The UTF-16 code unit it stands for.
        std::uint32_t unit;

        /// Where it begins: its backslash.
        std::size_t offset;
    };

    /// Reads an escape after its backslash.
    ///
    /// \return The UTF-16 code unit it stands for.
    std::expected<std::uint32_t, SyntaxError> read_escape() {
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
            return read_hex_digits();
        default:
            return fail(SyntaxErrorCode::invalid_escape);
        }
        ++offset_;
        return static_cast<std::uint32_t>(character);
    }

    /// Reads the four hexadecimal digits of a `\u` escape.
    ///
    /// \return The code unit they give.
    std::expected<std::uint32_t, SyntaxError> read_hex_digits() {
        std::uint32_t unit = 0;
        for (std::size_t digit = 0; digit < 4; ++digit) {
            if (at_end()) {
                return fail(SyntaxErrorCode::unterminated_string);
            }
            const std::optional<std::uint32_t> value = hex_digit(peek());
            if (!value) {
                return fail(SyntaxErrorCode::invalid_escape);
            }
            unit = (unit << 4U) | *value;
            ++offset_;
        }
        return unit;
    }

    /// Writes the code unit of an escape onto the decoded strings in UTF-8, pairing a high
    /// surrogate with the low one after it into the one code point they stand for together.
    ///
    /// \param escape The escape.
    /// \param high The high surrogate escape before it that waits for its pair, if any. A high
    ///             surrogate is left here to wait, and a completed pair clears it.
    std::expected<void, SyntaxError> append_escape(Escape escape, std::optional<Escape>& high) {
        if (high) {
            if (!is_low_surrogate(escape.unit)) {
                return fail_at(SyntaxErrorCode::unpaired_surrogate, high->offset);
            }
            append_utf8(0x10000 + ((high->unit - 0xD800) << 10U) + (escape.unit - 0xDC00));
            high.reset();
        } else if (is_high_surrogate(escape.unit)) {
            high = escape;
        } else if (is_low_surrogate(escape.unit)) {
            return fail_at(SyntaxErrorCode::unpaired_surrogate, escape.offset);
        } else {
            append_utf8(escape.unit);
        }
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
    ParseOptions options_;

    /// The offset of the next byte to read.
    std::size_t offset_ = 0;

    /// The arrays and objects open at the current offset, the innermost last.
    std::vector<Frame> frames_;

    /// The children read so far of every open array and object, those of the innermost last. An
    /// object's are its keys and values in turn.
    std::vector<std::size_t> pending_;
};

} // namespace

std::expected<Document, SyntaxError> parse(std::string text, ParseOptions options) {
    auto storage = Parser{std::move(text), options}.run();
    if (!storage) {
        return std::unexpected(storage.error());
    }
    return Document{std::move(*storage)};
}

} // namespace pedantjson
