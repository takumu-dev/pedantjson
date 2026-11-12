#include <pedantjson/pedantjson.hpp>

#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <forward_list>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace pedantjson {

namespace {

/// The shortest text that reads back as the same value of a number's type.
///
/// \tparam T The number's type.
/// \param value The number, which must be finite if it is a `float` or a `double`.
/// \return Its text.
template <typename T> std::string text_of(T value) {
    // Longer than any integer of 64 bits, sign included, and than the shortest form of any
    // double, such as -2.2250738585072014e-308.
    std::array<char, 32> characters{};
    const auto written = std::to_chars(characters.data(), std::to_address(characters.end()), value);
    // NOLINTNEXTLINE(misc-static-assert): the condition depends on the argument
    assert(written.ec == std::errc{} && "the array holds any number's text");
    return {characters.data(), written.ptr};
}

} // namespace

Draft::Draft(signed char value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(short value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(int value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(long value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(long long value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(unsigned char value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(unsigned short value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(unsigned int value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(unsigned long value) : kind_(Kind::number), characters_(text_of(value)) {}

Draft::Draft(unsigned long long value) : kind_(Kind::number), characters_(text_of(value)) {}

// A number that isn't finite has no text, and is refused when the draft is written.

Draft::Draft(float value)
    : kind_(Kind::number), characters_(std::isfinite(value) ? text_of(value) : std::string{}) {}

Draft::Draft(double value)
    : kind_(Kind::number), characters_(std::isfinite(value) ? text_of(value) : std::string{}) {}

Draft::Draft(const Value& value) {
    // The copy keeps a stack of its own rather than recursing, as the parser and the writer do.
    // Each array and object gets all its children before any of them is filled, so that the
    // pointers to them stay valid.
    struct Pending {
        Value from;
        Draft* target;
    };

    std::vector<Pending> pending{{.from = value, .target = this}};
    while (!pending.empty()) {
        const auto [from, target] = pending.back();
        pending.pop_back();
        target->kind_ = from.kind();
        if (const auto boolean = from.as_bool()) {
            target->is_true_ = *boolean;
        } else if (const auto number = from.as_number()) {
            target->characters_ = number->text();
        } else if (const auto string = from.as_string()) {
            target->characters_ = *string;
        } else if (const auto array = from.as_array()) {
            target->children_.resize(array->size());
            auto child = target->children_.begin();
            for (const Value element : *array) {
                pending.push_back({.from = element, .target = &*child++});
            }
        } else if (const auto object = from.as_object()) {
            target->children_.resize(object->size());
            target->names_.reserve(object->size());
            auto child = target->children_.begin();
            for (const Member member : *object) {
                target->names_.emplace_back(member.key.name());
                pending.push_back({.from = member.value, .target = &*child++});
            }
        }
    }
}

Draft::Draft(const Draft& other) {
    // As the copy from a value does.
    struct Pending {
        const Draft* from;
        Draft* target;
    };

    std::vector<Pending> pending{{.from = &other, .target = this}};
    while (!pending.empty()) {
        const auto [from, target] = pending.back();
        pending.pop_back();
        target->kind_ = from->kind_;
        target->is_true_ = from->is_true_;
        target->characters_ = from->characters_;
        target->names_ = from->names_;
        target->children_.resize(from->children_.size());
        auto child = target->children_.begin();
        for (const Draft& element : from->children_) {
            pending.push_back({.from = &element, .target = &*child++});
        }
    }
}

Draft& Draft::operator=(const Draft& other) {
    if (this != &other) {
        *this = Draft{other};
    }
    return *this;
}

Draft::~Draft() {
    // Each draft's children are taken from it, as a whole vector, before any draft is destroyed,
    // so that no destructor runs inside another's, however deep the draft is. The vectors are
    // kept in a list, whose nodes stay where they are, and destroyed with it, every draft in them
    // by then without children.
    std::forward_list<std::vector<Draft>> taken;
    taken.push_front(std::move(children_));
    std::vector<std::vector<Draft>*> pending{&taken.front()};
    while (!pending.empty()) {
        std::vector<Draft>* drafts = pending.back();
        pending.pop_back();
        for (Draft& draft : *drafts) {
            if (!draft.children_.empty()) {
                taken.push_front(std::move(draft.children_));
                pending.push_back(&taken.front());
            }
        }
    }
}

Draft Draft::array() noexcept {
    Draft draft;
    draft.kind_ = Kind::array;
    return draft;
}

Draft Draft::object() noexcept {
    Draft draft;
    draft.kind_ = Kind::object;
    return draft;
}

Draft& Draft::append(Draft element) {
    // NOLINTNEXTLINE(misc-static-assert): the condition depends on the draft
    assert(kind_ == Kind::array && "elements are appended to an array");
    children_.push_back(std::move(element));
    return *this;
}

Draft& Draft::append(std::string name, Draft value) {
    // NOLINTNEXTLINE(misc-static-assert): the condition depends on the draft
    assert(kind_ == Kind::object && "members are appended to an object");
    names_.push_back(std::move(name));
    children_.push_back(std::move(value));
    return *this;
}

} // namespace pedantjson
