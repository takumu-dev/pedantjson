#pragma once

#include <cstddef>
#include <string_view>

namespace pedantjson::detail {

/// The length of the well-formed UTF-8 sequence that begins at `offset`, as Table 3-7 of the
/// Unicode Standard defines one.
///
/// \param text The text.
/// \param offset Where the sequence begins, which must be before the end of the text.
/// \return Its length in bytes, or 0 if the bytes there aren't UTF-8.
inline std::size_t utf8_sequence_length(std::string_view text, std::size_t offset) {
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

} // namespace pedantjson::detail
