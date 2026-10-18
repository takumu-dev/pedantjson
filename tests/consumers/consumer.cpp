// A program that uses pedantjson as any project would, through its one header.

#include <pedantjson/pedantjson.hpp>

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
    return 0;
}
