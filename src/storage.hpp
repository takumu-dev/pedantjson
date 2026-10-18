#pragma once

#include <pedantjson/pedantjson.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace pedantjson::detail {

/// One key or value of a document.
struct Node {
    /// What kind of value it is. A key is a string.
    Kind kind = Kind::null;

    /// For a boolean, whether it is `true`.
    bool is_true = false;

    /// For a string, whether it had escapes, so that its characters are in `Storage::decoded`
    /// rather than in the text.
    bool escaped = false;

    /// Where it begins in the text.
    std::size_t offset = 0;

    /// For a number or a string, where its characters begin, in the text or in the decoded
    /// strings. For an array or an object, where its children begin in `Storage::children`.
    std::size_t first = 0;

    /// For a number or a string, the length of its characters in bytes. For an array, how many
    /// elements it has; for an object, how many members.
    std::size_t size = 0;
};

/// A document's text and its nodes, shared by every handle into the document.
struct Storage {
    /// The text the document was read from.
    std::string text;

    /// Every string that had escapes, decoded, one after another.
    std::string decoded;

    /// Every key and value, in the order of the text. The first is the root.
    std::vector<Node> nodes;

    /// The indices into `nodes` of each array's elements, and of each object's keys and values in
    /// turn, contiguous for each array and object.
    std::vector<std::size_t> children;

    // Every index into the nodes and the children comes from the parser, which made what it names.

    /// The node with index `index`.
    [[nodiscard]] Node& node(std::size_t index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return nodes[index];
    }

    /// The node with index `index`.
    [[nodiscard]] const Node& node(std::size_t index) const {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return nodes[index];
    }

    /// The index into `nodes` of the child with index `index` in `children`.
    [[nodiscard]] std::size_t child(std::size_t index) const {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): see above
        return children[index];
    }

    /// The characters of a number or a string.
    [[nodiscard]] std::string_view characters(const Node& node) const noexcept {
        const std::string_view source = node.escaped ? decoded : text;
        return source.substr(node.first, node.size);
    }
};

} // namespace pedantjson::detail
