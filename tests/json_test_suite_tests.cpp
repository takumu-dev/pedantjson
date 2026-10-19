#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using pedantjson::SyntaxError;
using pedantjson::SyntaxErrorCode;

namespace {

/// What pedantjson does with a text whose outcome RFC 8259 leaves to the parser.
struct Decision {
    /// The name of the file in the suite that holds the text.
    std::string_view file;

    /// The syntax error the text gives, or nothing if it is read as JSON.
    std::optional<SyntaxError> error;

    /// Why, in one line.
    std::string_view reason;
};

/// The row for a text that is read as JSON.
constexpr Decision parses(std::string_view file, std::string_view reason) {
    return {.file = file, .error = std::nullopt, .reason = reason};
}

/// The row for a text that is rejected with the syntax error of `code` at `offset`, whatever its
/// position.
constexpr Decision rejected(std::string_view file, SyntaxErrorCode code, std::size_t offset,
                            std::string_view reason) {
    return {
        .file = file,
        .error = SyntaxError{.code = code, .offset = offset, .position = {}},
        .reason = reason,
    };
}

using enum SyntaxErrorCode;

/// A row for every `i_` file in the suite.
constexpr std::array decisions{
    // RFC 8259 lets a parser limit the range and precision of numbers. pedantjson keeps a number's
    // text instead, and a number too large or too precise for a type fails when it is read into
    // that type, so the text is JSON whatever its size.
    parses("i_number_double_huge_neg_exp.json", "a number keeps its text, however small"),
    parses("i_number_huge_exp.json", "a number keeps its text, however large"),
    parses("i_number_neg_int_huge_exp.json", "a number keeps its text, however large"),
    parses("i_number_pos_double_huge_exp.json", "a number keeps its text, however large"),
    parses("i_number_real_neg_overflow.json", "a number keeps its text, however large"),
    parses("i_number_real_pos_overflow.json", "a number keeps its text, however large"),
    parses("i_number_real_underflow.json", "a number keeps its text, however small"),
    parses("i_number_too_big_neg_int.json", "a number keeps its text, however long"),
    parses("i_number_too_big_pos_int.json", "a number keeps its text, however long"),
    parses("i_number_very_big_negative_int.json", "a number keeps its text, however long"),

    // A surrogate escape stands for half a character, and only a high one followed at once by a
    // low one makes a whole character.
    rejected("i_object_key_lone_2nd_surrogate.json", unpaired_surrogate, 2,
             "a key is a string, and its low surrogate escape has no high one before it"),
    rejected("i_string_1st_surrogate_but_2nd_missing.json", unpaired_surrogate, 2,
             "a high surrogate escape ends the string"),
    rejected("i_string_1st_valid_surrogate_2nd_invalid.json", unpaired_surrogate, 2,
             "a high surrogate escape is followed by an escape that isn't a low surrogate"),
    rejected("i_string_incomplete_surrogate_and_escape_valid.json", unpaired_surrogate, 2,
             "a high surrogate escape is followed by a short escape"),
    rejected("i_string_incomplete_surrogate_pair.json", unpaired_surrogate, 2,
             "a low surrogate escape has no high one before it"),
    rejected("i_string_incomplete_surrogates_escape_valid.json", unpaired_surrogate, 2,
             "a high surrogate escape is followed by another high one"),
    rejected("i_string_invalid_lonely_surrogate.json", unpaired_surrogate, 2,
             "a high surrogate escape ends the string"),
    rejected("i_string_invalid_surrogate.json", unpaired_surrogate, 2,
             "a high surrogate escape is followed by plain characters"),
    rejected("i_string_inverted_surrogates_U+1D11E.json", unpaired_surrogate, 2,
             "a pair's two escapes come in the wrong order"),
    rejected("i_string_lone_second_surrogate.json", unpaired_surrogate, 2,
             "a low surrogate escape has no high one before it"),

    // RFC 8259 asks for UTF-8, and anything else is read as bytes that aren't UTF-8.
    rejected("i_string_invalid_utf-8.json", invalid_utf8, 2, "the byte 0xFF is never UTF-8"),
    rejected("i_string_iso_latin_1.json", invalid_utf8, 2,
             "an ISO 8859-1 letter is a UTF-8 lead byte without its continuation"),
    rejected("i_string_lone_utf8_continuation_byte.json", invalid_utf8, 2,
             "a continuation byte has no lead byte before it"),
    rejected("i_string_not_in_unicode_range.json", invalid_utf8, 2,
             "the sequence stands for a code point beyond U+10FFFF"),
    rejected("i_string_overlong_sequence_2_bytes.json", invalid_utf8, 2,
             "a character is written in more bytes than it needs"),
    rejected("i_string_overlong_sequence_6_bytes.json", invalid_utf8, 2,
             "UTF-8 no longer has six-byte sequences"),
    rejected("i_string_overlong_sequence_6_bytes_null.json", invalid_utf8, 2,
             "UTF-8 no longer has six-byte sequences"),
    rejected("i_string_truncated-utf-8.json", invalid_utf8, 2,
             "a lead byte is followed by a byte that can't continue it"),
    rejected("i_string_UTF-8_invalid_sequence.json", invalid_utf8, 7,
             "UTF-8 no longer has the five-byte sequence that 0xFA would begin"),
    rejected("i_string_UTF8_surrogate_U+D800.json", invalid_utf8, 2,
             "a surrogate is never encoded in UTF-8"),
    rejected("i_string_UTF-16LE_with_BOM.json", invalid_utf8, 0,
             "UTF-16 isn't read, and its byte-order mark isn't UTF-8"),
    rejected("i_string_utf16BE_no_BOM.json", unexpected_character, 0,
             "UTF-16 isn't read, and the U+0000 it begins with can't begin a value"),
    rejected("i_string_utf16LE_no_BOM.json", unexpected_character, 1,
             "UTF-16 isn't read, and U+0000 can't follow an opening bracket"),

    rejected("i_structure_500_nested_arrays.json", too_deep, 256,
             "arrays 500 deep are past the default limit of 256"),
    parses("i_structure_UTF-8_BOM_empty_object.json",
           "a byte-order mark at the start of the text is ignored"),
};

/// The directory of the suite's files.
std::filesystem::path suite_directory() { return PEDANTJSON_TEST_SUITE_DIR; }

/// The names of the suite's files, in order.
std::vector<std::string> suite_files() {
    const std::filesystem::path suite = suite_directory();
    std::vector<std::string> names;
    std::error_code error;
    for (std::filesystem::directory_iterator entry{suite, error}, end; !error && entry != end;
         entry.increment(error)) {
        names.push_back(entry->path().filename().string());
    }
    INFO("reading " << suite.string() << ": " << error.message());
    REQUIRE_FALSE(error);
    std::ranges::sort(names);
    return names;
}

/// The names of the suite's files whose names begin with `prefix`, in order.
std::vector<std::string> suite_files(std::string_view prefix) {
    std::vector<std::string> names = suite_files();
    std::erase_if(names, [prefix](const std::string& name) { return !name.starts_with(prefix); });
    REQUIRE_FALSE(names.empty());
    return names;
}

/// The bytes of one of the suite's files.
std::string read(const std::string& name) {
    std::ifstream file{suite_directory() / name, std::ios::binary};
    INFO("reading " << name);
    REQUIRE(file.is_open());
    std::ostringstream bytes;
    bytes << file.rdbuf();
    return std::move(bytes).str();
}

/// The row of the table for a file, if it has one.
std::optional<Decision> decision_for(std::string_view file) {
    for (const Decision& decision : decisions) {
        if (decision.file == file) {
            return decision;
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("each file of the suite is to be read, rejected or decided", "[JSONTestSuite]") {
    for (const std::string& name : suite_files()) {
        CAPTURE(name);
        CHECK(name.ends_with(".json"));
        CHECK((name.starts_with("y_") || name.starts_with("n_") || name.starts_with("i_")));
    }
}

TEST_CASE("the suite is whole", "[JSONTestSuite]") {
    CHECK(suite_files().size() == 318);
    CHECK(suite_files("y_").size() == 95);
    CHECK(suite_files("n_").size() == 188);
    CHECK(suite_files("i_").size() == 35);
}

TEST_CASE("every text the suite says is JSON is read", "[JSONTestSuite]") {
    const auto name = GENERATE(from_range(suite_files("y_")));
    CAPTURE(name);
    support::parsed(read(name));
}

TEST_CASE("every text the suite says isn't JSON is a syntax error", "[JSONTestSuite]") {
    const auto name = GENERATE(from_range(suite_files("n_")));
    CAPTURE(name);
    // The suite says which texts aren't JSON but not what is wrong with them, so there is no code
    // or offset to check against.
    support::syntax_error(read(name));
}

TEST_CASE("every text whose outcome the suite leaves open has the outcome the table gives it",
          "[JSONTestSuite]") {
    const auto name = GENERATE(from_range(suite_files("i_")));
    CAPTURE(name);
    const Decision decision = support::required(decision_for(name));
    CAPTURE(decision.reason);
    if (decision.error) {
        const auto error = support::syntax_error(read(name));
        CHECK(error.code == decision.error->code);
        CHECK(error.offset == decision.error->offset);
    } else {
        support::parsed(read(name));
    }
}

TEST_CASE("the table names files whose outcome the suite leaves open, each once, with a reason",
          "[JSONTestSuite]") {
    const std::vector<std::string> files = suite_files("i_");
    const std::set<std::string_view> in_suite{files.begin(), files.end()};
    std::set<std::string_view> in_table;
    for (const Decision& decision : decisions) {
        CAPTURE(decision.file);
        CHECK(in_table.insert(decision.file).second);
        CHECK(in_suite.contains(decision.file));
        CHECK_FALSE(decision.reason.empty());
    }
}

TEST_CASE("the table names every file whose outcome the suite leaves open", "[JSONTestSuite]") {
    for (const std::string& file : suite_files("i_")) {
        CAPTURE(file);
        CHECK(decision_for(file).has_value());
    }
}
