#include "support.hpp"
#include <pedantjson/pedantjson.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
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

/// The outcome of a text that is read as JSON.
constexpr std::optional<SyntaxError> parses = std::nullopt;

/// The outcome of a text that is rejected with the syntax error of `code` at `offset`.
constexpr std::optional<SyntaxError> rejected(SyntaxErrorCode code, std::size_t offset) {
    return SyntaxError{.code = code, .offset = offset};
}

using enum SyntaxErrorCode;

/// A row for every `i_` file in the suite.
constexpr std::array decisions{
    // RFC 8259 lets a parser limit the range and precision of numbers. pedantjson keeps a number's
    // text instead, and a number too large or too precise for a type fails when it is read into
    // that type, so the text is JSON whatever its size.
    Decision{"i_number_double_huge_neg_exp.json", parses, "a number keeps its text, however small"},
    Decision{"i_number_huge_exp.json", parses, "a number keeps its text, however large"},
    Decision{"i_number_neg_int_huge_exp.json", parses, "a number keeps its text, however large"},
    Decision{"i_number_pos_double_huge_exp.json", parses, "a number keeps its text, however large"},
    Decision{"i_number_real_neg_overflow.json", parses, "a number keeps its text, however large"},
    Decision{"i_number_real_pos_overflow.json", parses, "a number keeps its text, however large"},
    Decision{"i_number_real_underflow.json", parses, "a number keeps its text, however small"},
    Decision{"i_number_too_big_neg_int.json", parses, "a number keeps its text, however long"},
    Decision{"i_number_too_big_pos_int.json", parses, "a number keeps its text, however long"},
    Decision{"i_number_very_big_negative_int.json", parses,
             "a number keeps its text, however long"},

    // A surrogate escape stands for half a character, and only a high one followed at once by a
    // low one makes a whole character.
    Decision{"i_object_key_lone_2nd_surrogate.json", rejected(unpaired_surrogate, 2),
             "a key is a string, and its low surrogate escape has no high one before it"},
    Decision{"i_string_1st_surrogate_but_2nd_missing.json", rejected(unpaired_surrogate, 2),
             "a high surrogate escape ends the string"},
    Decision{"i_string_1st_valid_surrogate_2nd_invalid.json", rejected(unpaired_surrogate, 2),
             "a high surrogate escape is followed by an escape that isn't a low surrogate"},
    Decision{"i_string_incomplete_surrogate_and_escape_valid.json", rejected(unpaired_surrogate, 2),
             "a high surrogate escape is followed by a short escape"},
    Decision{"i_string_incomplete_surrogate_pair.json", rejected(unpaired_surrogate, 2),
             "a low surrogate escape has no high one before it"},
    Decision{"i_string_incomplete_surrogates_escape_valid.json", rejected(unpaired_surrogate, 2),
             "a high surrogate escape is followed by another high one"},
    Decision{"i_string_invalid_lonely_surrogate.json", rejected(unpaired_surrogate, 2),
             "a high surrogate escape ends the string"},
    Decision{"i_string_invalid_surrogate.json", rejected(unpaired_surrogate, 2),
             "a high surrogate escape is followed by plain characters"},
    Decision{"i_string_inverted_surrogates_U+1D11E.json", rejected(unpaired_surrogate, 2),
             "a pair's two escapes come in the wrong order"},
    Decision{"i_string_lone_second_surrogate.json", rejected(unpaired_surrogate, 2),
             "a low surrogate escape has no high one before it"},

    // RFC 8259 asks for UTF-8, and anything else is read as bytes that aren't UTF-8.
    Decision{"i_string_invalid_utf-8.json", rejected(invalid_utf8, 2),
             "the byte 0xFF is never UTF-8"},
    Decision{"i_string_iso_latin_1.json", rejected(invalid_utf8, 2),
             "an ISO 8859-1 letter is a UTF-8 lead byte without its continuation"},
    Decision{"i_string_lone_utf8_continuation_byte.json", rejected(invalid_utf8, 2),
             "a continuation byte has no lead byte before it"},
    Decision{"i_string_not_in_unicode_range.json", rejected(invalid_utf8, 2),
             "the sequence stands for a code point beyond U+10FFFF"},
    Decision{"i_string_overlong_sequence_2_bytes.json", rejected(invalid_utf8, 2),
             "a character is written in more bytes than it needs"},
    Decision{"i_string_overlong_sequence_6_bytes.json", rejected(invalid_utf8, 2),
             "UTF-8 no longer has six-byte sequences"},
    Decision{"i_string_overlong_sequence_6_bytes_null.json", rejected(invalid_utf8, 2),
             "UTF-8 no longer has six-byte sequences"},
    Decision{"i_string_truncated-utf-8.json", rejected(invalid_utf8, 2),
             "a lead byte is followed by a byte that can't continue it"},
    Decision{"i_string_UTF-8_invalid_sequence.json", rejected(invalid_utf8, 7),
             "UTF-8 no longer has the five-byte sequence that 0xFA would begin"},
    Decision{"i_string_UTF8_surrogate_U+D800.json", rejected(invalid_utf8, 2),
             "a surrogate is never encoded in UTF-8"},
    Decision{"i_string_UTF-16LE_with_BOM.json", rejected(invalid_utf8, 0),
             "UTF-16 isn't read, and its byte-order mark isn't UTF-8"},
    Decision{"i_string_utf16BE_no_BOM.json", rejected(unexpected_character, 0),
             "UTF-16 isn't read, and the U+0000 it begins with can't begin a value"},
    Decision{"i_string_utf16LE_no_BOM.json", rejected(unexpected_character, 1),
             "UTF-16 isn't read, and U+0000 can't follow an opening bracket"},

    Decision{"i_structure_500_nested_arrays.json", rejected(too_deep, 256),
             "arrays 500 deep are past the default limit of 256"},
    Decision{"i_structure_UTF-8_BOM_empty_object.json", parses,
             "a byte-order mark at the start of the text is ignored"},
};

/// The directory of the suite's files.
const std::filesystem::path suite = PEDANTJSON_TEST_SUITE_DIR;

/// The names of the suite's files, in order.
std::vector<std::string> suite_files() {
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
    std::ifstream file{suite / name, std::ios::binary};
    INFO("reading " << name);
    REQUIRE(file.is_open());
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

/// The row of the table for a file, if it has one.
std::optional<Decision> decision_for(std::string_view file) {
    const auto row = std::ranges::find(decisions, file, &Decision::file);
    if (row == decisions.end()) {
        return std::nullopt;
    }
    return *row;
}

} // namespace

TEST_CASE("the suite is whole, each file to be read, rejected or decided", "[JSONTestSuite]") {
    const std::vector<std::string> files = suite_files();
    for (const std::string& name : files) {
        CAPTURE(name);
        CHECK(name.ends_with(".json"));
        CHECK((name.starts_with("y_") || name.starts_with("n_") || name.starts_with("i_")));
    }
    CHECK(files.size() == 318);
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
    const std::optional<Decision> decision = decision_for(name);
    REQUIRE(decision.has_value());
    CAPTURE(decision->reason);
    if (decision->error) {
        const auto error = support::syntax_error(read(name));
        CHECK(error.code == decision->error->code);
        CHECK(error.offset == decision->error->offset);
    } else {
        support::parsed(read(name));
    }
}

TEST_CASE("the table names each file whose outcome the suite leaves open, once, and nothing else",
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
    for (const std::string_view file : in_suite) {
        CAPTURE(file);
        CHECK(in_table.contains(file));
    }
}
