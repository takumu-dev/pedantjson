// A program that uses pedantjson as any project would, through its one header.

#include <pedantjson/pedantjson.hpp>

#include <optional>

namespace {

// Whether a number reads as a value of one type, through the library's code for that type.
template <pedantjson::NumberType T> bool reads_as(const pedantjson::Number& number, T expected) {
    const auto value = number.to<T>();
    return value && *value == expected;
}

} // namespace

int main() {
    const auto document = pedantjson::parse(R"({"name": "consumer", "tags": ["strict", "json"]})");
    if (!document) {
        return 1;
    }
    const auto object = document->root().as_object();
    if (!object) {
        return 1;
    }
    const auto tags = object->find("tags");
    if (!tags || !tags->value.as_array() || tags->value.as_array()->size() != 2) {
        return 1;
    }

    const auto numbers = pedantjson::parse("42");
    const auto number = numbers ? numbers->root().as_number() : std::nullopt;
    if (!number || !reads_as<signed char>(*number, 42) || !reads_as<short>(*number, 42) ||
        !reads_as<int>(*number, 42) || !reads_as<long>(*number, 42) ||
        !reads_as<long long>(*number, 42) || !reads_as<unsigned char>(*number, 42) ||
        !reads_as<unsigned short>(*number, 42) || !reads_as<unsigned int>(*number, 42U) ||
        !reads_as<unsigned long>(*number, 42UL) || !reads_as<unsigned long long>(*number, 42ULL) ||
        !reads_as<float>(*number, 42.0F) || !reads_as<double>(*number, 42.0)) {
        return 1;
    }
    return 0;
}
